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

#include "../../../byte_io_handle.hpp"
#include "import.hpp"

#include <WinSock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>

#include <deque>

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC std::pair<std::atomic<int>, WSAData> &socket_handle_instance_count()
  {
    static std::pair<std::atomic<int>, WSAData> v;
    return v;
  }
  LLFIO_HEADERS_ONLY_FUNC_SPEC void register_socket_handle_instance(void *ptr) noexcept
  {
    auto &inst = socket_handle_instance_count();
    auto prev = inst.first.fetch_add(1, std::memory_order_relaxed);
    (void) ptr;
    // std::cout << "+ " << ptr << " count = " << prev << std::endl;
    assert(prev >= 0);
    if(prev == 0)
    {
      int retcode = WSAStartup(MAKEWORD(2, 2), &inst.second);
      if(retcode != 0)
      {
        LLFIO_LOG_FATAL(nullptr, "FATAL: Failed to initialise Winsock!");
        abort();
      }
    }
  }
  LLFIO_HEADERS_ONLY_FUNC_SPEC void unregister_socket_handle_instance(void *ptr) noexcept
  {
    auto &inst = socket_handle_instance_count();
    auto prev = inst.first.fetch_sub(1, std::memory_order_relaxed);
    (void) ptr;
    // std::cout << "- " << ptr << " count = " << prev << std::endl;
    assert(prev >= 0);
    if(prev == 1)
    {
      int retcode = WSACleanup();
      if(retcode != 0)
      {
        LLFIO_LOG_FATAL(nullptr, "FATAL: Failed to deinitialise Winsock!");
        abort();
      }
    }
  }
}  // namespace detail

namespace ip
{
  namespace detail
  {
    struct resolver_impl : resolver
    {
      std::string name, service;
      std::vector<address> addresses;
      ::ADDRINFOEXW hints;
      OVERLAPPED ol;
      ::ADDRINFOEXW *res{nullptr};
      HANDLE ophandle{nullptr};
      bool done{false};

      resolver_impl() { clear(); }

      void clear()
      {
        name.clear();
        service.clear();
        addresses.clear();
        memset(&hints, 0, sizeof(hints));
        HANDLE event = ol.hEvent;
        memset(&ol, 0, sizeof(ol));
        ResetEvent(event);
        ol.hEvent = event;
        if(res != nullptr)
        {
          FreeAddrInfoExW(res);
          res = nullptr;
        }
        ophandle = nullptr;
        done = false;
      }

      // Returns 0 for not ready yet, -1 for already processed, +1 for just processed
      int check(DWORD millis)
      {
        if(!done && 0 != WaitForSingleObject(ol.hEvent, millis))
        {
          return 0;
        }
        if(!res)
        {
          return -1;
        }
        done = true;
        auto unaddrinfo = make_scope_exit(
        [&]() noexcept
        {
          FreeAddrInfoExW(res);
          res = nullptr;
        });
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
        return 1;
      }
      ~resolver_impl()
      {
        if(ol.hEvent != nullptr)
        {
          CloseHandle(ol.hEvent);
        }
        if(res != nullptr)
        {
          FreeAddrInfoExW(res);
        }
      }
    };
    struct resolver_impl_cache_t
    {
      std::mutex lock;
      std::deque<resolver_impl *> list;
      resolver_impl_cache_t() { LLFIO_V2_NAMESPACE::detail::register_socket_handle_instance(nullptr); }
      ~resolver_impl_cache_t()
      {
        for(auto *i : list)
        {
          delete i;
        }
        list.clear();
        LLFIO_V2_NAMESPACE::detail::unregister_socket_handle_instance(nullptr);
      }
    };
    LLFIO_HEADERS_ONLY_FUNC_SPEC resolver_impl_cache_t &resolver_impl_cache()
    {
      static resolver_impl_cache_t v;
      return v;
    }
    void resolver_deleter::operator()(resolver *_p) const
    {
      auto *p = static_cast<resolver_impl *>(_p);
      if(!p->done && 0 != WaitForSingleObject(p->ol.hEvent, 0))
      {
        GetAddrInfoExCancel(&p->ophandle);
        WaitForSingleObject(p->ol.hEvent, INFINITE);
      }
      p->clear();
      auto &cache = resolver_impl_cache();
      std::lock_guard<std::mutex> g(cache.lock);
      try
      {
        cache.list.push_back(p);
      }
      catch(...)
      {
        delete p;
      }
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
    return !self->done && 0 != WaitForSingleObject(self->ol.hEvent, 0);
  }
  result<span<address>> resolver::get() noexcept
  {
    auto *self = static_cast<detail::resolver_impl *>(this);
    self->check(INFINITE);
    auto retcode = GetAddrInfoExOverlappedResult(&self->ol);
    if(retcode != NO_ERROR)
    {
      if(WSAETIMEDOUT == retcode)
      {
        return errc::operation_canceled;
      }
      return win32_error(retcode);
    }
    return span<address>(self->addresses);
  }
  result<void> resolver::wait(deadline d) noexcept
  {
    auto *self = static_cast<detail::resolver_impl *>(this);
    DWORD millis = INFINITE;
    if(d)
    {
      if(d.steady)
      {
        millis = (DWORD) (d.nsecs / 1000000);
      }
      else
      {
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - d.to_time_point()).count();
        if(diff < 0)
        {
          diff = 0;
        }
        millis = (DWORD) diff;
      }
    }
    if(0 == self->check(millis))
    {
      return errc::timed_out;
    }
    return success();
  }

  result<resolver_ptr> resolve(string_view name, string_view service, family _family, deadline d, resolve_flag flags) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(nullptr);
    try
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      resolver_ptr ret;
      detail::resolver_impl *p = nullptr;
      {
        auto &cache = detail::resolver_impl_cache();
        std::lock_guard<std::mutex> g(cache.lock);
        if(!cache.list.empty())
        {
          p = cache.list.back();
          cache.list.pop_back();
          ret = resolver_ptr(p);
        }
      }
      if(!ret)
      {
        ret = resolver_ptr((p = new detail::resolver_impl));
        p->ol.hEvent = CreateEvent(nullptr, true, false, nullptr);
        if(p->ol.hEvent == INVALID_HANDLE_VALUE)
        {
          return win32_error();
        }
      }
      p->name.assign(name.data(), name.size());
      p->service.assign(service.data(), service.size());
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
      ::timeval *timeout = nullptr, _timeout;
      memset(&_timeout, 0, sizeof(_timeout));
      if(d)
      {
        if(d.steady)
        {
          _timeout.tv_sec = (long) (d.nsecs / 1000000000ULL);
          _timeout.tv_usec = (long) ((d.nsecs / 1000ULL) % 1000000ULL);
        }
        else
        {
          auto diff = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now() - d.to_time_point()).count();
          if(diff < 0)
          {
            diff = 0;
          }
          _timeout.tv_sec = (long) (diff / 1000000);
          _timeout.tv_usec = (long) (diff % 1000000);
        }
        timeout = &_timeout;
        // Can't combine blocking and timeouts
        flags &= ~resolve_flag::blocking;
      }
      p->hints.ai_socktype = SOCK_STREAM;
      p->hints.ai_flags = AI_ADDRCONFIG;
      if(_family != family::v4)
      {
        p->hints.ai_flags |= AI_V4MAPPED;
      }
      if(flags & resolve_flag::passive)
      {
        p->hints.ai_flags |= AI_PASSIVE;
      }
      auto to_wstring = [](const std::string &str) -> result<std::wstring>
      {
        ULONG written = 0;
        // Ask for the length needed
        NTSTATUS ntstat = RtlUTF8ToUnicodeN(nullptr, 0, &written, str.c_str(), static_cast<ULONG>(str.size()));
        if(ntstat < 0)
        {
          return ntkernel_error(ntstat);
        }
        std::wstring ret;
        ret.resize(written / sizeof(wchar_t));
        written = 0;
        // Do the conversion UTF-8 to UTF-16
        ntstat = RtlUTF8ToUnicodeN(const_cast<wchar_t *>(ret.data()), static_cast<ULONG>(ret.size() * sizeof(wchar_t)), &written, str.c_str(),
                                   static_cast<ULONG>(str.size()));
        if(ntstat < 0)
        {
          return ntkernel_error(ntstat);
        }
        ret.resize(written / sizeof(wchar_t));
        return ret;
      };
      OUTCOME_TRY(auto &&_name, to_wstring(p->name));
      OUTCOME_TRY(auto &&_service, to_wstring(p->service));
      auto errcode = GetAddrInfoExW(_name.c_str(), _service.c_str(), NS_ALL, nullptr, &p->hints, &p->res, timeout,
                                    (flags & resolve_flag::blocking) ? nullptr : &p->ol, nullptr, (flags & resolve_flag::blocking) ? nullptr : &p->ophandle);
      if(NO_ERROR == errcode)
      {
        p->done = true;
      }
      else if(WSA_IO_PENDING != errcode)
      {
        p->done = true;
        return win32_error(errcode);
      }
      p->check(0);
      return {std::move(ret)};
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

  result<size_t> resolve_trim_cache(size_t maxitems) noexcept
  {
    auto &cache = detail::resolver_impl_cache();
    std::lock_guard<std::mutex> g(cache.lock);
    while(cache.list.size() > maxitems)
    {
      cache.list.pop_front();
    }
    return cache.list.size();
  }
}  // namespace ip

/********************************************************************************************************************/

namespace detail
{
  inline result<void> create_socket(void *p, native_handle_type &nativeh, ip::family _family, handle::mode _mode, handle::caching _caching,
                                    handle::flag flags) noexcept
  {
    flags &= ~handle::flag(handle::flag::unlink_on_first_close);
    nativeh.behaviour |= native_handle_type::disposition::socket | native_handle_type::disposition::kernel_handle;
    OUTCOME_TRY(access_mask_from_handle_mode(nativeh, _mode, flags));
    OUTCOME_TRY(attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
    nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
    if(_family == ip::family::v6)
    {
      nativeh.behaviour |= native_handle_type::disposition::is_alternate;
    }

    detail::register_socket_handle_instance(p);
    const unsigned short family = (_family == ip::family::v6) ? AF_INET6 : ((_family == ip::family::v4) ? AF_INET : 0);
    nativeh.sock =
    WSASocketW(family, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_NO_HANDLE_INHERIT | ((flags & handle::flag::multiplexable) ? WSA_FLAG_OVERLAPPED : 0));
    if(nativeh.sock == INVALID_SOCKET)
    {
      auto retcode = WSAGetLastError();
      detail::unregister_socket_handle_instance(p);
      return win32_error(retcode);
    }
    if(_caching < handle::caching::all)
    {
      {
        int val = 1;
        if(SOCKET_ERROR == ::setsockopt(nativeh.sock, SOL_SOCKET, SO_SNDBUF, (char *) &val, sizeof(val)))
        {
          return win32_error(WSAGetLastError());
        }
      }
      {
        BOOL val = 1;
        if(SOCKET_ERROR == ::setsockopt(nativeh.sock, IPPROTO_TCP, TCP_NODELAY, (char *) &val, sizeof(val)))
        {
          return win32_error(WSAGetLastError());
        }
      }
    }
    if(flags & handle::flag::multiplexable)
    {
      // Also set to non-blocking so reads/writes etc return partially completed, but also
      // connects and accepts do not block.
      u_long val = 1;
      if(SOCKET_ERROR == ::ioctlsocket(nativeh.sock, FIONBIO, &val))
      {
        return win32_error(WSAGetLastError());
      }
    }
    return success();
  }
}  // namespace detail

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> byte_socket_handle::local_endpoint() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  ip::address ret;
  int len = (int) sizeof(ret._storage);
  if(SOCKET_ERROR == getsockname(_v.sock, (::sockaddr *) ret._storage, &len))
  {
    return win32_error(WSAGetLastError());
  }
  return ret;
}
LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> byte_socket_handle::remote_endpoint() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  ip::address ret;
  int len = (int) sizeof(ret._storage);
  if(SOCKET_ERROR == getpeername(_v.sock, (::sockaddr *) ret._storage, &len))
  {
    return win32_error(WSAGetLastError());
  }
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> byte_socket_handle::shutdown(shutdown_kind kind) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  const int how = (kind == shutdown_write) ? SD_SEND : ((kind == shutdown_both) ? SD_BOTH : SD_RECEIVE);
  if(SOCKET_ERROR == ::shutdown(_v.sock, how))
  {
    return win32_error(WSAGetLastError());
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
    if(SOCKET_ERROR == ::connect(_v.sock, addr.to_sockaddr(), addr.sockaddrlen()))
    {
      auto retcode = WSAGetLastError();
      if(retcode != WSAEWOULDBLOCK)
      {
        return win32_error(retcode);
      }
    }
    _v.behaviour |= native_handle_type::disposition::_is_connected;
  }
  if(_v.is_nonblocking())
  {
    for(;;)
    {
      WSAPOLLFD fds;
      fds.fd = _v.sock;
      fds.events = POLLOUT;
      fds.revents = 0;
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
      auto ret = WSAPoll(&fds, 1, timeout);
      if(SOCKET_ERROR == ret)
      {
        return win32_error(WSAGetLastError());
      }
      if(fds.revents & (POLLERR | POLLHUP))
      {
        return errc::connection_refused;
      }
      if(fds.revents & POLLOUT)
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


LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle> byte_socket_handle::byte_socket(ip::family _family, mode _mode, caching _caching,
                                                                                           flag flags) noexcept
{
  result<byte_socket_handle> ret(byte_socket_handle(native_handle_type(), _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  OUTCOME_TRY(detail::create_socket(&ret.value(), nativeh, _family, _mode, _caching, flags));
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
    if(SOCKET_ERROR == ::closesocket(_v.sock))
    {
      return win32_error(WSAGetLastError());
    }
    _v = {};
    detail::unregister_socket_handle_instance(this);
  }
  return success();
}


/*******************************************************************************************************************/

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<ip::address> listening_socket_handle::local_endpoint() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  ip::address ret;
  int len = (int) sizeof(ret._storage);
  if(SOCKET_ERROR == getsockname(_v.sock, (::sockaddr *) ret._storage, &len))
  {
    return win32_error(WSAGetLastError());
  }
  return ret;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> listening_socket_handle::bind(const ip::address &addr, creation _creation, int backlog) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_creation != creation::only_if_not_exist)
  {
    BOOL val = 1;
    if(SOCKET_ERROR == ::setsockopt(_v.sock, SOL_SOCKET, SO_REUSEADDR, (char *) &val, sizeof(val)))
    {
      return win32_error(WSAGetLastError());
    }
  }
  if(SOCKET_ERROR == ::bind(_v.sock, addr.to_sockaddr(), addr.sockaddrlen()))
  {
    return win32_error(WSAGetLastError());
  }
  if(SOCKET_ERROR == ::listen(_v.sock, (backlog == -1) ? SOMAXCONN : backlog))
  {
    return win32_error(WSAGetLastError());
  }
  return success();
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_socket_handle> listening_socket_handle::listening_socket(ip::family family, mode _mode, caching _caching,
                                                                                                          flag flags) noexcept
{
  result<listening_socket_handle> ret(listening_socket_handle(native_handle_type(), _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(detail::create_socket(&ret.value(), nativeh, family, _mode, _caching, flags));
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
  nativeh.behaviour |= native_handle_type::disposition::socket | native_handle_type::disposition::kernel_handle;
  OUTCOME_TRY(access_mask_from_handle_mode(nativeh, _mode, _.flags));
  OUTCOME_TRY(attributes_from_handle_caching_and_flags(nativeh, _caching, _.flags));
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  for(;;)
  {
    bool ready_to_accept = true;
    if(d)
    {
      ready_to_accept = false;
      pollfd readfds;
      readfds.fd = _v.sock;
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
      if(SOCKET_ERROR == WSAPoll(&readfds, 1, timeout))
      {
        return win32_error(WSAGetLastError());
      }
      if(readfds.revents & POLLIN)
      {
        ready_to_accept = true;
      }
    }
    if(ready_to_accept)
    {
      int len = (int) sizeof(b.second._storage);
      nativeh.sock = WSAAccept(_v.sock, (sockaddr *) b.second._storage, &len, nullptr, 0);
      if(INVALID_SOCKET != nativeh.sock)
      {
        break;
      }
      auto retcode = WSAGetLastError();
      if(WSAEWOULDBLOCK != retcode)
      {
        return win32_error(retcode);
      }
    }
    LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
  }
  nativeh.behaviour |= native_handle_type::disposition::_is_connected;
  if(_caching < caching::all)
  {
    {
      int val = 1;
      if(SOCKET_ERROR == ::setsockopt(nativeh.sock, SOL_SOCKET, SO_SNDBUF, (char *) &val, sizeof(val)))
      {
        return win32_error(WSAGetLastError());
      }
    }
    {
      BOOL val = 1;
      if(SOCKET_ERROR == ::setsockopt(nativeh.sock, IPPROTO_TCP, TCP_NODELAY, (char *) &val, sizeof(val)))
      {
        return win32_error(WSAGetLastError());
      }
    }
  }
  b.first = byte_socket_handle(nativeh, _caching, _.flags, _ctx);
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
