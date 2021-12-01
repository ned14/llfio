/* A handle to a directory
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

#include "../../../directory_handle.hpp"
#include "import.hpp"

#ifdef QUICKCPPLIB_ENABLE_VALGRIND
#include "quickcpplib/valgrind/memcheck.h"  // from quickcpplib include directory
#define LLFIO_VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(a, b) VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE((a), (b))
#else
#define LLFIO_VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(a, b)
#endif

#include <dirent.h> /* Defines DT_* constants */
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/syscall.h>

LLFIO_V2_NAMESPACE_BEGIN

result<directory_handle> directory_handle::directory(const path_handle &base, path_view_type path, mode _mode, creation _creation, caching _caching,
                                                     flag flags) noexcept
{
  if(flags & flag::unlink_on_first_close)
  {
    return errc::invalid_argument;
  }
  result<directory_handle> ret(directory_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::directory | native_handle_type::disposition::path;
  // POSIX does not permit directory opens with O_RDWR like Windows, so silently convert to read
  if(_mode == mode::attr_write)
  {
    _mode = mode::attr_read;
  }
  else if(_mode == mode::write || _mode == mode::append)
  {
    _mode = mode::read;
  }
  // Also trying to truncate a directory returns EISDIR
  if(_creation == creation::truncate_existing)
  {
    return errc::is_a_directory;
  }
  OUTCOME_TRY(auto &&attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, _creation, _caching, flags));
  attribs &= ~O_NONBLOCK;
  nativeh.behaviour &= ~native_handle_type::disposition::nonblocking;
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
#ifdef O_DIRECTORY
  attribs |= O_DIRECTORY;
#endif
#ifdef O_SEARCH
  attribs |= O_SEARCH;
#endif
  if(base.is_valid() && path.empty())
  {
    // It can happen that we are passed a base fd and no leafname, in which case he
    // really ought to be cloning the handle. But let's humour him.
    path = ".";
  }
  path_view::zero_terminated_rendered_path<> zpath(path);
  auto rename_random_dir_over_existing_dir = [_mode, _caching, flags](const path_handle &base, path_view_type path) -> result<directory_handle> {
    // Take a path handle to the directory containing the file
    auto path_parent = path.parent_path();
    path_handle dirh;
    if(base.is_valid() && path_parent.empty())
    {
      OUTCOME_TRY(auto &&dh, base.clone());
      dirh = path_handle(std::move(dh));
    }
    else if(!path_parent.empty())
    {
      OUTCOME_TRY(auto &&dh, path_handle::path(base, path_parent));
      dirh = std::move(dh);
    }
    // Create a randomly named directory, and rename it over
    OUTCOME_TRY(auto &&rfh, uniquely_named_directory(dirh, _mode, _caching, flags));
    auto r = rfh.relink(dirh, path.filename());
    if(r)
    {
      return std::move(rfh);
    }
    // If failed to rename, remove
    (void) rfh.unlink();
    return std::move(r).error();
  };
  if(base.is_valid())
  {
    if(_creation == creation::if_needed || _creation == creation::only_if_not_exist || _creation == creation::always_new)
    {
      if(-1 == ::mkdirat(base.native_handle().fd, zpath.c_str(), 0x1f8 /*770*/))
      {
        if(EEXIST != errno || _creation == creation::only_if_not_exist)
        {
          return posix_error();
        }
        if(_creation == creation::always_new)
        {
          auto r = rename_random_dir_over_existing_dir(base, path);
          if(!r)
          {
            return std::move(r).error();
          }
          ret = std::move(r).value();
          goto opened;
        }
      }
      attribs &= ~(O_CREAT | O_EXCL);
    }
    nativeh.fd = ::openat(base.native_handle().fd, zpath.c_str(), attribs);
  }
  else
  {
    if(_creation == creation::if_needed || _creation == creation::only_if_not_exist || _creation == creation::always_new)
    {
      if(-1 == ::mkdir(zpath.c_str(), 0x1f8 /*770*/))
      {
        if(EEXIST != errno || _creation == creation::only_if_not_exist)
        {
          return posix_error();
        }
        if(_creation == creation::always_new)
        {
          auto r = rename_random_dir_over_existing_dir(base, path);
          if(!r)
          {
            return std::move(r).error();
          }
          ret = std::move(r).value();
          goto opened;
        }
      }
      attribs &= ~(O_CREAT | O_EXCL);
    }
    nativeh.fd = ::open(zpath.c_str(), attribs);
  }
  if(-1 == nativeh.fd)
  {
    return posix_error();
  }
opened:
  if(!(flags & flag::disable_safety_unlinks))
  {
    if(!ret.value()._fetch_inode())
    {
      // If fetching inode failed e.g. were opening device, disable safety unlinks
      ret.value()._flags &= ~flag::disable_safety_unlinks;
    }
  }
  if(ret.value().are_safety_barriers_issued())
  {
    fsync(nativeh.fd);
  }
  return ret;
}

result<directory_handle> directory_handle::reopen(mode mode_, caching caching_, deadline d) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  // Fast path
  if(mode_ == mode::unchanged && caching_ == caching::unchanged)
  {
    result<directory_handle> ret(directory_handle(native_handle_type(), _devid, _inode, kernel_caching(), _flags));
    ret.value()._v.behaviour = _v.behaviour;
    ret.value()._v.fd = ::fcntl(_v.fd, F_DUPFD_CLOEXEC, 0);
    if(-1 == ret.value()._v.fd)
    {
      return posix_error();
    }
    return ret;
  }
  // Slow path
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
  for(;;)
  {
    // Get the current path of myself
    OUTCOME_TRY(auto &&currentpath, current_path());
    // Open myself
    auto fh = directory({}, currentpath, mode_, creation::open_existing, caching_, _flags);
    if(fh)
    {
      if(fh.value().unique_id() == unique_id())
      {
        return fh;
      }
    }
    else
    {
      if(fh.error() != errc::no_such_file_or_directory)
      {
        return std::move(fh).error();
      }
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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<path_handle> directory_handle::clone_to_path_handle() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  result<path_handle> ret(path_handle(native_handle_type(), kernel_caching(), _flags));
  ret.value()._v.behaviour = _v.behaviour;
  ret.value()._v.fd = ::fcntl(_v.fd, F_DUPFD_CLOEXEC, 0);
  if(-1 == ret.value()._v.fd)
  {
    return posix_error();
  }
  return ret;
}

result<directory_handle::buffers_type> directory_handle::read(io_request<buffers_type> req, deadline /*unused*/) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(req.buffers.empty())
  {
    return std::move(req.buffers);
  }
  // Is glob a single entry match? If so, this is really a stat call
  path_view_type::zero_terminated_rendered_path<> zglob(req.glob);
  if(!req.glob.empty() && !req.glob.contains_glob())
  {
    struct stat s
    {
    };
    if(-1 == ::fstatat(_v.fd, zglob.c_str(), &s, AT_SYMLINK_NOFOLLOW))
    {
      return posix_error();
    }
    req.buffers[0].stat.st_dev = s.st_dev;
    req.buffers[0].stat.st_ino = s.st_ino;
    req.buffers[0].stat.st_type = to_st_type(s.st_mode);
    req.buffers[0].stat.st_perms = s.st_mode & 0xfff;
    req.buffers[0].stat.st_nlink = s.st_nlink;
    req.buffers[0].stat.st_uid = s.st_uid;
    req.buffers[0].stat.st_gid = s.st_gid;
    req.buffers[0].stat.st_rdev = s.st_rdev;
#ifdef __ANDROID__
    req.buffers[0].stat.st_atim = to_timepoint(*((struct timespec *) &s.st_atime));
    req.buffers[0].stat.st_mtim = to_timepoint(*((struct timespec *) &s.st_mtime));
    req.buffers[0].stat.st_ctim = to_timepoint(*((struct timespec *) &s.st_ctime));
#elif defined(__APPLE__)
    req.buffers[0].stat.st_atim = to_timepoint(s.st_atimespec);
    req.buffers[0].stat.st_mtim = to_timepoint(s.st_mtimespec);
    req.buffers[0].stat.st_ctim = to_timepoint(s.st_ctimespec);
#else  // Linux and BSD
    req.buffers[0].stat.st_atim = to_timepoint(s.st_atim);
    req.buffers[0].stat.st_mtim = to_timepoint(s.st_mtim);
    req.buffers[0].stat.st_ctim = to_timepoint(s.st_ctim);
#endif
    req.buffers[0].stat.st_size = s.st_size;
    req.buffers[0].stat.st_allocated = static_cast<handle::extent_type>(s.st_blocks) * 512;
    req.buffers[0].stat.st_blocks = s.st_blocks;
    req.buffers[0].stat.st_blksize = s.st_blksize;
#ifdef HAVE_STAT_FLAGS
    req.buffers[0].stat.st_flags = s.st_flags;
#endif
#ifdef HAVE_STAT_GEN
    req.buffers[0].stat.st_gen = s.st_gen;
#endif
#ifdef HAVE_BIRTHTIMESPEC
#if defined(__APPLE__)
    req.buffers[0].stat.st_birthtim = to_timepoint(s.st_birthtimespec);
#else
    req.buffers[0].stat.st_birthtim = to_timepoint(s.st_birthtim);
#endif
#endif
    req.buffers[0].stat.st_sparse =
    static_cast<unsigned int>((static_cast<handle::extent_type>(s.st_blocks) * 512) < static_cast<handle::extent_type>(s.st_size));
    req.buffers._resize(1);
    static constexpr stat_t::want default_stat_contents = stat_t::want::dev | stat_t::want::ino | stat_t::want::type | stat_t::want::perms |
                                                          stat_t::want::nlink | stat_t::want::uid | stat_t::want::gid | stat_t::want::rdev |
                                                          stat_t::want::atim | stat_t::want::mtim | stat_t::want::ctim | stat_t::want::size |
                                                          stat_t::want::allocated | stat_t::want::blocks | stat_t::want::blksize
#ifdef HAVE_STAT_FLAGS
                                                          | stat_t::want::flags
#endif
#ifdef HAVE_STAT_GEN
                                                          | stat_t::want::gen
#endif
#ifdef HAVE_BIRTHTIMESPEC
                                                          | stat_t::want::birthtim
#endif
                                                          | stat_t::want::sparse;
    req.buffers._metadata = default_stat_contents;
    req.buffers._done = true;
    return std::move(req.buffers);
  }
#ifdef __linux__
  // Unlike FreeBSD, Linux doesn't define a getdents() function, so we'll do that here.
  using getdents64_t = int (*)(int, char *, unsigned int);
  static auto getdents = static_cast<getdents64_t>([](int fd, char *buf, unsigned count) -> int { return syscall(SYS_getdents64, fd, buf, count); });
  using dirent = dirent64;
#endif
#ifdef __APPLE__
  // OS X defines a getdirentries64() kernel syscall which can emulate getdents
  typedef int (*getdents_emulation_t)(int, char *, unsigned);
  static getdents_emulation_t getdents = static_cast<getdents_emulation_t>([](int fd, char *buf, unsigned count) -> int {
    off_t foo;
    return syscall(SYS_getdirentries64, fd, buf, count, &foo);
  });
#endif
  if(!req.buffers._kernel_buffer && req.kernelbuffer.empty())
  {
    // Let's assume the average leafname will be 64 characters long.
    size_t toallocate = (sizeof(dirent) + 64) * req.buffers.size();
    auto *mem = (char *) operator new[](toallocate, std::nothrow);  // don't initialise
    if(mem == nullptr)
    {
      return errc::not_enough_memory;
    }
    req.buffers._kernel_buffer = std::unique_ptr<char[]>(mem);
    req.buffers._kernel_buffer_size = toallocate;
  }
  stat_t::want default_stat_contents = stat_t::want::ino | stat_t::want::type;
  dirent *buffer;
  size_t bytesavailable, bytes;
  bool done = false;
  do
  {
    buffer = req.kernelbuffer.empty() ? reinterpret_cast<dirent *>(req.buffers._kernel_buffer.get()) : reinterpret_cast<dirent *>(req.kernelbuffer.data());
    bytesavailable = req.kernelbuffer.empty() ? req.buffers._kernel_buffer_size : req.kernelbuffer.size();
    while(_lock.exchange(1, std::memory_order_relaxed) != 0)
    {
      std::this_thread::yield();
    }
    auto unlock = make_scope_exit([this]() noexcept { _lock.store(0, std::memory_order_release); });
    (void) unlock;
// Seek to start
#ifdef __linux__
    if(-1 == ::lseek64(_v.fd, 0, SEEK_SET))
    {
      return posix_error();
    }
#else
    if(-1 == ::lseek(_v.fd, 0, SEEK_SET))
      return posix_error();
#endif
    bytes = 0;
    int _bytes;
    do
    {
      assert(bytes <= bytesavailable);
      _bytes = getdents(_v.fd, reinterpret_cast<char *>(buffer) + bytes, bytesavailable - bytes);
      if(_bytes == 0)
      {
        done = true;
        break;
      }
      if(req.kernelbuffer.empty() && _bytes == -1 && EINVAL == errno)
      {
        size_t toallocate = req.buffers._kernel_buffer_size * 2;
        auto *mem = (char *) operator new[](toallocate, std::nothrow);  // don't initialise
        if(mem == nullptr)
        {
          return errc::not_enough_memory;
        }
        req.buffers._kernel_buffer.reset();
        req.buffers._kernel_buffer = std::unique_ptr<char[]>(mem);
        req.buffers._kernel_buffer_size = toallocate;
        // We need to reset and do the whole thing against to ensure single shot atomicity
        break;
      }
      else if(_bytes == -1)
      {
        return posix_error();
      }
      else
      {
        assert(_bytes > 0);
        bytes += _bytes;
      }
    } while(!done);
  } while(!done);
  if(bytes == 0)
  {
    req.buffers._resize(0);
    req.buffers._metadata = default_stat_contents;
    req.buffers._done = true;
    return std::move(req.buffers);
  }
  LLFIO_VALGRIND_MAKE_MEM_DEFINED_IF_ADDRESSABLE(buffer, bytes);  // NOLINT
  size_t n = 0;
  for(dirent *dent = buffer;; dent = reinterpret_cast<dirent *>(reinterpret_cast<uintptr_t>(dent) + dent->d_reclen))
  {
    if(dent->d_ino != 0u)
    {
      size_t length = strchr(dent->d_name, 0) - dent->d_name;
      if(length <= 2 && '.' == dent->d_name[0])
      {
        if(1 == length || '.' == dent->d_name[1])
        {
          goto cont;
        }
      }
      if(!req.glob.empty() && fnmatch(zglob.c_str(), dent->d_name, 0) != 0)
      {
        goto cont;
      }
      directory_entry &item = req.buffers[n];
      item.leafname = path_view(dent->d_name, length, path_view::zero_terminated);
      item.stat = stat_t(nullptr);
      item.stat.st_ino = dent->d_ino;
      char d_type = dent->d_type;
      switch(d_type)
      {
      case DT_BLK:
        item.stat.st_type = filesystem::file_type::block;
        break;
      case DT_CHR:
        item.stat.st_type = filesystem::file_type::character;
        break;
      case DT_DIR:
        item.stat.st_type = filesystem::file_type::directory;
        break;
      case DT_FIFO:
        item.stat.st_type = filesystem::file_type::fifo;
        break;
      case DT_LNK:
        item.stat.st_type = filesystem::file_type::symlink;
        break;
      case DT_REG:
        item.stat.st_type = filesystem::file_type::regular;
        break;
      case DT_SOCK:
        item.stat.st_type = filesystem::file_type::socket;
        break;
      case DT_UNKNOWN:
        // Don't say we return type
        default_stat_contents = default_stat_contents & ~stat_t::want::type;
        break;
      }
      n++;
    }
  cont:
    if((bytes -= dent->d_reclen) <= 0)
    {
      // Fill is complete
      req.buffers._resize(n);
      req.buffers._metadata = default_stat_contents;
      req.buffers._done = true;
      return std::move(req.buffers);
    }
    if(n >= req.buffers.size())
    {
      // Fill is incomplete
      req.buffers._metadata = default_stat_contents;
      req.buffers._done = false;
      return std::move(req.buffers);
    }
  }
}

LLFIO_V2_NAMESPACE_END
