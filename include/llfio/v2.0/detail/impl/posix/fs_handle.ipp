/* A filing system handle
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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
  result<path_handle> containing_directory(optional<std::reference_wrapper<filesystem::path>> out_filename, const handle &h, const fs_handle &fsh, deadline d) noexcept
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
        OUTCOME_TRY(_currentpath, h.current_path());
        // If current path is empty, it's been deleted
        if(_currentpath.empty())
        {
          return errc::no_such_file_or_directory;
        }
        // Split the path into root and leafname
        path_view currentpath(_currentpath);
        path_view filename = currentpath.filename();
        currentpath.remove_filename();
        // Zero terminate the root path so it doesn't get copied later
        const_cast<filesystem::path::string_type &>(_currentpath.native())[currentpath.native_size()] = 0;
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
        path_view::c_str<> zpath(filename);
        struct stat s
        {
        };
        if(-1 == ::fstatat(currentdirh.native_handle().fd, zpath.buffer, &s, AT_SYMLINK_NOFOLLOW))
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
}

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
  path_view::c_str<> zpath(path);
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
    if(-1 == ::linkat(AT_FDCWD, _path, base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer, AT_SYMLINK_FOLLOW))
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
  OUTCOME_TRY(dirh, detail::containing_directory(std::ref(filename), h, *this, d));
  if(!atomic_replace)
  {
// Some systems provide an extension for atomic non-replacing renames
#ifdef RENAME_NOREPLACE
    if(-1 != ::renameat2(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer, RENAME_NOREPLACE))
      return success();
    if(EEXIST == errno)
      return posix_error();
#endif
    // Otherwise we need to use linkat followed by renameat (non-atomic)
    if(-1 == ::linkat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer, 0))
    {
      return posix_error();
    }
  }
  if(-1 == ::renameat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer))
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
  OUTCOME_TRY(dirh, detail::containing_directory(std::ref(filename), h, *this, d));
  if(-1 == ::unlinkat(dirh.native_handle().fd, filename.c_str(), h.is_directory() ? AT_REMOVEDIR : 0))
  {
    return posix_error();
  }
  return success();
}

LLFIO_V2_NAMESPACE_END
