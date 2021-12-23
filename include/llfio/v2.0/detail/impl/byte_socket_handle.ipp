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

#include "../../byte_socket_handle.hpp"

#ifdef _WIN32
#include <WinSock2.h>
#include <ws2ipdef.h>
#else
#include <netinet/ip.h>
#include <sys/socket.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace ip
{
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address::address(const ::sockaddr_in &storage) noexcept
  {
    static_assert(sizeof(_storage) >= sizeof(::sockaddr_in), "address is not bigger than sockaddr_in");
    memcpy(_storage, &storage, sizeof(storage));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address::address(const ::sockaddr_in6 &storage) noexcept
  {
    static_assert(sizeof(_storage) >= sizeof(sockaddr_in6), "address is not bigger than sockaddr_in6");
    memcpy(_storage, &storage, sizeof(storage));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_loopback() const noexcept
  {
    union
    {
      uint32_t val4 = htonl(INADDR_LOOPBACK);
      byte loopback4[4];
    };
    return (is_v4() && 0 == memcmp(ipv4._addr, loopback4, sizeof(ipv4._addr))) || (is_v6() && 0 == memcmp(ipv6._addr, &in6addr_loopback, sizeof(ipv6._addr)));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_multicast() const noexcept
  {
    return (is_v4() && IN_MULTICAST(ntohl(ipv4._addr_be))) || (is_v6() && ipv6._addr[0] == to_byte(0xff) && ipv6._addr[1] == to_byte(0x00));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_any() const noexcept
  {
    return (_family == AF_INET && 0 == ipv4._addr_be && 0 == _port) ||
           (_family == AF_INET6 && ipv6._addr[0] == to_byte(0) && ipv6._addr[1] == to_byte(0) && ipv6._addr[2] == to_byte(0) && ipv6._addr[3] == to_byte(0) &&
            ipv6._addr[4] == to_byte(0) && ipv6._addr[5] == to_byte(0) && ipv6._addr[6] == to_byte(0) && ipv6._addr[7] == to_byte(0) &&
            ipv6._addr[8] == to_byte(0) && ipv6._addr[9] == to_byte(0) && ipv6._addr[10] == to_byte(0) && ipv6._addr[11] == to_byte(0) &&
            ipv6._addr[12] == to_byte(0) && ipv6._addr[13] == to_byte(0) && ipv6._addr[14] == to_byte(0) && ipv6._addr[15] == to_byte(0) && _port == 0);
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_v4() const noexcept { return _family == AF_INET; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_v6() const noexcept { return _family == AF_INET6; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC enum family address::family() const noexcept
  {
    switch(_family)
    {
    case AF_INET:
      return family::v4;
    case AF_INET6:
      return family::v6;
    default:
      return family::unknown;
    }
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC int address::sockaddrlen() const noexcept
  {
    switch(_family)
    {
    case AF_INET:
      return sizeof(sockaddr_in);
    case AF_INET6:
      return sizeof(sockaddr_in6);
    default:
      return 0;
    }
  }

  LLFIO_HEADERS_ONLY_FUNC_SPEC std::ostream &operator<<(std::ostream &s, const address &v)
  {
    auto print_byte = [](const byte &b) -> uint16_t
    {
      auto *v = (const uint8_t *) &b;
      return *v;
    };
    switch(v._family)
    {
    case AF_INET:
      return s << print_byte(v.ipv4._addr[0]) << "." << print_byte(v.ipv4._addr[1]) << "." << print_byte(v.ipv4._addr[2]) << "." << print_byte(v.ipv4._addr[3])
               << ":" << v._port;
    case AF_INET6:
    {
      std::stringstream ss;
      ss << std::hex << ":";
      for(size_t n = 0; n < 8; n++)
      {
#ifdef __BIG_ENDIAN__
        ss << (print_byte(v.ipv6._addr[n * 2 + 0]) | (print_byte(v.ipv6._addr[n * 2 + 1]) << 8)) << ":";
#else
        ss << (print_byte(v.ipv6._addr[n * 2 + 1]) | (print_byte(v.ipv6._addr[n * 2 + 0]) << 8)) << ":";
#endif
      }
      ss << std::dec << "]" << v._port;
      std::string str(std::move(ss).str());
      string_view zeros(":0:0:0:0:0:0:0:0:");
      if(0 == string_view(str).find(zeros))
      {
        str.replace(0, zeros.size(), "::::");
      }
      else
      {
        for(size_t len = zeros.size(); len > 3; len -= 2)
        {
          auto idx = string_view(str).find(zeros.substr(0, len));
          if(idx != str.npos)
          {
            str.replace(idx, len, (idx == 0 || (str[idx + len] == ']')) ? ":::" : "::");
            break;
          }
        }
      }
      auto idx = str.rfind(":]");
      str[0] = '[';
      str[idx] = ']';
      str[idx + 1] = ':';
      return s << str;
    }
    default:
      return s << "unknown address";
    }
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4::address_v4(const bytes_type &bytes, uint16_t port) noexcept
  {
    static_assert(sizeof(_storage) >= sizeof(sockaddr_in), "address is not bigger than sockaddr_in");
    static_assert(offsetof(address_v4, ipv4._addr) == offsetof(sockaddr_in, sin_addr), "offset of ipv4._addr is not that of sockaddr_in.sin_addr");
    _family = AF_INET;
    _port = port;
    memcpy(ipv4._addr, bytes.data(), sizeof(ipv4._addr));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4::address_v4(uint_type addr, uint16_t port) noexcept
  {
    _family = AF_INET;
    _port = port;
    ipv4._addr_be = htonl(addr);
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4::bytes_type address_v4::to_bytes() const noexcept { return ipv4._addr; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4::uint_type address_v4::to_uint() const noexcept { return ntohl(ipv4._addr_be); }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4 address_v4::any() noexcept { return address_v4(0, 0); }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4 address_v4::loopback() noexcept { return address_v4(INADDR_LOOPBACK, 0); }

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v4> make_address_v4(string_view str) noexcept
  {
    address_v4 ret(0);
    auto parse = [&](auto &out, size_t &idx, char sep) -> result<void>
    {
      if(idx >= str.size())
      {
        return errc::invalid_argument;
      }
      auto idx2 = (sep != 0) ? str.find(sep, idx) : str.npos;
      if(idx2 == str.npos)
      {
        idx2 = str.size();
      }
      const char *end = str.data() + idx2;
      auto res = strtoul(str.data() + idx, (char **) &end, 10);
      if(res == ULONG_MAX || end != str.data() + idx2)
      {
        return errc::invalid_argument;
      }
      idx = idx2 + 1;
      out = static_cast<std::decay_t<decltype(out)>>(res);
      return success();
    };
    size_t idx = 0;
    OUTCOME_TRY(parse(*(uint8_t *) &ret.ipv4._addr[0], idx, '.'));
    OUTCOME_TRY(parse(*(uint8_t *) &ret.ipv4._addr[1], idx, '.'));
    OUTCOME_TRY(parse(*(uint8_t *) &ret.ipv4._addr[2], idx, '.'));
    OUTCOME_TRY(parse(*(uint8_t *) &ret.ipv4._addr[3], idx, ':'));
    OUTCOME_TRY(parse(ret._port, idx, 0));
    return ret;
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6::address_v6(const bytes_type &bytes, uint16_t port, uint32_t scope_id) noexcept
  {
    static_assert(sizeof(_storage) >= sizeof(sockaddr_in6), "address is not bigger than sockaddr_in6");
    static_assert(offsetof(address_v6, ipv6._addr) == offsetof(sockaddr_in6, sin6_addr), "offset of ipv6._addr is not that of sockaddr_in6.sin_addr");
    _family = AF_INET6;
    _port = port;
    ipv6._scope_id = scope_id;
    memcpy(ipv6._addr, bytes.data(), sizeof(ipv6._addr));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6::bytes_type address_v6::to_bytes() const noexcept { return ipv6._addr; }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6 address_v6::any() noexcept { return address_v6(bytes_type{(const byte *) &in6addr_any, 16}); }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6 address_v6::loopback() noexcept { return address_v6(bytes_type{(const byte *) &in6addr_loopback, 16}); }

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v6> make_address_v6(string_view str) noexcept
  {
    address_v6 ret(address_v6::any());
    if(str.empty() || str.front() != '[')
    {
      return errc::invalid_argument;
    }
    const auto compactidx = str.find("::"), portidx = str.rfind("]:");
    if(portidx == str.npos)
    {
      return errc::invalid_argument;
    }
    auto parse = [&](size_t &idx, char sep) -> result<uint16_t>
    {
      if(idx >= str.size())
      {
        return errc::invalid_argument;
      }
      auto idx2 = str.find(sep, idx);
      if(idx2 == str.npos)
      {
        idx2 = str.size();
      }
      const char *end = str.data() + idx2;
      auto res = strtoul(str.data() + idx, (char **) &end, 16);
      if(res == ULONG_MAX || end != str.data() + idx2)
      {
        return errc::invalid_argument;
      }
      idx = idx2 + 1;
      return (uint16_t) res;
    };
    size_t idx = 1, n = 0;
    do
    {
      OUTCOME_TRY(auto res, parse(idx, (n == 14) ? ']' : ':'));
      ret.ipv6._addr[n++] = to_byte(res >> 8);
      ret.ipv6._addr[n++] = to_byte(res & 0xff);
    } while(idx < compactidx && idx < portidx);
    if(compactidx != str.npos)
    {
      auto parse2 = [&](size_t &idx) -> result<uint16_t>
      {
        auto idx2 = str.rfind(':', idx - 1);
        if(idx2 == str.npos)
        {
          return errc::invalid_argument;
        }
        const char *end = str.data() + idx;
        auto res = strtoul(str.data() + idx2 + 1, (char **) &end, 16);
        if(res == ULONG_MAX || end != str.data() + idx)
        {
          return errc::invalid_argument;
        }
        idx = idx2;
        return (uint16_t) res;
      };
      idx = portidx;
      n = 15;
      do
      {
        OUTCOME_TRY(auto res, parse2(idx));
        ret.ipv6._addr[n--] = to_byte(res & 0xff);
        ret.ipv6._addr[n--] = to_byte(res >> 8);
      } while(idx > compactidx + 1);
    }
    const char *end = str.data() + str.size();
    auto res = strtoul(str.data() + portidx + 2, (char **) &end, 10);
    if(res == ULONG_MAX || end != str.data() + str.size())
    {
      return errc::invalid_argument;
    }
    ret._port = (uint16_t) res;
    return ret;
  }
}  // namespace ip

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#ifdef _WIN32
#include "windows/byte_socket_handle.ipp"
#else
#include "posix/byte_socket_handle.ipp"
#endif
#endif
