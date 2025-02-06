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

#if LLFIO_EXPERIMENTAL_STATUS_CODE
#if !OUTCOME_USE_SYSTEM_STATUS_CODE && __has_include("outcome/experimental/status-code/include/status-code/getaddrinfo_code.hpp")
#include "outcome/experimental/status-code/include/status-code/getaddrinfo_code.hpp"
#else
#include <status-code/getaddrinfo_code.hpp>
#endif
#else
#include "../getaddrinfo_category.hpp"
#endif

#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>

#if !defined(SOCK_CLOEXEC) || !defined(SOCK_NONBLOCK)
#include <fcntl.h>
#endif

#ifndef LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
/* Benchmarking shows that getaddrinfo_a() is implemented using a thread pool.
Therefore there seems no benefit to drag in an unnecessary extra library
dependency if we already have a local thread pool based implementation.
*/
//#ifdef __linux__
//#define LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO 1
//#else
#define LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO 0
//#endif
#endif

#include <string>
#include <vector>

#if !LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
#include <future>  // for std::async
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace ip
{
  namespace detail
  {
    struct resolver_impl : resolver
    {
      std::string name, service;
      std::chrono::steady_clock::time_point deadline_relative;
      std::chrono::system_clock::time_point deadline_absolute;
      std::vector<address> addresses;
      ::addrinfo hints;

      resolver_impl(string_view _name, string_view _service)
          : name(_name)
          , service(_service)
      {
        memset(&hints, 0, sizeof(hints));
      }

#if LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
      ::gaicb _req, *req{&_req};
      std::pair<int, int> get()
      {
        auto retcode = gai_error(req);
        if(retcode != 0)
        {
          return {retcode, errno};
        }
        auto unaddrinfo = make_scope_exit([&]() noexcept {
          ::freeaddrinfo(req->ar_result);
          req->ar_result = nullptr;
        });
        addresses.reserve(4);
        for(auto *p = req->ar_result; p != nullptr; p = p->ai_next)
        {
          if(p->ai_socktype == SOCK_STREAM)
          {
            address a;
            switch(p->ai_family)
            {
            default:
              break;  // ignore
            case AF_INET:
              assert(p->ai_addrlen <= sizeof(address));
              memcpy(const_cast<::sockaddr *>(a.to_sockaddr()), p->ai_addr, p->ai_addrlen);
              assert(a.is_v4());
              addresses.push_back(a);
              break;
            case AF_INET6:
              assert(p->ai_addrlen <= sizeof(address));
              memcpy(const_cast<::sockaddr *>(a.to_sockaddr()), p->ai_addr, p->ai_addrlen);
              assert(a.is_v6());
              addresses.push_back(a);
              break;
            }
          }
        }
        return {0, 0};
      }
      ~resolver_impl()
      {
        if(req->ar_result != nullptr)
        {
          ::freeaddrinfo(req->ar_result);
        }
      }
#else
      std::future<std::pair<int, int>> task;
      mutable std::mutex lock;

      int status{0};
      ::addrinfo *res{nullptr};

      // Runs in a different thread
      std::pair<int, int> invoke()
      {
        auto retcode = ::getaddrinfo(!name.empty() ? name.c_str() : nullptr, !service.empty() ? service.c_str() : nullptr, &hints, &res);
        if(retcode != 0)
        {
          return {retcode, errno};
        }
        auto unaddrinfo = make_scope_exit([&]() noexcept {
          ::freeaddrinfo(res);
          res = nullptr;
        });
        std::lock_guard<std::mutex> g(lock);
        if(status == -1)
        {
          // We have been abandoned
          std::thread t([this] {
            task.wait();
            delete this;
          });
          t.detach();
          return {EAI_SYSTEM, (int) errc::operation_canceled};
        }
        status = 1;
        addresses.reserve(4);
        for(auto *p = res; p != nullptr; p = p->ai_next)
        {
          if(p->ai_socktype == SOCK_STREAM)
          {
            address a;
            switch(p->ai_family)
            {
            default:
              break;  // ignore
            case AF_INET:
              assert(p->ai_addrlen <= sizeof(address));
              memcpy(const_cast<::sockaddr *>(a.to_sockaddr()), p->ai_addr, p->ai_addrlen);
              assert(a.is_v4());
              addresses.push_back(a);
              break;
            case AF_INET6:
              assert(p->ai_addrlen <= sizeof(address));
              memcpy(const_cast<::sockaddr *>(a.to_sockaddr()), p->ai_addr, p->ai_addrlen);
              assert(a.is_v6());
              addresses.push_back(a);
              break;
            }
          }
        }
        return {0, 0};
      }
      ~resolver_impl()
      {
        if(res != nullptr)
        {
          ::freeaddrinfo(res);
        }
      }
#endif

      // -1 for bounded limit passed, +1 for d passed, 0 for it's ready
      int wait(deadline d)
      {
        std::chrono::nanoseconds diff;
        int is_timedout = 0;
        if(deadline_relative != std::chrono::steady_clock::time_point())
        {
          diff = deadline_relative - std::chrono::steady_clock::now();
          is_timedout = 1;
        }
        else if(deadline_absolute != std::chrono::system_clock::time_point())
        {
          diff = deadline_absolute - std::chrono::system_clock::now();
          is_timedout = 1;
        }
        if(d)
        {
          if(d.steady)
          {
            if(std::chrono::nanoseconds(d.nsecs) < diff)
            {
              diff = std::chrono::nanoseconds(d.nsecs);
              is_timedout = 2;
            }
          }
          else
          {
            auto diff2 = std::chrono::system_clock::now() - d.to_time_point();
            if(diff2 < diff)
            {
              diff = diff2;
              is_timedout = 2;
            }
          }
        }
#if LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
        ::timespec *timeout = nullptr, _timeout;
        if(is_timedout != 0)
        {
          if(diff < std::chrono::seconds(0))
          {
            if(gai_error(req) == EAI_INPROGRESS)
            {
              gai_cancel(req);  // note this can fail
            }
            if(is_timedout == 1)
            {
              return -1;
            }
            return 1;
          }
          _timeout.tv_sec = (long) (diff.count() / 1000000000ULL);
          _timeout.tv_nsec = (long) (diff.count() % 1000000000ULL);
          timeout = &_timeout;
        }
        auto errcode = gai_suspend(&req, 1, timeout);
        if(errcode != EAI_ALLDONE)
        {
          if(is_timedout == 1)
          {
            return -1;
          }
          return 1;
        }
#else
        if(!task.valid())
        {
          return 0;
        }
        if(is_timedout == 0)
        {
          task.wait();
        }
        else
        {
          if(diff < std::chrono::seconds(0) || std::future_status::timeout == task.wait_for(diff))
          {
            if(is_timedout == 1)
            {
              return -1;
            }
            return 1;
          }
        }
#endif
        return 0;
      }
    };
    void resolver_deleter::operator()(resolver *_p) const
    {
      auto *p = static_cast<resolver_impl *>(_p);
#if LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
      while(gai_error(p->req) == EAI_INPROGRESS)
      {
        gai_cancel(p->req);  // note this can fail
      }
#else
      if(p->task.valid() && std::future_status::timeout == p->task.wait_for(std::chrono::seconds(0)))
      {
        std::lock_guard<std::mutex> g(p->lock);
        if(p->status == 0)
        {
          p->status = -1;  // abandoned
          return;
        }
      }
#endif
      delete p;
    }
  }  // namespace detail

  const std::string &resolver::name() const noexcept
  {
    auto *self = static_cast<const detail::resolver_impl *>(this);
    return self->name;
  }
  const std::string &resolver::service() const noexcept
  {
    auto *self = static_cast<const detail::resolver_impl *>(this);
    return self->service;
  }
  bool resolver::incomplete() const noexcept
  {
    auto *self = static_cast<const detail::resolver_impl *>(this);
#if LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
    return gai_error(self->req) == EAI_INPROGRESS;
#else
    std::lock_guard<std::mutex> g(self->lock);
    return self->status != 1;
#endif
  }
  result<span<address>> resolver::get() noexcept
  {
    auto *self = static_cast<detail::resolver_impl *>(this);
    auto timedout = self->wait({});
    std::pair<int, int> retcode{0, 0};
    switch(timedout)
    {
    case -1:  // bounded limit passed
      return errc::operation_canceled;
    case 0:  // ready
#if LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
      retcode = self->get();
#else
      if(self->task.valid())
        retcode = self->task.get();
#endif
      break;
    case 1:  // deadline passed
      abort();
    }
#if !LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
    std::lock_guard<std::mutex> g(self->lock);
#endif
    if(retcode.first != 0)
    {
      if(retcode.first == EAI_SYSTEM)
      {
        return posix_error(retcode.second);
      }
      else
      {
#if LLFIO_EXPERIMENTAL_STATUS_CODE
        return SYSTEM_ERROR2_NAMESPACE::getaddrinfo_code(retcode.first);
#else
        return failure(std::error_code(retcode.first, getaddrinfo_category()));
#endif
      }
    }
    return span<address>(self->addresses);
  }
  result<void> resolver::wait(deadline d) noexcept
  {
    auto *self = static_cast<detail::resolver_impl *>(this);
    self->wait(d);
    return success();
  }

  result<resolver_ptr> resolve(string_view name, string_view service, family _family, deadline d, resolve_flag flags) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(nullptr);
    LLFIO_EXCEPTION_TRY
    {
      detail::resolver_impl *p;
      resolver_ptr ret((p = new detail::resolver_impl(name, service)));
      switch(_family)
      {
      case family::v4:
        p->hints.ai_family = AF_INET;
        break;
      case family::v6:
        p->hints.ai_family = AF_INET6;
        break;
      default:
        p->hints.ai_family = AF_UNSPEC;
        break;
      }
      if(d)
      {
        if(d.steady)
        {
          p->deadline_relative = std::chrono::steady_clock::now() + std::chrono::nanoseconds(d.nsecs);
        }
        else
        {
          p->deadline_absolute = d.to_time_point();
        }
      }
      p->hints.ai_socktype = SOCK_STREAM;
      p->hints.ai_flags = AI_ADDRCONFIG | AI_V4MAPPED;
      if(flags & resolve_flag::passive)
      {
        p->hints.ai_flags |= AI_PASSIVE;
      }
#if LLFIO_IP_ADDRESS_RESOLVER_USE_ASYNC_GETADDRINFO
      p->req = &p->_req;
      p->req->ar_name = !p->name.empty() ? p->name.c_str() : nullptr;
      p->req->ar_service = !p->service.empty() ? p->service.c_str() : nullptr;
      p->req->ar_request = &p->hints;
      p->req->ar_result = nullptr;
      auto retcode = getaddrinfo_a((!!(flags & resolve_flag::blocking) && !d) ? GAI_WAIT : GAI_NOWAIT, &p->req, 1, nullptr);
      if(retcode != 0)
      {
        if(retcode == EAI_SYSTEM)
        {
          return posix_error();
        }
        else
        {
#if LLFIO_EXPERIMENTAL_STATUS_CODE
          return SYSTEM_ERROR2_NAMESPACE::getaddrinfo_code(retcode);
#else
          return failure(std::error_code(retcode, getaddrinfo_category()));
#endif
        }
      }
      if(!!(flags & resolve_flag::blocking) && d)
      {
        p->wait({});
      }
#else
      if(!!(flags & resolve_flag::blocking) && !d)
      {
        auto retcode = p->invoke();
        if(retcode.first != 0)
        {
          if(retcode.first == EAI_SYSTEM)
          {
            return posix_error(retcode.second);
          }
          else
          {
#if LLFIO_EXPERIMENTAL_STATUS_CODE
            return SYSTEM_ERROR2_NAMESPACE::getaddrinfo_code(retcode.first);
#else
            return failure(std::error_code(retcode.first, getaddrinfo_category()));
#endif
          }
        }
      }
      else
      {
        p->task = std::async(std::launch::async, &detail::resolver_impl::invoke, p);
        if(flags & resolve_flag::blocking)
        {
          p->wait({});
        }
      }
#endif
      return {std::move(ret)};
    }
    LLFIO_EXCEPTION_CATCH_ALL
    {
      return error_from_exception();
    }
  }

  result<size_t> resolve_trim_cache(size_t /*unused*/) noexcept { return 0; }
}  // namespace ip

namespace detail
{
  inline result<void> create_socket(native_handle_type &nativeh, ip::family _family, handle::mode _mode, handle::caching _caching, handle::flag flags) noexcept
  {
    flags &= ~handle::flag::unlink_on_first_close;
    nativeh.behaviour |= native_handle_type::disposition::socket | native_handle_type::disposition::kernel_handle;
    OUTCOME_TRY(attribs_from_handle_mode_caching_and_flags(nativeh, _mode, handle::creation::if_needed, _caching, flags));
    nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable

    const unsigned short family = (_family == ip::family::v6) ? AF_INET6 : ((_family == ip::family::v4) ? AF_INET : 0);
    nativeh.fd = ::socket(family,
                          SOCK_STREAM
#ifdef SOCK_CLOEXEC
                          | SOCK_CLOEXEC
#endif
#ifdef SOCK_NONBLOCK
                          | ((flags & handle::flag::multiplexable) ? SOCK_NONBLOCK : 0)
#endif
                          ,
                          IPPROTO_TCP);
    if(nativeh.fd == -1)
    {
      return posix_error();
    }
#ifndef SOCK_CLOEXEC
    // Not FD_CLOEXEC as it's only MacOS that doesn't implement SOCK_CLOEXEC, and its F_SETFD requires bit 0.
    if(-1 == ::fcntl(nativeh.fd, F_SETFD, 1))
    {
      return posix_error();
    }
#endif
#ifndef SOCK_NONBLOCK
    if(flags & handle::flag::multiplexable)
    {
      if(-1 == ::fcntl(nativeh.fd, F_SETFL, O_NONBLOCK))
      {
        return posix_error();
      }
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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> byte_socket_handle::_do_connect(const ip::address &addr, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(d && !_v.is_nonblocking())
  {
    return errc::not_supported;
  }
  LLFIO_DEADLINE_TO_SLEEP_INIT(d);
  if(!(_v.behaviour & native_handle_type::disposition::_is_connected))
  {
    if(-1 == ::connect(_v.fd, addr.to_sockaddr(), addr.sockaddrlen()))
    {
      auto retcode = errno;
      if(retcode != EINPROGRESS && retcode != EAGAIN)
      {
        return posix_error(retcode);
      }
    }
    _v.behaviour |= native_handle_type::disposition::_is_connected;
  }
  if(_v.is_nonblocking())
  {
    for(;;)
    {
      pollfd writefds;
      writefds.fd = _v.fd;
      writefds.events = POLLOUT | POLLERR | POLLHUP;
      writefds.revents = 0;
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
      if(-1 == ::poll(&writefds, 1, timeout))
      {
        return posix_error();
      }
      if(writefds.revents & (POLLERR | POLLHUP))
      {
        return errc::connection_refused;
      }
      if(writefds.revents & POLLOUT)
      {
        break;
      }
      LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
    }
  }
  if(!is_writable())
  {
    OUTCOME_TRY(shutdown(shutdown_write));
  }
  else if(!is_readable())
  {
    OUTCOME_TRY(shutdown(shutdown_read));
  }
  return success();
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle> byte_socket_handle::byte_socket(ip::family family, mode _mode, caching _caching, flag flags) noexcept
{
  result<byte_socket_handle> ret(byte_socket_handle(native_handle_type(), flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  OUTCOME_TRY(detail::create_socket(nativeh, family, _mode, _caching, flags));
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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> listening_byte_socket_handle::local_endpoint() const noexcept
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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> listening_byte_socket_handle::bind(const ip::address &addr, creation _creation, int backlog) noexcept
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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_byte_socket_handle> listening_byte_socket_handle::listening_byte_socket(ip::family family, mode _mode, caching _caching,
                                                                                                          flag flags) noexcept
{
  result<listening_byte_socket_handle> ret(listening_byte_socket_handle(native_handle_type(), flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(detail::create_socket(nativeh, family, _mode, _caching, flags));
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_byte_socket_handle::buffers_type> listening_byte_socket_handle::_do_read(io_request<buffers_type> req,
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
  nativeh.behaviour |= native_handle_type::disposition::socket | native_handle_type::disposition::kernel_handle;
  OUTCOME_TRY(attribs_from_handle_mode_caching_and_flags(nativeh, _mode, handle::creation::if_needed, _caching, _.flags));
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
        timeout = (int) ms.count();
      }
      if(-1 == ::poll(&readfds, 1, timeout))
      {
        return posix_error();
      }
      if(readfds.revents & (POLLIN | POLLERR))
      {
        ready_to_accept = true;
      }
    }
    if(ready_to_accept)
    {
      socklen_t len = (socklen_t) sizeof(b.second._storage);
#ifdef __linux__
      // Linux's accept() doesn't inherit flags for some odd reason
      nativeh.fd = ::accept4(_v.fd, (sockaddr *) b.second._storage, &len, SOCK_CLOEXEC | ((_.flags & handle::flag::multiplexable) ? SOCK_NONBLOCK : 0));
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
  b.first = byte_socket_handle(nativeh, _.flags, _ctx);
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
