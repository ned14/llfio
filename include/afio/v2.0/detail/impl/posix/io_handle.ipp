/* A handle to something
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: Dec 2015


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

#include "../../../io_handle.hpp"

#include <climits>  // for IOV_MAX
#include <fcntl.h>
#include <sys/uio.h>  // for preadv etc
#include <unistd.h>
#if AFIO_USE_POSIX_AIO
#include <aio.h>
#endif

AFIO_V2_NAMESPACE_BEGIN

size_t io_handle::max_buffers() const noexcept
{
  static size_t v;
  if(v == 0u)
  {
#ifdef __APPLE__
    v = 1;
#else
    long r = sysconf(_SC_IOV_MAX);
    if(r == -1)
    {
#ifdef IOV_MAX
      r = IOV_MAX;
#else
      r = 1;
#endif
    }
    v = r;
#endif
  }
  return v;
}

io_handle::io_result<io_handle::buffers_type> io_handle::read(io_handle::io_request<io_handle::buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(d)
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > IOV_MAX)
  {
    return errc::argument_list_too_long;
  }
#if 0
  struct iovec *iov = (struct iovec *) alloca(reqs.buffers.size() * sizeof(struct iovec));
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    iov[n].iov_base = reqs.buffers[n].data;
    iov[n].iov_len = reqs.buffers[n].len;
  }
#else
  static_assert(sizeof(buffer_type) == sizeof(iovec), "buffer_type and struct iovec do not match");
  auto *iov = reinterpret_cast<struct iovec *>(reqs.buffers.data());
#endif
#ifndef NDEBUG
  if(_v.requires_aligned_io())
  {
    assert((reqs.offset & 511) == 0);
    for(size_t n = 0; n < reqs.buffers.size(); n++)
    {
      assert((reinterpret_cast<uintptr_t>(iov[n].iov_base) & 511) == 0);
      assert((iov[n].iov_len & 511) == 0);
    }
  }
#endif
  ssize_t bytesread = 0;
#if AFIO_MISSING_PIOV
  off_t offset = reqs.offset;
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    bytesread += ::pread(_v.fd, iov[n].iov_base, iov[n].iov_len, offset);
    offset += iov[n].iov_len;
  }
#else
  bytesread = ::preadv(_v.fd, iov, reqs.buffers.size(), reqs.offset);
#endif
  if(bytesread < 0)
  {
    return {errno, std::system_category()};
  }
  for(auto &buffer : reqs.buffers)
  {
    if(buffer.len >= static_cast<size_t>(bytesread))
    {
      bytesread -= buffer.len;
    }
    else if(bytesread > 0)
    {
      buffer.len = bytesread;
      bytesread = 0;
    }
    else
    {
      buffer.len = 0;
    }
  }
  return {reqs.buffers};
}

io_handle::io_result<io_handle::const_buffers_type> io_handle::write(io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(d)
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > IOV_MAX)
  {
    return errc::argument_list_too_long;
  }
#if 0
  struct iovec *iov = (struct iovec *) alloca(reqs.buffers.size() * sizeof(struct iovec));
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    iov[n].iov_base = const_cast<char *>(reqs.buffers[n].data);
    iov[n].iov_len = reqs.buffers[n].len;
  }
#else
  static_assert(sizeof(buffer_type) == sizeof(iovec), "buffer_type and struct iovec do not match");
  auto *iov = reinterpret_cast<struct iovec *>(reqs.buffers.data());
#endif
#ifndef NDEBUG
  if(_v.requires_aligned_io())
  {
    assert((reqs.offset & 511) == 0);
    for(size_t n = 0; n < reqs.buffers.size(); n++)
    {
      assert((reinterpret_cast<uintptr_t>(iov[n].iov_base) & 511) == 0);
      assert((iov[n].iov_len & 511) == 0);
    }
  }
#endif
  ssize_t byteswritten = 0;
#if AFIO_MISSING_PIOV
  off_t offset = reqs.offset;
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    byteswritten += ::pwrite(_v.fd, iov[n].iov_base, iov[n].iov_len, offset);
    offset += iov[n].iov_len;
  }
#else
  byteswritten = ::pwritev(_v.fd, iov, reqs.buffers.size(), reqs.offset);
#endif
  if(byteswritten < 0)
  {
    return {errno, std::system_category()};
  }
  for(auto &buffer : reqs.buffers)
  {
    if(buffer.len >= static_cast<size_t>(byteswritten))
    {
      byteswritten -= buffer.len;
    }
    else if(byteswritten > 0)
    {
      buffer.len = byteswritten;
      byteswritten = 0;
    }
    else
    {
      buffer.len = 0;
    }
  }
  return {reqs.buffers};
}

result<io_handle::extent_guard> io_handle::lock(io_handle::extent_type offset, io_handle::extent_type bytes, bool exclusive, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(d && d.nsecs > 0)
  {
    return errc::not_supported;
  }
  bool failed = false;
#if !defined(__linux__) && !defined(F_OFD_SETLK)
  if(0 == bytes)
  {
    // Non-Linux has a sane locking system in flock() if you are willing to lock the entire file
    int operation = ((d && !d.nsecs) ? LOCK_NB : 0) | (exclusive ? LOCK_EX : LOCK_SH);
    if(-1 == flock(_v.fd, operation))
      failed = true;
  }
  else
#endif
  {
    struct flock fl
    {
    };
    memset(&fl, 0, sizeof(fl));
    fl.l_type = exclusive ? F_WRLCK : F_RDLCK;
    constexpr extent_type extent_topbit = static_cast<extent_type>(1) << (8 * sizeof(extent_type) - 1);
    if((offset & extent_topbit) != 0u)
    {
      AFIO_LOG_WARN(_v.fd, "io_handle::lock() called with offset with top bit set, masking out");
    }
    if((bytes & extent_topbit) != 0u)
    {
      AFIO_LOG_WARN(_v.fd, "io_handle::lock() called with bytes with top bit set, masking out");
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


    return {errno, std::system_category()};
  }
  return extent_guard(this, offset, bytes, exclusive);
}

void io_handle::unlock(io_handle::extent_type offset, io_handle::extent_type bytes) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  bool failed = false;
#if !defined(__linux__) && !defined(F_OFD_SETLK)
  if(0 == bytes)
  {
    if(-1 == flock(_v.fd, LOCK_UN))
      failed = true;
  }
  else
#endif
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
    error_info ret{errno, std::system_category()};
    (void) ret;
    AFIO_LOG_FATAL(_v.fd, "io_handle::unlock() failed");
    std::terminate();
  }
}


AFIO_V2_NAMESPACE_END
