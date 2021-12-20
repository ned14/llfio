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

#include "../../byte_io_handle.hpp"

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
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_loopback() const noexcept
  {
    union
    {
      uint32_t val4 = htonl(INADDR_LOOPBACK);
      byte loopback4[4];
    };
    return (is_v4() && 0 == memcmp(ipv4._addr, loopback4, sizeof(ipv4._addr))) || (is_v6() && 0 == memcmp(ipv6._addr, &in6addr_loopback, sizeof(ipv4._addr)));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_multicast() const noexcept
  {
    return (is_v4() && IN_MULTICAST(ntohl(ipv4._addr_be))) || (is_v6() && ipv6._addr[0] == (byte) 0xff && ipv6._addr[1] == (byte) 0x00);
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_unspecified() const noexcept { return _family == 0; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_v4() const noexcept { return _family == AF_INET; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool address::is_v6() const noexcept { return _family == AF_INET6; }

  LLFIO_HEADERS_ONLY_FUNC_SPEC std::ostream &operator<<(std::ostream &s, const address &v)
  {
    // TODO
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4::address_v4(const bytes_type &bytes, uint16_t port) noexcept
  {
    static_assert(sizeof(*this) >= sizeof(sockaddr_in), "address is not bigger than sockaddr_in");
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

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4 address_v4::any() noexcept { return {}; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4 address_v4::loopback() noexcept { return address_v4(INADDR_LOOPBACK, 0); }

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v4> make_address_v4(string_view str) noexcept
  {
    // TODO
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6::address_v6(const bytes_type &bytes, uint16_t port, uint32_t scope_id) noexcept
  {
    static_assert(sizeof(*this) >= sizeof(sockaddr_in6), "address is not bigger than sockaddr_in6");
    static_assert(offsetof(address_v6, ipv6._addr) == offsetof(sockaddr_in6, sin6_addr), "offset of ipv6._addr is not that of sockaddr_in6.sin_addr");
    _family = AF_INET6;
    _port = port;
    ipv6._scope_id = scope_id;
    memcpy(ipv6._addr, bytes.data(), sizeof(ipv6._addr));
  }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6::bytes_type address_v6::to_bytes() const noexcept { return ipv6._addr; }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6 address_v6::any() noexcept { return {}; }
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6 address_v6::loopback() noexcept { return address_v6(bytes_type{(const byte *) &in6addr_loopback, 16}); }

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v6> make_address_v6(string_view str) noexcept
  {
    // TODO
  }
}  // namespace ip

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const ip::address &byte_socket_handle::local_endpoint() const noexcept {}
LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const ip::address &byte_socket_handle::remote_endpoint() const noexcept {}

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#ifdef _WIN32
#include "windows/byte_socket_handle.ipp"
#else
#include "posix/byte_socket_handle.ipp"
#endif
#endif
