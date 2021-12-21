/* A handle to a byte-orientated socket
(C) 2021-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Dec 2021


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

#include "../../../byte_socket_handle.hpp"
#include "import.hpp"

#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>

#ifndef SOCK_CLOEXEC
#include <fcntl.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  inline result<void> create_socket(native_handle_type &nativeh, unsigned short family, handle::mode _mode, handle::caching _caching,
                                    handle::flag flags) noexcept
  {
    flags &= ~handle::flag::unlink_on_first_close;
    nativeh.behaviour |= native_handle_type::disposition::socket;
    OUTCOME_TRY(attribs_from_handle_mode_caching_and_flags(nativeh, _mode, handle::creation::if_needed, _caching, flags));
    nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable

    nativeh.fd = ::socket(family,
                          SOCK_STREAM |
#ifdef SOCK_CLOEXEC
                          SOCK_CLOEXEC |
#endif
                          ((flags & handle::flag::multiplexable) ? SOCK_NONBLOCK : 0),
                          IPPROTO_TCP);
    if(nativeh.fd == -1)
    {
      return posix_error();
    }
#ifndef SOCK_CLOEXEC
    // Not FD_CLOEXEC as it's only MacOS that doesn't implement SOCK_NONBLOCK, and its F_SETFD requires bit 0.
    if(-1 == ::fcntl(nativeh.fd, F_SETFD, 1))
    {
      return posix_error();
    }
#endif
    if(_caching < handle::caching::all)
    {
      {
        int val = 1;
        if(-1 == ::setsockopt(nativeh.fd, SOL_SOCKET, SO_SNDBUF, (char *) &val, sizeof(val)))
        {
          return posix_error();
        }
      }
      {
        int val = 1;
        if(-1 == ::setsockopt(nativeh.fd, IPPROTO_TCP, TCP_NODELAY, (char *) &val, sizeof(val)))
        {
          return posix_error();
        }
      }
    }
    return success();
  }
}  // namespace detail

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> byte_socket_handle::local_endpoint() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  ip::address ret;
  socklen_t len = (socklen_t) sizeof(ret._storage);
  if(-1 == getsockname(_v.fd, (::sockaddr *) ret._storage, &len))
  {
    return posix_error();
  }
  return ret;
}
LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> byte_socket_handle::remote_endpoint() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  ip::address ret;
  socklen_t len = (socklen_t) sizeof(ret._storage);
  if(-1 == getpeername(_v.fd, (::sockaddr *) ret._storage, &len))
  {
    return posix_error();
  }
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> byte_socket_handle::shutdown(shutdown_kind kind) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  const int how = (kind == shutdown_write) ? SHUT_WR : ((kind == shutdown_both) ? SHUT_RDWR : SHUT_RD);
  if(-1 == ::shutdown(_v.fd, how))
  {
    return posix_error();
  }
  return success();
}


LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle> byte_socket_handle::byte_socket(const ip::address &addr, mode _mode, caching _caching,
                                                                                           flag flags) noexcept
{
  result<byte_socket_handle> ret(byte_socket_handle(native_handle_type(), _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  OUTCOME_TRY(detail::create_socket(nativeh, addr.family(), _mode, _caching, flags));
  if(-1 == ::connect(nativeh.fd, addr.to_sockaddr(), addr.sockaddrlen()))
  {
    auto retcode = errno;
    if(retcode != EINPROGRESS && retcode != EAGAIN)
    {
      return posix_error(retcode);
    }
  }
  else
  {
    nativeh.behaviour |= native_handle_type::disposition::_is_connected;
    if(_mode == mode::read)
    {
      OUTCOME_TRY(ret.value().shutdown(shutdown_write));
    }
    else if(_mode == mode::append)
    {
      OUTCOME_TRY(ret.value().shutdown(shutdown_read));
    }
  }
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> byte_socket_handle::close() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_v)
  {
    if(are_safety_barriers_issued() && is_writable())
    {
      auto r = shutdown();
      if(r)
      {
        byte buffer[4096];
        for(;;)
        {
          OUTCOME_TRY(auto readed, read(0, {{buffer}}));
          if(readed == 0)
          {
            break;
          }
        }
      }
      else if(r.error() != errc::not_connected)
      {
        OUTCOME_TRY(std::move(r));
      }
    }
    if(-1 == ::close(_v.fd))
    {
      return posix_error();
    }
    _v = {};
  }
  return success();
}


/*******************************************************************************************************************/

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> listening_socket_handle::local_endpoint() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  ip::address ret;
  socklen_t len = (socklen_t) sizeof(ret._storage);
  if(-1 == getsockname(_v.fd, (::sockaddr *) ret._storage, &len))
  {
    return posix_error();
  }
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> listening_socket_handle::bind(const ip::address &addr, creation _creation, int backlog) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_creation != creation::only_if_not_exist)
  {
    int val = 1;
    if(-1 == ::setsockopt(_v.fd, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(val)))
    {
      return posix_error();
    }
  }
  if(-1 == ::bind(_v.fd, addr.to_sockaddr(), addr.sockaddrlen()))
  {
    return posix_error();
  }
  if(-1 == ::listen(_v.fd, (backlog == -1) ? SOMAXCONN : backlog))
  {
    return posix_error();
  }
  return success();
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_socket_handle> listening_socket_handle::listening_socket(bool use_ipv6, mode _mode, caching _caching,
                                                                                                          flag flags) noexcept
{
  result<listening_socket_handle> ret(listening_socket_handle(native_handle_type(), _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(detail::create_socket(nativeh, use_ipv6 ? AF_INET6 : AF_INET, _mode, _caching, flags));
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_socket_handle::buffers_type> listening_socket_handle::_do_read(io_request<buffers_type> req,
                                                                                                                deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(req.buffers.empty())
  {
    return std::move(req.buffers);
  }
  LLFIO_DEADLINE_TO_SLEEP_INIT(d);
  mode _mode = this->is_append_only() ? mode::append : (this->is_writable() ? mode::write : mode::read);
  caching _caching = this->kernel_caching();
  auto &b = *req.buffers.begin();
  native_handle_type nativeh;
  nativeh.behaviour |= native_handle_type::disposition::socket;
  OUTCOME_TRY(attribs_from_handle_mode_caching_and_flags(nativeh, _mode, handle::creation::if_needed, _caching, _flags));
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  for(;;)
  {
    bool ready_to_accept = true;
    if(d)
    {
      ready_to_accept = false;
      pollfd readfds;
      readfds.fd = _v.fd;
      readfds.events = POLLIN;
      readfds.revents = 0;
      int timeout = -1;
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
        timeout = ms.count();
      }
      if(-1 == ::poll(&readfds, 1, timeout))
      {
        return posix_error();
      }
      if(readfds.revents & POLLIN)
      {
        ready_to_accept = true;
      }
    }
    if(ready_to_accept)
    {
      socklen_t len = (socklen_t) sizeof(b.second._storage);
#ifdef __linux__
      // Linux's accept() doesn't inherit flags for some odd reason
      nativeh.fd = ::accept4(_v.fd, (sockaddr *) b.second._storage, &len, SOCK_CLOEXEC | ((_flags & handle::flag::multiplexable) ? SOCK_NONBLOCK : 0));
#else
      nativeh.fd = ::accept(_v.fd, (sockaddr *) b.second._storage, &len);
#endif
      if(-1 != nativeh.fd)
      {
        break;
      }
      auto retcode = errno;
      if(EAGAIN != retcode && EWOULDBLOCK != retcode)
      {
        return posix_error(retcode);
      }
    }
    LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
  }
  nativeh.behaviour |= native_handle_type::disposition::_is_connected;
  if(_caching < caching::all)
  {
    {
      int val = 1;
      if(-1 == ::setsockopt(nativeh.fd, SOL_SOCKET, SO_SNDBUF, (char *) &val, sizeof(val)))
      {
        return posix_error();
      }
    }
    {
      int val = 1;
      if(-1 == ::setsockopt(nativeh.fd, IPPROTO_TCP, TCP_NODELAY, (char *) &val, sizeof(val)))
      {
        return posix_error();
      }
    }
  }
  b.first = byte_socket_handle(nativeh, _caching, _flags, _ctx);
  if(_mode == mode::read)
  {
    OUTCOME_TRY(b.first.shutdown(byte_socket_handle::shutdown_write));
  }
  else if(_mode == mode::append)
  {
    OUTCOME_TRY(b.first.shutdown(byte_socket_handle::shutdown_read));
  }
  return std::move(req.buffers);
}

LLFIO_V2_NAMESPACE_END
