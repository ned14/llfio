/* A filing system handle
(C) 2017-2020 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Aug 2017


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
    (See accompanying file Licence.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../../../fs_handle.hpp"
#include "../../../stat.hpp"
#include "../../../utils.hpp"
#include "import.hpp"

#include <climits>  // for PATH_MAX

#ifdef __linux__
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

result<void> fs_handle::_fetch_inode() const noexcept
{
  stat_t s(nullptr);
  OUTCOME_TRYV(s.fill(_get_handle(), stat_t::want::dev | stat_t::want::ino));
  _devid = s.st_dev;
  _inode = s.st_ino;
  return success();
}

namespace detail
{
  result<path_handle> containing_directory(optional<std::reference_wrapper<filesystem::path>> out_filename, const handle &h, const fs_handle &fsh,
                                           deadline d) noexcept
  {
    std::chrono::steady_clock::time_point began_steady;
    std::chrono::system_clock::time_point end_utc;
    if(d)
    {
      if(d.steady)
      {
        began_steady = std::chrono::steady_clock::now();
      }
      else
      {
        end_utc = d.to_time_point();
      }
    }
    try
    {
      for(;;)
      {
        // Get current path for handle and open its containing dir
        OUTCOME_TRY(auto &&_currentpath, h.current_path());
        // If current path is empty, it's been deleted
        if(_currentpath.empty())
        {
#if defined(__linux__) && 0  // not the cause of the Travis failure
          if(h.is_directory())
          {
            /* Docker's mechanism for protecting /proc on Linux is bugged. For files,
            we simply must give up. For directories, we can hack a workaround.
            */
            fprintf(stderr, "llfio: Docker bug failure to retrieve parent path workaround is being employed\n");
            auto tempname = utils::random_string(32);
            int tffd = ::openat(h.native_handle().fd, tempname.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, 0x1b0 /*660*/);
            if(tffd >= 0)
            {
              auto untffd = make_scope_exit([&]() noexcept {
                ::unlinkat(h.native_handle().fd, tempname.c_str(), 0);
                ::close(tffd);
              });
              std::string ret(32769, '\0');
              auto *out = const_cast<char *>(ret.data());
              // Linux keeps a symlink at /proc/self/fd/n
              char in[64];
              snprintf(in, sizeof(in), "/proc/self/fd/%d", tffd);
              ssize_t len;
              if((len = readlink(in, out, 32768)) >= 0)
              {
                ret.resize(len);
                _currentpath = std::move(ret);
                _currentpath = _currentpath.parent_path();
              }
            }
          }
          if(_currentpath.empty())
#endif
          {
            return errc::no_such_file_or_directory;
          }
        }
        // Split the path into root and leafname
        path_view currentpath(_currentpath);
        path_view filename = currentpath.filename();
        currentpath = currentpath.remove_filename().without_trailing_separator();
        // Zero terminate the root path so it doesn't get copied later
        if(_currentpath.native()[currentpath.native_size()] == '/')
        {
          const_cast<filesystem::path::string_type &>(_currentpath.native())[currentpath.native_size()] = 0;
        }
        auto currentdirh_ = path_handle::path(currentpath);
        if(!currentdirh_)
        {
          continue;
        }
        path_handle currentdirh = std::move(currentdirh_.value());
        if((h.flags() & handle::flag::disable_safety_unlinks) != 0)
        {
          if(out_filename)
          {
            out_filename->get() = filename.path();
          }
          return success(std::move(currentdirh));
        }
        // stat the same file name, and compare dev and inode
        path_view::zero_terminated_rendered_path<> zpath(filename);
        struct stat s
        {
        };
        memset(&s, 0, sizeof(s));
        if(-1 == ::fstatat(currentdirh.native_handle().fd, zpath.c_str(), &s, AT_SYMLINK_NOFOLLOW))
        {
          continue;
        }
        // If the same, we know for a fact that this is the correct containing dir for now at least
        if(static_cast<fs_handle::dev_t>(s.st_dev) == fsh.st_dev() && s.st_ino == fsh.st_ino())
        {
          if(out_filename)
          {
            out_filename->get() = filename.path();
          }
          return success(std::move(currentdirh));
        }
        // Check timeout
        if(d)
        {
          if(d.steady)
          {
            if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds(d.nsecs)))
            {
              return errc::timed_out;
            }
          }
          else
          {
            if(std::chrono::system_clock::now() >= end_utc)
            {
              return errc::timed_out;
            }
          }
        }
      }
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace detail

result<path_handle> fs_handle::parent_path_handle(deadline d) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();
  if(_devid == 0 && _inode == 0)
  {
    OUTCOME_TRY(_fetch_inode());
  }
  return detail::containing_directory({}, h, *this, d);
}

result<void> fs_handle::relink(const path_handle &base, path_view_type path, bool atomic_replace, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto &h = const_cast<handle &>(_get_handle());
  path_view::zero_terminated_rendered_path<> zpath(path);
#ifdef O_TMPFILE
  // If the handle was created with O_TMPFILE, we need a different approach
  if(h.flags() & handle::flag::anonymous_inode)
  {
    if(atomic_replace)
    {
      return errc::function_not_supported;
    }
    char _path[PATH_MAX];
    snprintf(_path, PATH_MAX, "/proc/self/fd/%d", h.native_handle().fd);
    if(-1 == ::linkat(AT_FDCWD, _path, base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(), AT_SYMLINK_FOLLOW))
    {
      return posix_error();
    }
    h._flags &= ~handle::flag::anonymous_inode;
    return success();
  }
#endif
  // Open our containing directory
  filesystem::path filename;
  if(_devid == 0 && _inode == 0)
  {
    OUTCOME_TRY(_fetch_inode());
  }
  OUTCOME_TRY(auto &&dirh, detail::containing_directory(std::ref(filename), h, *this, d));
  if(!atomic_replace)
  {
// Linux has an extension for atomic non-replacing renames
#ifdef __linux__
    errno = 0;
    if(-1 !=
#if defined __aarch64__
       syscall(276 /*__NR_renameat2*/, dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(),
               1 /*RENAME_NOREPLACE*/)
#elif defined __arm__
       syscall(382 /*__NR_renameat2*/, dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(),
               1 /*RENAME_NOREPLACE*/)
#elif defined __i386__
       syscall(353 /*__NR_renameat2*/, dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(),
               1 /*RENAME_NOREPLACE*/)
#elif defined __powerpc64__
       syscall(357 /*__NR_renameat2*/, dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(),
               1 /*RENAME_NOREPLACE*/)
#elif defined __sparc__
       syscall(345 /*__NR_renameat2*/, dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(),
               1 /*RENAME_NOREPLACE*/)
#elif defined __x86_64__
       syscall(316 /*__NR_renameat2*/, dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(),
               1 /*RENAME_NOREPLACE*/)
#else
#error Unknown Linux platform
#endif
    )
    {
      return success();
    }
    if(EEXIST == errno)
    {
      return posix_error();
    }
#endif
// Otherwise we need to use linkat followed by unlinkat, carefully reopening the file descriptor
// to preserve path tracking
    if(-1 == ::linkat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(), 0))
    {
      return posix_error();
    }
#ifdef __APPLE__
    // Apple randomly loses link tracking if you open a file with more than
    // one hard link, so we unlink before opening the handle
    if(-1 == ::unlinkat(dirh.native_handle().fd, filename.c_str(), 0))
    {
      return posix_error();
    }
#endif
    int attribs = fcntl(h.native_handle().fd, F_GETFL);
    int errcode = errno;
    if(-1 != attribs)
    {
#if 0
      attribs &= (O_RDONLY | O_WRONLY | O_RDWR
#ifdef O_EXEC
                  | O_EXEC
#endif
                  | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW
#ifdef O_TMPFILE
                  | O_TMPFILE
#endif
                  | O_APPEND | O_DIRECT
#ifdef O_DSYNC
                  | O_DSYNC
#endif
#ifdef O_LARGEFILE
                  | O_LARGEFILE
#endif
#ifdef O_NOATIME
                  | O_NOATIME
#endif
#ifdef O_PATH
                  | O_PATH
#endif
                  | O_SYNC);
#endif
      int fd = ::openat(base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(), attribs, 0x1b0 /*660*/);
      if(-1 == fd)
      {
        errcode = errno;
      }
      else
      {
        ::close(h._v.fd);
        h._v.fd = fd;
        errcode = 0;
      }
    }
#ifndef __APPLE__
    if(-1 == ::unlinkat(dirh.native_handle().fd, filename.c_str(), 0))
    {
      return posix_error();
    }
#endif
    if(errcode != 0)
    {
      return posix_error(errcode);
    }
    return success();
  }
  if(-1 == ::renameat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str()))
  {
    return posix_error();
  }
  return success();
}

result<void> fs_handle::link(const path_handle &base, path_view_type path, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto &h = const_cast<handle &>(_get_handle());
  path_view::zero_terminated_rendered_path<> zpath(path);
#ifdef AT_EMPTY_PATH
  // Try to use the fd linking syscall
  if(-1 != ::linkat(h.native_handle().fd, "", base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(), AT_EMPTY_PATH))
  {
    return success();
  }
#endif
  // Open our containing directory
  filesystem::path filename;
  if(_devid == 0 && _inode == 0)
  {
    OUTCOME_TRY(_fetch_inode());
  }
  OUTCOME_TRY(auto &&dirh, detail::containing_directory(std::ref(filename), h, *this, d));
  if(-1 == ::linkat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.c_str(), 0))
  {
    return posix_error();
  }
  return success();
}

result<void> fs_handle::unlink(deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();
  // Open our containing directory
  filesystem::path filename;
  if(_devid == 0 && _inode == 0)
  {
    OUTCOME_TRY(_fetch_inode());
  }
  OUTCOME_TRY(auto &&dirh, detail::containing_directory(std::ref(filename), h, *this, d));
  if(-1 == ::unlinkat(dirh.native_handle().fd, filename.c_str(), h.is_directory() ? AT_REMOVEDIR : 0))
  {
    return posix_error();
  }
  return success();
}

LLFIO_V2_NAMESPACE_END
