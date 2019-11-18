/* A handle to something
(C) 2015-2019 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
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
#if LLFIO_USE_POSIX_AIO
#include <aio.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

constexpr inline void _check_iovec_match()
{
  static_assert(sizeof(io_handle::buffer_type) == sizeof(iovec), "buffer_type and struct iovec do not match in size");
  static_assert(offsetof(io_handle::buffer_type, _data) == offsetof(iovec, iov_base), "buffer_type and struct iovec do not have same offset of data member");
  static_assert(offsetof(io_handle::buffer_type, _len) == offsetof(iovec, iov_len), "buffer_type and struct iovec do not have same offset of len member");
}

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
  LLFIO_LOG_FUNCTION_CALL(this);
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
#if LLFIO_MISSING_PIOV
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
    return posix_error();
  }
  for(size_t i = 0; i < reqs.buffers.size(); i++)
  {
    auto &buffer = reqs.buffers[i];
    if(buffer.size() >= static_cast<size_t>(bytesread))
    {
      bytesread -= buffer.size();
    }
    else
    {
      buffer = {buffer.data(), (size_type) bytesread};
      reqs.buffers = {reqs.buffers.data(), i + 1};
      break;
    }
  }
  return {reqs.buffers};
}

io_handle::io_result<io_handle::const_buffers_type> io_handle::write(io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
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
#if LLFIO_MISSING_PIOV
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
    return posix_error();
  }
  for(size_t i = 0; i < reqs.buffers.size(); i++)
  {
    auto &buffer = reqs.buffers[i];
    if(buffer.size() >= static_cast<size_t>(byteswritten))
    {
      byteswritten -= buffer.size();
    }
    else
    {
      buffer = {buffer.data(), (size_type) byteswritten};
      reqs.buffers = {reqs.buffers.data(), i + 1};
      break;
    }
  }
  return {reqs.buffers};
}

LLFIO_V2_NAMESPACE_END
