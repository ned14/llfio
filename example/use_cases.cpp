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

void read_entire_file1()
{
  //! [file_entire_file1]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the file for read
  llfio::file_handle fh = llfio::file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // Make a vector sized the current maximum extent of the file
  std::vector<llfio::byte> buffer(fh.maximum_extent().value());

  // Synchronous scatter read from file
  llfio::file_handle::size_type bytesread = llfio::read(
    fh,                                 // handle to read from
    0,                                  // offset
    {{ buffer.data(), buffer.size() }}  // Single scatter buffer of the vector 
                                        // default deadline is infinite
  ).value();                            // If failed, throw a filesystem_error exception

  // In case of racy truncation of file by third party to new length, adjust buffer to
  // bytes actually read
  buffer.resize(bytesread);
  //! [file_entire_file1]
}

void scatter_write()
{
  /* WARNING: This example cannot possibly work because files opened with caching::only_metadata
  are required by the operating system to be supplied with buffers aligned to, and be a multiple of,
  the device's native block size (often 4Kb). So the gather buffers below would need to be each 4Kb
  long, and aligned to 4Kb boundaries. If you would like this example to work as-is, change the
  caching::only_metadata to caching::all.
  */
  return;
  //! [scatter_write]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the file for write, creating if needed, don't cache reads nor writes
  llfio::file_handle fh = llfio::file(  //
    {},                                         // path_handle to base directory
    "hello",                                    // path_view to path fragment relative to base directory
    llfio::file_handle::mode::write,             // write access please
    llfio::file_handle::creation::if_needed,     // create new file if needed
    llfio::file_handle::caching::only_metadata   // cache neither reads nor writes of data on this handle
                                                // default flags is none
  ).value();                                    // If failed, throw a filesystem_error exception

  // Empty file
  fh.truncate(0).value();

  // Perform gather write
  const char a[] = "hel";
  const char b[] = "l";
  const char c[] = "lo w";
  const char d[] = "orld";

  fh.write(0,                // offset
    {                        // gather list, buffers use std::byte
      { reinterpret_cast<const llfio::byte *>(a), sizeof(a) - 1 },
      { reinterpret_cast<const llfio::byte *>(b), sizeof(b) - 1 },
      { reinterpret_cast<const llfio::byte *>(c), sizeof(c) - 1 },
      { reinterpret_cast<const llfio::byte *>(d), sizeof(d) - 1 },
    }
                            // default deadline is infinite
  ).value();                // If failed, throw a filesystem_error exception

  // Explicitly close the file rather than letting the destructor do it
  fh.close().value();
  //! [scatter_write]
}

void malloc1()
{
  //! [malloc1]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Call whatever the equivalent to mmap() is on this platform to fetch
  // new private memory backed by the swap file. This will be the system
  // all bits zero page mapped into each page of the allocation. Only on
  // first write will a page fault allocate a real zeroed page for that
  // page.
  llfio::map_handle mh = llfio::map(4096).value();

  // Fill the newly allocated memory with 'a' C style. For each first write
  // to a page, it will be page faulted into a private page by the kernel.
  llfio::byte *p = mh.address();
  size_t len = mh.length();
  // map_handle::address() returns indeterminate bytes, so you need to bless
  // them into existence before use
  new(p) llfio::byte[len];
  memset(p, 'a', len);

  // Tell the kernel to throw away the contents of any whole pages
  // by resetting them to the system all zeros page. These pages
  // will be faulted into existence on first write.
  mh.zero_memory({ mh.address(), mh.length() }).value();

  // Do not write these pages to the swap file (flip dirty bit to false)
  mh.do_not_store({mh.address(), mh.length()}).value();

  // Fill the memory with 'b' C++ style, probably faulting new pages into existence
  llfio::attached<char> p2(mh);
  std::fill(p2.begin(), p2.end(), 'b');

  // Kick the contents of the memory out to the swap file so it is no longer cached in RAM
  // This also remaps the memory to reserved address space.
  mh.decommit({mh.address(), mh.length()}).value();

  // Map the swap file stored edition back into memory, it will fault on
  // first read to do the load back into the kernel page cache.
  mh.commit({ mh.address(), mh.length() }).value();

  // And rather than wait until first page fault read, tell the system we are going to
  // use this region soon. Most systems will begin an asynchronous population of the
  // kernel page cache immediately.
  llfio::map_handle::buffer_type pf[] = { { mh.address(), mh.length() } };
  mh.prefetch(pf).value();


  // You can actually save yourself some time and skip manually creating map handles.
  // Just construct a mapped_span directly, this creates an internal map_handle instance,
  // so memory is released when the span is destroyed
  llfio::mapped<float> f(1000);  // 1000 floats, allocated used mmap()
  std::fill(f.begin(), f.end(), 1.23f);
  //! [malloc1]
}

void malloc2()
{
  //! [malloc2]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Create 4Kb of anonymous shared memory. This will persist
  // until the last handle to it in the system is destructed.
  // You can fetch a path to it to give to other processes using
  // sh.current_path()
  llfio::section_handle sh = llfio::section(4096).value();

  {
    // Map it into memory, and fill it with 'a'
    llfio::mapped<char> ms1(sh);
    std::fill(ms1.begin(), ms1.end(), 'a');

    // Destructor unmaps it from memory
  }

  // Map it into memory again, verify it contains 'a'
  llfio::mapped<char> ms1(sh);
  assert(ms1[0] == 'a');

  // Map a *second view* of the same memory
  llfio::mapped<char> ms2(sh);
  assert(ms2[0] == 'a');

  // The addresses of the two maps are unique
  assert(ms1.data() != ms2.data());

  // Yet writes to one map appear in the other map
  ms2[0] = 'b';
  assert(ms1[0] == 'b');
  //! [malloc2]
}

void map_file()
{
  //! [map_file]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the file for read
  llfio::file_handle rfh = llfio::file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // Open the same file for atomic append
  llfio::file_handle afh = llfio::file(  //
    {},                               // path_handle to base directory
    "foo",                            // path_view to path fragment relative to base directory
    llfio::file_handle::mode::append   // open for atomic append
                                      // default creation is open existing
                                      // default caching is all
                                      // default flags is none
  ).value();                          // If failed, throw a filesystem_error exception

  // Create a section for the file of exactly the current length of the file
  llfio::section_handle sh = llfio::section(rfh).value();

  // Map the end of the file into memory with a 1Mb address reservation
  llfio::map_handle mh = llfio::map(sh, 1024 * 1024, sh.length().value() & ~4095).value();

  // Append stuff to append only handle
  llfio::write(afh,
    0,                                                      // offset is ignored for atomic append only handles
    {{ reinterpret_cast<const llfio::byte *>("hello"), 6 }}  // single gather buffer
                                                            // default deadline is infinite
  ).value();

  // Poke map to update itself into its reservation if necessary to match its backing
  // file, bringing the just appended text into the map. A no-op on many platforms.
  size_t length = mh.update_map().value();

  // Find my appended text
  for (char *p = reinterpret_cast<char *>(mh.address()); (p = (char *) memchr(p, 'h', reinterpret_cast<char *>(mh.address()) + length - p)); p++)
  {
    if (strcmp(p, "hello"))
    {
      std::cout << "Happy days!" << std::endl;
    }
  }
  //! [map_file]
}

void mapped_file()
{
  //! [mapped_file]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the mapped file for read
  llfio::mapped_file_handle mh = llfio::mapped_file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  auto length = mh.maximum_extent().value();

  // Find my text
  for (char *p = reinterpret_cast<char *>(mh.address()); (p = (char *)memchr(p, 'h', reinterpret_cast<char *>(mh.address()) + length - p)); p++)
  {
    if (strcmp(p, "hello"))
    {
      std::cout << "Happy days!" << std::endl;
    }
  }
  //! [mapped_file]
}

#if !defined(__SIZE_MAX__) || __SIZE_MAX__ > 999999999999UL
void sparse_array()
{
  //! [sparse_array]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Make me a 1 trillion element sparsely allocated integer array!
  llfio::mapped_file_handle mfh = llfio::mapped_temp_inode().value();

  // On an extents based filing system, doesn't actually allocate any physical
  // storage but does map approximately 4Tb of all bits zero data into memory
  (void) mfh.truncate(1000000000000ULL * sizeof(int));

  // Create a typed view of the one trillion integers
  llfio::attached<int> one_trillion_int_array(mfh);

  // Write and read as you see fit, if you exceed physical RAM it'll be paged out
  one_trillion_int_array[0] = 5;
  one_trillion_int_array[999999999999ULL] = 6;
  //! [sparse_array]
}
#endif

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
          topoll[handleidx]&=~llfio::poll_what::is_writable;
        }
      }

      // Process buffers read and written for this socket ...
    }
  }
  //! [tls_socket_server]
}

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
