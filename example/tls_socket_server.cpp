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

#ifndef LLFIO_EXCLUDE_NETWORKING
void tls_socket_server()
{
  //! [tls_socket_server]
  namespace llfio = LLFIO_V2_NAMESPACE;
  std::string host;  // set to one of my network cards

  // Get me a FIPS 140 compliant source of TLS sockets which uses kernel
  // sockets, this call takes bits set and bits masked, so the implementation
  // features bitfield is masked for the FIPS 140 bit, and only if that is set
  // is the TLS socket source considered.
  //
  // The need for a kernel sockets based TLS implementation is so poll() will
  // work, as it requires kernel sockets compatible input.
  static constexpr auto required_features =
    llfio::tls_socket_source_implementation_features::FIPS_140_2
    | llfio::tls_socket_source_implementation_features::kernel_sockets;
  llfio::tls_socket_source_ptr tls_socket_source =
    llfio::tls_socket_source_registry::default_source(required_features,
      required_features).instantiate().value();

  // Create a listening TLS socket on any IP family.
  llfio::listening_tls_socket_handle_ptr serversocket = tls_socket_source
    ->multiplexable_listening_socket(llfio::ip::family::any).value();

  // The default is to NOT authenticate the TLS certificates of those connecting
  // in with the system's TLS certificate store. If you wanted something
  // different, you'd set that now using:
  // serversocket->set_authentication_certificates_path()

  // Bind the listening TLS socket to port 8989 on the NIC described by host
  // ip::address incidentally is inspired by ASIO's class, it is very
  // similar. I only made it more constexpr.
  serversocket->bind(llfio::ip::make_address(host+":8989").value()).value();

  // poll() works on three spans. We deliberately have separate poll_what
  // storage so we can avoid having to reset topoll's contents per poll()
  std::vector<llfio::pollable_handle *> pollable_handles;
  std::vector<llfio::poll_what> topoll, whatchanged;

  // We will want to know about new inbound connections to our listening
  // socket
  pollable_handles.push_back(serversocket.get());
  topoll.push_back(llfio::poll_what::is_readable);
  whatchanged.push_back(llfio::poll_what::none);

  // Connected socket state
  struct connected_socket {
    // The connected socket
    llfio::tls_socket_handle_ptr socket;

    // The endpoint from which it connected
    llfio::ip::address endpoint;

    // The size of registered buffer actually allocated (we request system
    // page size, the DMA controller may return that or more)
    size_t rbuf_storage_length{llfio::utils::page_size()};
    size_t wbuf_storage_length{llfio::utils::page_size()};

    // These are shared ptrs to memory potentially registered with the NIC's
    // DMA controller and therefore i/o using them would be true whole system
    // zero copy
    llfio::tls_socket_handle::registered_buffer_type rbuf_storage, wbuf_storage;

    // These spans of byte and const byte index into buffer storage and get
    // updated by each i/o operation to describe what was done
    llfio::tls_socket_handle::buffer_type rbuf;
    llfio::tls_socket_handle::const_buffer_type wbuf;

    explicit connected_socket(std::pair<llfio::tls_socket_handle_ptr, llfio::ip::address> s)
      : socket(std::move(s.first))
      , endpoint(std::move(s.second))
      , rbuf_storage(socket->allocate_registered_buffer(rbuf_storage_length).value())
      , wbuf_storage(socket->allocate_registered_buffer(wbuf_storage_length).value())
      , rbuf(*rbuf_storage)            // fill this buffer
      , wbuf(wbuf_storage->data(), 0)  // nothing to write
      {}
  };
  std::vector<connected_socket> connected_sockets;

  // Begin the processing loop
  for(;;) {
    // We need to clear whatchanged before use, as it accumulates bits set.
    std::fill(whatchanged.begin(), whatchanged.end(), llfio::poll_what::none);

    // As with all LLFIO calls, you can set a deadline here so this call
    // times out. The default as usual is wait until infinity.
    size_t handles_changed
      = llfio::poll(whatchanged, {pollable_handles}, topoll).value();

    // Loop the polled handles looking for events changed.
    for(size_t handleidx = 0;
      handles_changed > 0 && handleidx < pollable_handles.size();
      handleidx++) {
      if(whatchanged[handleidx] == llfio::poll_what::none) {
        continue;
      }
      if(0 == handleidx) {
        // This is the listening socket, and there is a new connection to be
        // read, as listening socket defines its read operation to be
        // connected sockets and the endpoint they connected from.
        std::pair<llfio::tls_socket_handle_ptr, llfio::ip::address> s;
        serversocket->read({s}).value();
        connected_sockets.emplace_back(std::move(s));

        // Watch this connected socket going forth for data to read or failure
        pollable_handles.push_back(connected_sockets.back().socket.get());
        topoll.push_back(llfio::poll_what::is_readable|llfio::poll_what::is_errored);
        whatchanged.push_back(llfio::poll_what::none);
        handles_changed--;
        continue;
      }
      // There has been an event on this socket
      auto &sock = connected_sockets[handleidx - 1];
      handles_changed--;
      if(whatchanged[handleidx] & llfio::poll_what::is_errored) {
        // Force it closed and ignore it from polling going forth
        sock.socket->close().value();
        sock.rbuf_storage.reset();
        sock.wbuf_storage.reset();
        pollable_handles[handleidx] = nullptr;
      }
      if(whatchanged[handleidx] & llfio::poll_what::is_readable) {
        // Set up the buffer to fill. Note the scatter buffer list
        // based API.
        sock.rbuf = *sock.rbuf_storage;
        sock.socket->read({{&sock.rbuf, 1}}).value();
        // sock.rbuf has its size adjusted to bytes read
      }
      if(whatchanged[handleidx] & llfio::poll_what::is_writable) {
        // If this was set in topoll, it means write buffers
        // were full at some point, and we are now being told
        // there is space to write some more
        if(!sock.wbuf.empty()) {
          // Take a copy of the buffer to write, as it will be
          // modified in place with what was written
          auto b(sock.wbuf);
          sock.socket->write({{&b, 1}}).value();
          // Adjust the buffer to yet to write
          sock.wbuf = {sock.wbuf.data() + b.size(), sock.wbuf.size() - b.size() };
        }
        if(sock.wbuf.empty()) {
          // Nothing more to write, so no longer poll for this
          topoll[handleidx]&=uint8_t(~llfio::poll_what::is_writable);
        }
      }

      // Process buffers read and written for this socket ...
    }
  }
  //! [tls_socket_server]
}
#endif

int main()
{
  return 0;
}
