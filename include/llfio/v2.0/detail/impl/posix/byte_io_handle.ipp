/* A handle to something
(C) 2015-2021 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
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

#include "../../../byte_io_handle.hpp"

#include "import.hpp"

#include <climits>  // for IOV_MAX
#include <fcntl.h>
#include <poll.h>
#include <sys/uio.h>  // for preadv etc
#include <unistd.h>

#ifndef LLFIO_EXCLUDE_NETWORKING
#include <poll.h>
#endif

#ifndef LLFIO_DISABLE_SIGNAL_GUARD
#include "quickcpplib/signal_guard.hpp"
#endif

LLFIO_V2_NAMESPACE_BEGIN

constexpr inline void _check_iovec_match()
{
  static_assert(sizeof(byte_io_handle::buffer_type) == sizeof(iovec), "buffer_type and struct iovec do not match in size");
  static_assert(offsetof(byte_io_handle::buffer_type, _data) == offsetof(iovec, iov_base),
                "buffer_type and struct iovec do not have same offset of data member");
  static_assert(offsetof(byte_io_handle::buffer_type, _len) == offsetof(iovec, iov_len), "buffer_type and struct iovec do not have same offset of len member");
}

size_t byte_io_handle::_do_max_buffers() const noexcept
{
  static size_t v;
  if(v == 0u)
  {
#ifdef __APPLE__
    v = 1;
#else
    long r = sysconf(_SC_IOV_MAX);
    if(r < 1)
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

byte_io_handle::io_result<byte_io_handle::buffers_type> byte_io_handle::_do_read(byte_io_handle::io_request<byte_io_handle::buffers_type> reqs,
                                                                                 deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(d && !_v.is_nonblocking())
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > IOV_MAX)
  {
    return errc::argument_list_too_long;
  }
  LLFIO_POSIX_DEADLINE_TO_SLEEP_INIT(d);
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
      // Reads must have aligned length for portability
      assert((iov[n].iov_len & 511) == 0);
    }
  }
#endif
  ssize_t bytesread = 0;
  if(is_seekable())
  {
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
  }
  else
  {
    do
    {
      bytesread = ::readv(_v.fd, iov, reqs.buffers.size());
      if(bytesread <= 0)
      {
        if(bytesread == 0 && is_socket())
        {
          // Sockets read zero if the remote has shutdown
          break;
        }
        if(bytesread < 0 && EWOULDBLOCK != errno && EAGAIN != errno)
        {
          return posix_error();
        }
        if(!d || !d.steady || d.nsecs != 0)
        {
          LLFIO_POSIX_DEADLINE_TO_SLEEP_LOOP(d);
          int mstimeout = (timeout == nullptr) ? -1 : (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000LL);
          pollfd p;
          memset(&p, 0, sizeof(p));
          p.fd = _v.fd;
          p.events = POLLIN | POLLERR;
          if(-1 == ::poll(&p, 1, mstimeout))
          {
            return posix_error();
          }
        }
        LLFIO_POSIX_DEADLINE_TO_TIMEOUT_LOOP(d);
      }
    } while(bytesread <= 0);
  }
  for(size_t i = 0; i < reqs.buffers.size(); i++)
  {
    auto &buffer = reqs.buffers[i];
    if(buffer.size() <= static_cast<size_t>(bytesread))
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

byte_io_handle::io_result<byte_io_handle::const_buffers_type> byte_io_handle::_do_write(byte_io_handle::io_request<byte_io_handle::const_buffers_type> reqs,
                                                                                        deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(d && !_v.is_nonblocking())
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > IOV_MAX)
  {
    return errc::argument_list_too_long;
  }
  LLFIO_POSIX_DEADLINE_TO_SLEEP_INIT(d);
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
      // length can be unaligned for writes
    }
  }
#endif
  ssize_t byteswritten = 0;
  if(is_seekable())
  {
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
  }
  else
  {
    do
    {
      // Can't guarantee that user code hasn't enabled SIGPIPE
      byteswritten =
#ifndef LLFIO_DISABLE_SIGNAL_GUARD
      QUICKCPPLIB_NAMESPACE::signal_guard::signal_guard(
      QUICKCPPLIB_NAMESPACE::signal_guard::signalc_set::broken_pipe,
      [&]
      {
        return
#endif
        ::writev(_v.fd, iov, reqs.buffers.size());
#ifndef LLFIO_DISABLE_SIGNAL_GUARD
      },
      [&](const QUICKCPPLIB_NAMESPACE::signal_guard::raised_signal_info * /*unused*/)
      {
        errno = EPIPE;
        return -1;
      });
#endif
      if(byteswritten <= 0)
      {
        if(byteswritten == 0 && is_socket())
        {
          // Sockets write zero if write has been shutdown
          break;
        }
        if(byteswritten < 0 && EWOULDBLOCK != errno && EAGAIN != errno)
        {
          return posix_error();
        }
        if(!d || !d.steady || d.nsecs != 0)
        {
          LLFIO_POSIX_DEADLINE_TO_SLEEP_LOOP(d);
          int mstimeout = (timeout == nullptr) ? -1 : (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000LL);
          pollfd p;
          memset(&p, 0, sizeof(p));
          p.fd = _v.fd;
          p.events = POLLOUT | POLLERR;
          if(-1 == ::poll(&p, 1, mstimeout))
          {
            return posix_error();
          }
        }
        LLFIO_POSIX_DEADLINE_TO_TIMEOUT_LOOP(d);
      }
    } while(byteswritten <= 0);
  }
  for(size_t i = 0; i < reqs.buffers.size(); i++)
  {
    auto &buffer = reqs.buffers[i];
    if(buffer.size() <= static_cast<size_t>(byteswritten))
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

byte_io_handle::io_result<byte_io_handle::const_buffers_type> byte_io_handle::_do_barrier(byte_io_handle::io_request<byte_io_handle::const_buffers_type> reqs,
                                                                                          barrier_kind kind, deadline d) noexcept
{
  (void) kind;
  LLFIO_LOG_FUNCTION_CALL(this);
  if(is_pipe() || is_socket())
  {
    return success();  // nothing was flushed
  }
  if(d)
  {
    return errc::not_supported;
  }
#ifdef __linux__
  if(kind <= barrier_kind::wait_data_only)
  {
    // Linux has a lovely dedicated syscall giving us exactly what we need here
    extent_type offset = reqs.offset, bytes = 0;
    // empty buffers means bytes = 0 which means sync entire file
    for(const auto &req : reqs.buffers)
    {
      bytes += req.size();
    }
    unsigned flags = SYNC_FILE_RANGE_WRITE;  // start writing all dirty pages in range now
    if(kind == barrier_kind::wait_data_only)
    {
      flags |= SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WAIT_AFTER;  // block until they're on storage
    }
    if(-1 != ::sync_file_range(_v.fd, offset, bytes, flags))
    {
      return {reqs.buffers};
    }
  }
#endif
#if !defined(__FreeBSD__) && !defined(__APPLE__)  // neither of these have fdatasync()
  if(kind <= barrier_kind::wait_data_only)
  {
    if(-1 == ::fdatasync(_v.fd))
    {
      return posix_error();
    }
    return {reqs.buffers};
  }
#endif
#ifdef __APPLE__
  if(((uint8_t) kind & 1) == 0)
  {
    // OS X fsync doesn't wait for the device to flush its buffers
    if(-1 == ::fsync(_v.fd))
      return posix_error();
    return {std::move(reqs.buffers)};
  }
  // This is the fsync as on every other OS
  if(-1 == ::fcntl(_v.fd, F_FULLFSYNC))
    return posix_error();
#else
  if(-1 == ::fsync(_v.fd))
  {
    return posix_error();
  }
#endif
  return {reqs.buffers};
}

#ifndef LLFIO_EXCLUDE_NETWORKING
result<size_t> poll(span<poll_what> out, span<pollable_handle *> handles, span<const poll_what> query, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(nullptr);
  if(out.size() != handles.size())
  {
    return errc::invalid_argument;
  }
  if(out.size() != query.size())
  {
    return errc::invalid_argument;
  }
  if(handles.empty())
  {
    return 0;
  }
  if(handles.size() > 1024)
  {
    return errc::argument_out_of_domain;
  }
  LLFIO_DEADLINE_TO_SLEEP_INIT(d);
  auto *fds = (pollfd *) alloca(handles.size() * sizeof(pollfd));
  memset(fds, 0, handles.size() * sizeof(pollfd));
  nfds_t fdscount = 0;
  for(size_t n = 0; n < handles.size(); n++)
  {
    if(handles[n] != nullptr)
    {
      auto &h_ = handles[n]->_get_handle();
      auto &h = h_.native_handle().is_third_party_pointer() ? *(handle *) h_.native_handle().ptr : h_;
      if(h.is_kernel_handle())
      {
        fds[fdscount].fd = h.native_handle().fd;
        if(query[n] & poll_what::is_readable)
        {
          fds[fdscount].events |= POLLIN;
        }
        if(query[n] & poll_what::is_writable)
        {
          fds[fdscount].events |= POLLOUT;
        }
        fdscount++;
      }
    }
  }
  if(0 == fdscount)
  {
    return 0;
  }
  for(;;)
  {
    int timeout = -1;
    if(d)
    {
      std::chrono::milliseconds ms;
      if(d.steady)
      {
        ms = std::chrono::duration_cast<std::chrono::milliseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());
      }
      else
      {
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(d.to_time_point() - std::chrono::system_clock::now());
      }
      if(ms.count() < 0)
      {
        timeout = 0;
      }
      else
      {
        timeout = (int) ms.count();
      }
    }
    auto ret = ::poll(fds, fdscount, timeout);
    if(-1 == ret)
    {
      return posix_error();
    }
    if(ret > 0)
    {
      auto count = ret;
      fdscount = 0;
      for(size_t n = 0; n < handles.size(); n++)
      {
        if(handles[n] != nullptr)
        {
          auto &h_ = handles[n]->_get_handle();
          auto &h = h_.native_handle().is_third_party_pointer() ? *(handle *) h_.native_handle().ptr : h_;
          if(h.is_kernel_handle())
          {
            if(fds[fdscount].revents != 0)
            {
              if(fds[fdscount].revents & POLLIN)
              {
                out[n] |= poll_what::is_readable;
              }
              if(fds[fdscount].revents & POLLOUT)
              {
                out[n] |= poll_what::is_writable;
              }
              if(fds[fdscount].revents & POLLERR)
              {
                out[n] |= poll_what::is_errored;
              }
              if(fds[fdscount].revents & POLLHUP)
              {
                out[n] |= poll_what::is_closed;
              }
              if(--count == 0)
              {
                return ret;
              }
            }
          }
          else
          {
            out[n] |= poll_what::not_pollable;
          }
          fdscount++;
        }
      }
      return ret;
    }
    LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
  }
}
#endif

LLFIO_V2_NAMESPACE_END
