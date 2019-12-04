/* A lockable i/o handle to something
(C) 2015-2019 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Nov 2019


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

#include "../../../lockable_io_handle.hpp"
#include "import.hpp"

#include <sys/file.h>

LLFIO_V2_NAMESPACE_BEGIN

#if 0
#if !defined(__linux__) && !defined(F_OFD_SETLK)
  if(0 == bytes)
  {
    // Non-Linux has a sane locking system in flock() if you are willing to lock the entire file
    int operation = ((d && !d.nsecs) ? LOCK_NB : 0) | ((kind != lock_kind::shared) ? LOCK_EX : LOCK_SH);
    if(-1 == flock(_v.fd, operation))
      failed = true;
  }
  else
#endif

#if !defined(__linux__) && !defined(F_OFD_SETLK)
  if(0 == bytes)
  {
    if(-1 == flock(_v.fd, LOCK_UN))
      failed = true;
  }
  else
#endif
#endif


result<void> lockable_io_handle::lock_file() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(-1 == flock(_v.fd, LOCK_EX))
  {
    return posix_error();
  }
  return success();
}
bool lockable_io_handle::try_lock_file() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(-1 == flock(_v.fd, LOCK_EX | LOCK_NB))
  {
    return false;
  }
  return true;
}
void lockable_io_handle::unlock_file() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  (void) flock(_v.fd, LOCK_UN);
}

result<void> lockable_io_handle::lock_file_shared() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(-1 == flock(_v.fd, LOCK_SH))
  {
    return posix_error();
  }
  return success();
}
bool lockable_io_handle::try_lock_file_shared() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(-1 == flock(_v.fd, LOCK_SH | LOCK_NB))
  {
    return false;
  }
  return true;
}
void lockable_io_handle::unlock_file_shared() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  (void) flock(_v.fd, LOCK_UN);
}


result<lockable_io_handle::extent_guard> lockable_io_handle::lock_file_range(io_handle::extent_type offset, io_handle::extent_type bytes, lock_kind kind, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(d && d.nsecs > 0)
  {
    return errc::not_supported;
  }
  bool failed = false;
  {
    struct flock fl
    {
    };
    memset(&fl, 0, sizeof(fl));
    fl.l_type = (kind != lock_kind::shared) ? F_WRLCK : F_RDLCK;
    constexpr extent_type extent_topbit = static_cast<extent_type>(1) << (8 * sizeof(extent_type) - 1);
    if((offset & extent_topbit) != 0u)
    {
      LLFIO_LOG_WARN(_v.fd, "file_handle::lock() called with offset with top bit set, masking out");
    }
    if((bytes & extent_topbit) != 0u)
    {
      LLFIO_LOG_WARN(_v.fd, "file_handle::lock() called with bytes with top bit set, masking out");
    }
    fl.l_whence = SEEK_SET;
    fl.l_start = offset & ~extent_topbit;
    fl.l_len = bytes & ~extent_topbit;
#ifdef F_OFD_SETLK
    if(-1 == fcntl(_v.fd, (d && !d.nsecs) ? F_OFD_SETLK : F_OFD_SETLKW, &fl))
    {
      if(EINVAL == errno)  // OFD locks not supported on this kernel
      {
        if(-1 == fcntl(_v.fd, (d && !d.nsecs) ? F_SETLK : F_SETLKW, &fl))
          failed = true;
        else
          _flags |= flag::byte_lock_insanity;
      }
      else
        failed = true;
    }
#else
    if(-1 == fcntl(_v.fd, (d && (d.nsecs == 0u)) ? F_SETLK : F_SETLKW, &fl))
    {
      failed = true;
    }
    else
    {
      _flags |= flag::byte_lock_insanity;
    }
#endif
  }
  if(failed)
  {
    if(d && (d.nsecs == 0u) && (EACCES == errno || EAGAIN == errno || EWOULDBLOCK == errno))
    {
      return errc::timed_out;
    }
    return posix_error();
  }
  return extent_guard(this, offset, bytes, kind);
}

void lockable_io_handle::unlock_file_range(io_handle::extent_type offset, io_handle::extent_type bytes) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  bool failed = false;
  {
    struct flock fl
    {
    };
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_UNLCK;
    constexpr extent_type extent_topbit = static_cast<extent_type>(1) << (8 * sizeof(extent_type) - 1);
    fl.l_whence = SEEK_SET;
    fl.l_start = offset & ~extent_topbit;
    fl.l_len = bytes & ~extent_topbit;
#ifdef F_OFD_SETLK
    if(-1 == fcntl(_v.fd, F_OFD_SETLK, &fl))
    {
      if(EINVAL == errno)  // OFD locks not supported on this kernel
      {
        if(-1 == fcntl(_v.fd, F_SETLK, &fl))
          failed = true;
      }
      else
        failed = true;
    }
#else
    if(-1 == fcntl(_v.fd, F_SETLK, &fl))
    {
      failed = true;
    }
#endif
  }
  if(failed)
  {
    auto ret(posix_error());
    (void) ret;
    LLFIO_LOG_FATAL(_v.fd, "io_handle::unlock() failed");
    std::terminate();
  }
}

LLFIO_V2_NAMESPACE_END
