/* A handle to a symbolic link
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Jul 2018


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

#include "../../../symlink_handle.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  // Used by stat_t.cpp to work around a dependency problem
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<void> stat_from_symlink(struct stat &s, const handle &_h) noexcept
  {
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
    (void) s;
    (void) _h;
    return posix_error(EBADF);
#else
    const auto &h = static_cast<const symlink_handle &>(_h);
    if(-1 == ::fstatat(h._dirh.native_handle().fd, h._leafname.c_str(), &s, AT_SYMLINK_NOFOLLOW))
    {
      return posix_error();
    }
    return success();
#endif
  }
}  // namespace detail

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> symlink_handle::_create_symlink(const path_handle &dirh, const handle::path_type &filename, path_view target,
                                                                             deadline d, bool atomic_replace, bool exists_is_ok) noexcept
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
  path_view::zero_terminated_rendered_path<> zpath(target);
  try
  {
    if(atomic_replace)
    {
      // symlinkat() won't replace an existing symlink, so we need to create it
      // with a random name and atomically rename over the existing one.
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        // std::cerr << "symlinkat " << zpath.c_str() << " " << dirh.native_handle().fd << " " << randomname << std::endl;
        if(-1 == ::symlinkat(zpath.c_str(), dirh.is_valid() ? dirh.native_handle().fd : AT_FDCWD, randomname.c_str()))
        {
          if(EEXIST == errno)
          {
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
            continue;
          }
          return posix_error();
        }
        // std::cerr << "renameat " << dirh.native_handle().fd << " " << randomname << " " << filename << std::endl;
        if(-1 == ::renameat(dirh.is_valid() ? dirh.native_handle().fd : AT_FDCWD, randomname.c_str(), dirh.is_valid() ? dirh.native_handle().fd : AT_FDCWD,
                            filename.c_str()))
        {
          return posix_error();
        }
        return success();
      }
    }
    else
    {
      // std::cerr << "symlinkat " << zpath.c_str() << " " << dirh.native_handle().fd << " " << filename << std::endl;
      if(-1 == ::symlinkat(zpath.c_str(), dirh.is_valid() ? dirh.native_handle().fd : AT_FDCWD, filename.c_str()))
      {
        if(exists_is_ok && EEXIST == errno)
        {
          return success();
        }
        return posix_error();
      }
      return success();
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<symlink_handle> symlink_handle::reopen(mode mode_, deadline d) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
#if LLFIO_SYMLINK_HANDLE_IS_FAKED
  result<symlink_handle> ret(symlink_handle(native_handle_type(), _devid, _inode, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  OUTCOME_TRY(auto &&dirh, _dirh.clone());
  ret.value()._dirh = path_handle(std::move(dirh));
  try
  {
    ret.value()._leafname = _leafname;
  }
  catch(...)
  {
    return error_from_exception();
  }
  return ret;
#endif
  // fast path
  if(mode_ == mode::unchanged)
  {
    result<symlink_handle> ret(symlink_handle(native_handle_type(), _devid, _inode, _flags));
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
    auto fh = symlink({}, currentpath, mode_, creation::open_existing, _flags);
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

#if LLFIO_SYMLINK_HANDLE_IS_FAKED
result<symlink_handle::path_type> symlink_handle::current_path() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  try
  {
    // Deleted?
    if(!_dirh.is_valid() && _leafname.empty())
    {
      return _leafname;
    }
    for(;;)
    {
      // Sanity check that we still exist
      struct stat s1, s2;
      if(-1 == ::fstatat(_dirh.native_handle().fd, _leafname.c_str(), &s1, AT_SYMLINK_NOFOLLOW))
      {
        return posix_error();
      }
      OUTCOME_TRY(auto &&dirpath, _dirh.current_path());
      dirpath /= _leafname;
      if(-1 == ::lstat(dirpath.c_str(), &s2) || s1.st_dev != s2.st_dev || s1.st_ino != s2.st_ino)
      {
        continue;
      }
      return {std::move(dirpath)};
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<void> symlink_handle::relink(const path_handle &base, path_view_type path, bool atomic_replace, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(fs_handle::relink(base, path, atomic_replace, d));
  try
  {
    // Take a path handle to the directory containing the symlink
    auto path_parent = path.parent_path();
    _leafname = path.filename().path();
    if(base.is_valid() && path_parent.empty())
    {
      OUTCOME_TRY(auto &&dh, base.clone());
      _dirh = path_handle(std::move(dh));
    }
    else if(!path_parent.empty())
    {
      OUTCOME_TRY(auto &&dh, path_handle::path(base, path_parent));
      _dirh = std::move(dh);
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
  return success();
}

result<void> symlink_handle::unlink(deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(fs_handle::unlink(d));
  _dirh = {};
  _leafname = path_type();
  return success();
}
#endif

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<symlink_handle> symlink_handle::symlink(const path_handle &base, symlink_handle::path_view_type path,
                                                                               symlink_handle::mode _mode, symlink_handle::creation _creation,
                                                                               flag flags) noexcept
{
  result<symlink_handle> ret(symlink_handle(native_handle_type(), 0, 0, flags));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::symlink;
  if(_mode == mode::append || _creation == creation::truncate_existing)
  {
    return errc::function_not_supported;
  }
  OUTCOME_TRY(auto &&attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, _creation, caching::all, flags));
  attribs &= ~O_NONBLOCK;
  nativeh.behaviour &= ~native_handle_type::disposition::nonblocking;
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  path_handle _dirh_, *dirh = &_dirh_;
  path_type leafname;
#else
  (void) attribs;
  path_handle *dirh = &ret.value()._dirh;
  path_type &leafname = ret.value()._leafname;
#endif
  int dirhfd = AT_FDCWD;
  try
  {
    // Take a path handle to the directory containing the symlink
    auto path_parent = path.parent_path();
    leafname = path.filename().path();
    if(base.is_valid() && path_parent.empty())
    {
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
      dirhfd = base.native_handle().fd;
      dirh = const_cast<path_handle *>(&base);
#else
      OUTCOME_TRY(auto &&dh, base.clone());
      *dirh = path_handle(std::move(dh));
      dirhfd = dirh->native_handle().fd;
#endif
    }
    else
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED  // always take a dirh if faking the handle for race safety
    if(!path_parent.empty())
#endif
    {
      // If faking the symlink, write this directly into the member variable cache
      OUTCOME_TRY(*dirh, path_handle::path(base, path_parent.empty() ? "." : path_parent));
      dirhfd = dirh->native_handle().fd;
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
  switch(_creation)
  {
  case creation::open_existing:
  {
    // Complain if it doesn't exist
    struct stat s;
    if(-1 == ::fstatat(dirhfd, leafname.c_str(), &s, AT_SYMLINK_NOFOLLOW))
    {
      return posix_error();
    }
    break;
  }
  case creation::only_if_not_exist:
  case creation::if_needed:
  case creation::truncate_existing:
  case creation::always_new:
  {
    // Create an empty symlink, ignoring any file exists errors, unless only_if_not_exist
    auto r = ret.value()._create_symlink(*dirh, leafname,
#if defined(__linux__) || defined(__APPLE__)
                                         ".",  // Linux and Mac OS is not POSIX conforming here, and refuses to create empty symlinks
#else
                                         "",
#endif
                                         std::chrono::seconds(10), creation::always_new == _creation, creation::if_needed == _creation);
    if(!r)
    {
      return std::move(r).error();
    }
    break;
  }
  }
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  // Linux can open symbolic links directly like this
  attribs |= O_PATH | O_NOFOLLOW;
  nativeh.fd = ::openat(dirhfd, leafname.c_str(), attribs, 0x1b0 /*660*/);
  if(-1 == nativeh.fd)
  {
    return posix_error();
  }
#endif
  return ret;
}

result<symlink_handle::buffers_type> symlink_handle::read(symlink_handle::io_request<symlink_handle::buffers_type> req) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  symlink_handle::buffers_type tofill;
  if(req.kernelbuffer.empty())
  {
    // Let's assume the average symbolic link will be 256 characters long.
    size_t toallocate = 256;
    auto *mem = new(std::nothrow) char[toallocate];
    if(mem == nullptr)
    {
      return errc::not_enough_memory;
    }
    tofill._kernel_buffer = std::unique_ptr<char[]>(mem);
    tofill._kernel_buffer_size = toallocate;
  }
  for(;;)
  {
    char *buffer = req.kernelbuffer.empty() ? tofill._kernel_buffer.get() : req.kernelbuffer.data();
    size_t bytes = req.kernelbuffer.empty() ? tofill._kernel_buffer_size : req.kernelbuffer.size();
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
    // Linux has the ability to read the link from a fd
    ssize_t read = ::readlinkat(_v.fd, "", buffer, bytes);
#else
    ssize_t read = ::readlinkat(_dirh.native_handle().fd, _leafname.c_str(), buffer, bytes);
#endif
    if(read == -1)
    {
      return posix_error();
    }
    if((size_t) read == bytes)
    {
      if(req.kernelbuffer.empty())
      {
        tofill._kernel_buffer.reset();
        size_t toallocate = tofill._kernel_buffer_size * 2;
        auto *mem = new(std::nothrow) char[toallocate];
        if(mem == nullptr)
        {
          return errc::not_enough_memory;
        }
        tofill._kernel_buffer = std::unique_ptr<char[]>(mem);
        tofill._kernel_buffer_size = toallocate;
        continue;
      }
      return errc::not_enough_memory;
    }
    // We know we can null terminate as read < bytes
    buffer[read] = 0;
    tofill._link = path_view(buffer, read, path_view::zero_terminated);
    tofill._type = symlink_type::symbolic;
    return {std::move(tofill)};
  }
}

result<symlink_handle::const_buffers_type> symlink_handle::write(symlink_handle::io_request<symlink_handle::const_buffers_type> req, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  if(_devid == 0 && _inode == 0)
  {
    OUTCOME_TRY(_fetch_inode());
  }
  path_type filename;
  OUTCOME_TRY(auto &&dirh, detail::containing_directory(std::ref(filename), *this, *this, d));
#else
  const path_handle &dirh = _dirh;
  const path_type &filename = _leafname;
#endif
  OUTCOME_TRY(_create_symlink(dirh, filename, req.buffers.path(), d, true, false));
#if !LLFIO_SYMLINK_HANDLE_IS_FAKED
  {
    // Current fd now points at the symlink we just atomically replaced, so need to reopen
    // it onto the new symlink
    auto newthis = symlink(dirh, filename, is_writable() ? mode::write : mode::read, creation::open_existing, _flags);
    // Prevent unlink on first close if set
    _flags &= ~flag::unlink_on_first_close;
    // Close myself
    OUTCOME_TRY(close());
    // If reopen failed, report that now
    if(!newthis)
    {
      return std::move(newthis).error();
    }
    // Swap myself with the new this
    swap(newthis.value());
  }
#endif
  return success(std::move(req.buffers));
}

LLFIO_V2_NAMESPACE_END
