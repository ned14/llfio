/* Examples of LLFIO use
(C) 2018 - 2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2018


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

#include "../include/llfio.hpp"

#include <future>
#include <iostream>
#include <vector>

// clang-format off
#ifdef _MSC_VER
#pragma warning(disable: 4706)  // assignment within conditional
#endif

void wrap_tls_socket()
{
  //! [wrap_tls_socket]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // I want a TLS socket source which supports wrapping externally
  // supplied byte_socket_handle instances
  static constexpr auto required_features =
    llfio::tls_socket_source_implementation_features::supports_wrap;
  llfio::tls_socket_source_ptr tls_socket_source =
    llfio::tls_socket_source_registry::default_source(required_features,
      required_features).instantiate().value();

  // Create a raw kernel socket. This could also be your custom
  // subclass of byte_socket_handle or listening_byte_socket_handle.
  // 
  // byte_socket_handle and listening_byte_socket_handle default
  // to your OS kernel's BSD socket implementation, but their
  // implementation is completely customisable. In fact, tls_socket_ptr
  // points at an unknown (i.e. ABI erased) byte_socket_handle implementation.
  llfio::byte_socket_handle rawsocket
    = llfio::byte_socket_handle::byte_socket(llfio::ip::family::any).value();

  llfio::listening_byte_socket_handle rawlsocket
    = llfio::listening_byte_socket_handle::listening_byte_socket(llfio::ip::family::any).value();

  // Attempt to wrap the raw socket with TLS. Note the "attempt" part,
  // just because a TLS implementation supports wrapping doesn't mean
  // that it will wrap this specific raw socket, it may error out.
  //
  // Note also that no ownership of the raw socket is taken - you must
  // NOT move it in memory until the TLS wrapped socket is done with it.
  llfio::tls_socket_handle_ptr securesocket
    = tls_socket_source->wrap(&rawsocket).value();

  llfio::listening_tls_socket_handle_ptr serversocket
    = tls_socket_source->wrap(&rawlsocket).value();

  //! [wrap_tls_socket]
}

int main()
{
  return 0;
}
