/* Integration test kernel for whether TLS socket handles work
(C) 2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Jan 2022


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

#include "../test_kernel_decl.hpp"

#include <future>

static inline void TestBlockingTLSSocketHandles()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  if(llfio::tls_socket_source_registry::empty())
  {
    std::cout << "\nNOTE: This platform has no TLS socket sources in its registry, skipping this test." << std::endl;
    return;
  }
  auto tls_socket_source = llfio::tls_socket_source_registry::default_source().instantiate().value();
  std::cout << "The default TLS socket source on this platform is " << tls_socket_source->implementation_information() << std::endl;
  auto runtest = [](llfio::listening_tls_socket_handle_ptr serversocket, llfio::tls_socket_handle_ptr writer)
  {
    BOOST_REQUIRE(serversocket->is_valid());
    BOOST_CHECK(serversocket->is_socket());
    BOOST_CHECK(serversocket->is_readable());
    BOOST_CHECK(serversocket->is_writable());
    // Disable authentication
    serversocket->set_authentication_certificates_path({}).value();
    writer->set_authentication_certificates_path({}).value();
    {
      auto desc = serversocket->algorithms_description();
      std::cout << "Server socket will offer during handshake the ciphers (" << (1 + std::count(desc.begin(), desc.end(), ',')) << "): ";
      std::istringstream desc2(desc);
      for(std::string s; std::getline(desc2, s, ',');)
      {
        std::cout << "\n   " << s;
      }
      std::cout << std::endl;
    }
    {
      auto desc = writer->algorithms_description();
      std::cout << "\nConnecting socket will offer during handshake the ciphers (" << (1 + std::count(desc.begin(), desc.end(), ',')) << "): ";
      std::istringstream desc2(desc);
      for(std::string s; std::getline(desc2, s, ',');)
      {
        std::cout << "\n   " << s;
      }
      std::cout << std::endl;
    }
    serversocket->bind(llfio::ip::address_v4::loopback()).value();
    auto endpoint = serversocket->local_endpoint().value();
    std::cout << "\nServer socket is listening on " << endpoint << std::endl;
    if(endpoint.family() == llfio::ip::family::unknown && getenv("CI") != nullptr)
    {
      std::cout << "\nNOTE: Currently on CI and couldn't bind a listening socket to loopback, assuming it is CI host restrictions and skipping this test."
                << std::endl;
      return;
    }
    std::mutex printlock;
    std::unique_lock<std::mutex> g(printlock);
    auto readerthread = std::async(
    [serversocket = std::move(serversocket), &printlock]() mutable
    {
      std::pair<llfio::tls_socket_handle_ptr, llfio::ip::address> s;
      serversocket->read({s}).value();  // This immediately blocks in blocking mode
      std::unique_lock<std::mutex> g(printlock);
      BOOST_REQUIRE(s.first->is_valid());
      BOOST_CHECK(s.first->is_socket());
      BOOST_CHECK(s.first->is_readable());
      BOOST_CHECK(s.first->is_writable());
      std::cout << "\nServer thread sees incoming connection from " << s.second << std::endl;
      g.unlock();
      serversocket->close().value();
      llfio::byte buffer[64];
      auto read = s.first->read({{buffer, 64}}).value();
      g.lock();
      std::cout << "\nThe inbound server socket negotiated the cipher " << s.first->algorithms_description() << std::endl;
      BOOST_REQUIRE(read == 5);
      BOOST_CHECK(0 == memcmp(buffer, "hello", 5));
      g.unlock();
      s.first->shutdown_and_close().value();
    });
    auto begin = std::chrono::steady_clock::now();
    while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() < 100)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if(std::future_status::ready == readerthread.wait_for(std::chrono::seconds(0)))
    {
      readerthread.get();  // rethrow exception
    }
    begin = std::chrono::steady_clock::now();
    while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() < 1000)
    {
      g.unlock();
      auto r = writer->connect(endpoint);
      g.lock();
      if(r)
      {
        break;
      }
    }
    BOOST_REQUIRE(writer->is_valid());
    BOOST_CHECK(writer->is_socket());
    BOOST_CHECK(writer->is_readable());
    BOOST_CHECK(writer->is_writable());
    std::cout << "\nThe connecting socket negotiated the cipher " << writer->algorithms_description() << std::endl;
    g.unlock();
    auto written = writer->write({{(const llfio::byte *) "hello", 5}}).value();
    BOOST_REQUIRE(written == 5);
    writer->shutdown_and_close().value();
    readerthread.get();
  };
  std::cout << "\nUnwrapped TLS socket:\n" << std::endl;
  runtest(tls_socket_source->listening_socket(llfio::ip::family::v4).value(), tls_socket_source->connecting_socket(llfio::ip::family::v4).value());

  std::cout << "\nWrapped TLS socket:\n" << std::endl;
  auto rawserversocket = llfio::listening_byte_socket_handle::listening_byte_socket(llfio::ip::family::v4).value();
  auto rawwriter = llfio::byte_socket_handle::byte_socket(llfio::ip::family::v4).value();
  runtest(tls_socket_source->wrap(&rawserversocket).value(), tls_socket_source->wrap(&rawwriter).value());
}

struct TestNonBlockingTLSSocketHandlesRunTest
{
  template <class F> TestNonBlockingTLSSocketHandlesRunTest(LLFIO_V2_NAMESPACE::listening_tls_socket_handle_ptr serversocket, F &&make_writer)
  {
    namespace llfio = LLFIO_V2_NAMESPACE;
    BOOST_REQUIRE(serversocket->is_valid());
    BOOST_CHECK(serversocket->is_socket());
    BOOST_CHECK(serversocket->is_readable());
    BOOST_CHECK(serversocket->is_writable());
    // Disable authentication
    serversocket->set_authentication_certificates_path({}).value();
    serversocket->bind(llfio::ip::address_v4::loopback()).value();
    auto endpoint = serversocket->local_endpoint().value();
    std::cout << "Server socket is listening on " << endpoint << std::endl;
    if(endpoint.family() == llfio::ip::family::unknown && getenv("CI") != nullptr)
    {
      std::cout << "\nNOTE: Currently on CI and couldn't bind a listening socket to loopback, assuming it is CI host restrictions and skipping this test."
                << std::endl;
      return;
    }

    std::pair<llfio::tls_socket_handle_ptr, llfio::ip::address> reader;
    {  // no incoming, so non-blocking read should time out
      auto read = serversocket->read({reader}, std::chrono::milliseconds(0));
      BOOST_REQUIRE(read.has_error());
      BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
    }
    {  // no incoming, so blocking read should time out
      auto read = serversocket->read({reader}, std::chrono::seconds(1));
      BOOST_REQUIRE(read.has_error());
      BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
    }

    // Form the connection.
    llfio::tls_socket_handle_ptr writer = make_writer();
    // Disable authentication
    writer->set_authentication_certificates_path({}).value();
    {
      auto r = writer->connect(endpoint, std::chrono::milliseconds(0));
      BOOST_REQUIRE(!r);
      BOOST_REQUIRE(r.error() == llfio::errc::timed_out);  // can't possibly connect immediately
    }
    BOOST_REQUIRE(serversocket->is_nonblocking());
    serversocket->read({reader}, std::chrono::seconds(1)).value();
    // clang seems to be losing the nonblocking bit???
    BOOST_REQUIRE(reader.first->is_nonblocking());
    std::cout << "Server socket sees incoming connection from " << reader.second << std::endl;
    // The TLS negotiation needs to do various reading and writing until the connection goes up
    bool connected = false;
    for(size_t count = 0; count < 16; count++)
    {
      auto r1 = writer->connect(endpoint, std::chrono::milliseconds(0));
      auto r2 = reader.first->read({{nullptr, 0}}, std::chrono::milliseconds(0));
      if(r1)
      {
        connected = true;
        break;
      }
      BOOST_REQUIRE(r2 || r2.error() != llfio::errc::not_supported);
      if(count == 8)
      {
        if(r2)
        {
          std::cout << "\nread() returns success " << r2.value() << std::endl;
        }
        else
        {
          std::cout << "\nread() returns failure " << r2.error().message() << std::endl;
        }
      }
      std::cout << "*" << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << std::endl;
    BOOST_REQUIRE(connected);

    llfio::byte buffer[64];
    {  // no data, so non-blocking read should time out
      auto read = reader.first->read(0, {{buffer, 64}}, std::chrono::milliseconds(0));
      BOOST_REQUIRE(read.has_error());
      std::cout << read.error().message() << std::endl;
      BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
    }
    {  // no data, so blocking read should time out
      auto read = reader.first->read(0, {{buffer, 64}}, std::chrono::seconds(1));
      if(!read.has_error())
      {
        std::cout << "Blocking read did not return error, instead returned " << read.value() << std::endl;
      }
      BOOST_REQUIRE(read.has_error());
      std::cout << read.error().message() << std::endl;
      BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
    }
    auto written = writer->write(0, {{(const llfio::byte *) "hello", 5}}).value();
    BOOST_REQUIRE(written == 5);
    // writer.shutdown_and_close().value();  // would block until socket drained by reader
    // writer.close().value();  // would cause all further reads to fail due to socket broken
    auto read = reader.first->read(0, {{buffer, 64}}, std::chrono::milliseconds(1));
    BOOST_REQUIRE(read.value() == 5);
    BOOST_CHECK(0 == memcmp(buffer, "hello", 5));
    // The TLS shutdown needs to do various reading and writing until the connection goes down
    for(size_t count = 0; count < 1024; count++)
    {
      auto r1 = writer->shutdown_and_close(std::chrono::milliseconds(0));
      auto r2 = reader.first->read({{nullptr, 0}}, std::chrono::milliseconds(0));
      (void) r2;
      if(r1)
      {
        connected = false;
        break;
      }
      std::cout << "*" << std::flush;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::cout << std::endl;
    BOOST_REQUIRE(!connected);
    writer->close().value();
    reader.first->close().value();
  }
};
static inline void TestNonBlockingTLSSocketHandles()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  if(llfio::tls_socket_source_registry::empty())
  {
    std::cout << "\nNOTE: This platform has no TLS socket sources in its registry, skipping this test." << std::endl;
    return;
  }
  auto tls_socket_source = llfio::tls_socket_source_registry::default_source().instantiate().value();
  std::cout << "\nUnwrapped TLS socket:\n" << std::endl;
  TestNonBlockingTLSSocketHandlesRunTest(tls_socket_source->multiplexable_listening_socket(llfio::ip::family::v4).value(),
                                         [&] { return tls_socket_source->multiplexable_connecting_socket(llfio::ip::family::v4).value(); });

  std::cout << "\nWrapped TLS socket:\n" << std::endl;
  auto rawserversocket = llfio::listening_byte_socket_handle::multiplexable_listening_byte_socket(llfio::ip::family::v4).value();
  auto rawwriter = llfio::byte_socket_handle::multiplexable_byte_socket(llfio::ip::family::v4).value();
  TestNonBlockingTLSSocketHandlesRunTest(tls_socket_source->wrap(&rawserversocket).value(), [&] { return tls_socket_source->wrap(&rawwriter).value(); });
}

/* This test makes the assumption that the host OS is able to validate github.com's
TLS certificate.
*/
static inline void TestAuthenticatingTLSSocketHandles()
{
  //! [https_get]
  namespace llfio = LLFIO_V2_NAMESPACE;
  static constexpr llfio::string_view test_host("github.com");
  static constexpr llfio::string_view get_request("GET / HTTP/1.0\r\nHost: github.com\r\n\r\n");
  if(llfio::tls_socket_source_registry::empty())
  {
    std::cout << "\nNOTE: This platform has no TLS socket sources in its registry, skipping this test." << std::endl;
    return;
  }
  auto test_host_ip = llfio::ip::resolve(test_host, "https", llfio::ip::family::any, {}, llfio::ip::resolve_flag::blocking).value()->get().value().front();
  std::cout << "The IP address of " << test_host << " is " << test_host_ip << std::endl;
  BOOST_REQUIRE(test_host_ip.is_v4() || test_host_ip.is_v6());
  auto tls_socket_source = llfio::tls_socket_source_registry::default_source().instantiate().value();
  auto sock = tls_socket_source->multiplexable_connecting_socket(test_host_ip.family()).value();
  {
    auto r = sock->connect(test_host, 443, std::chrono::seconds(5));
    if(!r)
    {
      if(r.error() == llfio::errc::timed_out || r.error() == llfio::errc::host_unreachable || r.error() == llfio::errc::network_unreachable)
      {
        std::cout << "\nNOTE: Failed to connect to " << test_host
                  << " within five seconds, assuming there is no internet connection and skipping this test. Error was: " << r.error().message() << std::endl;
        return;
      }
      r.value();
    }
  }
  // Get the front page
  std::cout << "\nThe socket which connected to " << test_host << " negotiated the cipher " << sock->algorithms_description() << std::endl;
  llfio::tls_socket_handle::const_buffer_type get_request_buffer(reinterpret_cast<const llfio::byte *>(get_request.data()), get_request.size());
  auto written = sock->write({get_request_buffer}).value();
  BOOST_REQUIRE(written == get_request.size());
  // Fetch the front page. The connection will close once all data is sent.
  std::vector<llfio::byte> buffer(4096);
  size_t offset = 0;
  for(size_t nread = 0; (nread = sock->read({{buffer.data() + offset, buffer.size() - offset}}, std::chrono::seconds(3)).value()) > 0;)
  {
    offset += nread;
    if(buffer.size() - offset < 1024)
    {
      buffer.resize(buffer.size() + 4096);
    }
  }
  buffer.resize(offset);
  std::cout << "\nRead from " << test_host << " " << offset << " bytes. The first 1024 bytes are:\n\n"
            << llfio::string_view(reinterpret_cast<const char *>(buffer.data()), offset).substr(0, 1024) << "\n"
            << std::endl;
  // Make sure this doesn't hang because the socket is closed
  sock->shutdown_and_close().value();
  //! [https_get]
}

#if 0
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
static inline void TestMultiplexedTLSSocketHandles()
{
  static constexpr size_t MAX_SOCKETS = 64;
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto test_multiplexer = [](llfio::byte_io_multiplexer_ptr multiplexer) {
    std::vector<llfio::byte_socket_handle> read_sockets, write_sockets;
    std::vector<size_t> received_for(MAX_SOCKETS);
    struct checking_receiver final : public llfio::byte_io_multiplexer::io_operation_state_visitor
    {
      size_t myindex;
      std::unique_ptr<llfio::byte[]> io_state_ptr;
      std::vector<size_t> &received_for;
      union
      {
        llfio::byte _buffer[sizeof(size_t)];
        size_t _index;
      };
      llfio::byte_socket_handle::buffer_type buffer;
      llfio::byte_io_multiplexer::io_operation_state *io_state{nullptr};

      checking_receiver(size_t _myindex, llfio::byte_io_multiplexer_ptr &multiplexer, std::vector<size_t> &r)
          : myindex(_myindex)
          , io_state_ptr(std::make_unique<llfio::byte[]>(multiplexer->io_state_requirements().first))
          , received_for(r)
          , buffer(_buffer, sizeof(_buffer))
      {
        memset(_buffer, 0, sizeof(_buffer));
      }
      checking_receiver(const checking_receiver &) = delete;
      checking_receiver(checking_receiver &&o) = default;
      checking_receiver &operator=(const checking_receiver &) = delete;
      checking_receiver &operator=(checking_receiver &&) = default;
      ~checking_receiver()
      {
        if(io_state != nullptr)
        {
          if(!is_finished(io_state->current_state()))
          {
            abort();
          }
          io_state->~io_operation_state();
          io_state = nullptr;
        }
      }

      // Initiated the read
      llfio::result<void> read_begin(llfio::byte_io_multiplexer_ptr &multiplexer, llfio::byte_io_handle &h)
      {
        if(io_state != nullptr)
        {
          BOOST_REQUIRE(is_finished(io_state->current_state()));
          io_state->~io_operation_state();
          io_state = nullptr;
        }
        buffer = {_buffer, sizeof(_buffer)};
        io_state = multiplexer->construct_and_init_io_operation(
        {io_state_ptr.get(), 4096 /*lies*/}, &h, this, {}, {}, llfio::byte_socket_handle::io_request<llfio::byte_socket_handle::buffers_type>({&buffer, 1}, 0));
        return llfio::success();
      }

      // Called if the read did not complete immediately
      virtual void read_initiated(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type /*former*/) override
      {
        std::cout << "   Socket " << myindex << " will complete read later" << std::endl;
      }

      // Called when the read completes
      virtual bool read_completed(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type former,
                                  llfio::byte_socket_handle::io_result<llfio::byte_socket_handle::buffers_type> &&res) override
      {
        if(is_initialised(former))
        {
          std::cout << "   Socket " << myindex << " read completes immediately" << std::endl;
        }
        else
        {
          std::cout << "   Socket " << myindex << " read completes asynchronously" << std::endl;
        }
        BOOST_CHECK(res.has_value());
        if(res)
        {
          BOOST_REQUIRE(res.value().size() == 1);
          BOOST_CHECK(res.value()[0].data() == _buffer);
          BOOST_CHECK(res.value()[0].size() == sizeof(size_t));
          BOOST_REQUIRE(_index < MAX_SOCKETS);
          BOOST_CHECK(_index == myindex);
          received_for[_index]++;
        }
        return true;
      }

      // Called when the state for the read can be disposed
      virtual void read_finished(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type former) override
      {
        std::cout << "   Socket " << myindex << " read finishes" << std::endl;
        BOOST_REQUIRE(former == llfio::io_operation_state_type::read_completed);
      }
    };
    auto serversocket =
    llfio::listening_socket_handle::listening_socket(llfio::ip::family::v4, llfio::listening_socket_handle::mode::write,
                                                     llfio::byte_socket_handle::caching::all, llfio::byte_socket_handle::flag::multiplexable)
    .value();
    serversocket.bind(llfio::ip::address_v4::loopback()).value();
    auto endpoint = serversocket.local_endpoint().value();
    std::cout << "Server socket is listening on " << endpoint << std::endl;
    if(endpoint.family() == llfio::ip::family::unknown && getenv("CI") != nullptr)
    {
      std::cout << "\nNOTE: Currently on CI and couldn't bind a listening socket to loopback, assuming it is CI host restrictions and skipping this test."
                << std::endl;
      return;
    }

    std::vector<checking_receiver> async_reads;
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      auto write_socket = llfio::byte_socket_handle::byte_socket(llfio::ip::family::v4, llfio::byte_socket_handle::mode::write,
                                                                 llfio::byte_socket_handle::caching::all, llfio::byte_socket_handle::flag::multiplexable)
                          .value();
      write_socket.connect(endpoint).value();
      std::pair<llfio::byte_socket_handle, llfio::ip::address> read_socket;
      serversocket.read({read_socket}).value();
      read_socket.first.set_multiplexer(multiplexer.get()).value();
      async_reads.push_back(checking_receiver(n, multiplexer, received_for));
      read_sockets.push_back(std::move(read_socket.first));
      write_sockets.push_back(std::move(write_socket));
    }
    auto writerthread = std::async([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      for(size_t n = MAX_SOCKETS - 1; n < MAX_SOCKETS; n--)
      {
        auto r = write_sockets[n].write(0, {{(llfio::byte *) &n, sizeof(n)}});
        if(!r)
        {
          abort();
        }
      }
    });
    // Start the socket reads. They cannot move in memory until complete
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      async_reads[n].read_begin(multiplexer, read_sockets[n]).value();
    }
    // Wait for all reads to complete
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      // Spin until this i/o completes
      for(;;)
      {
        auto state = multiplexer->check_io_operation(async_reads[n].io_state);
        if(is_completed(state) || is_finished(state))
        {
          break;
        }
      }
    }
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      BOOST_CHECK(received_for[n] == 1);
    }
    // Wait for all reads to finish
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      llfio::io_operation_state_type state;
      while(!is_finished(state = async_reads[n].io_state->current_state()))
      {
        multiplexer->check_for_any_completed_io().value();
      }
    }
    writerthread.get();
  };
#ifdef _WIN32
  std::cout << "\nSingle threaded IOCP, immediate completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(1, false).value());
  std::cout << "\nSingle threaded IOCP, reactor completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(1, true).value());
  std::cout << "\nMultithreaded IOCP, immediate completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(2, false).value());
  std::cout << "\nMultithreaded IOCP, reactor completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(2, true).value());
#else
#error Not implemented yet
#endif
}

#if LLFIO_ENABLE_COROUTINES
static inline void TestCoroutinedTLSSocketHandles()
{
  static constexpr size_t MAX_SOCKETS = 70;
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto test_multiplexer = [](llfio::byte_io_multiplexer_ptr multiplexer) {
    struct coroutine
    {
      llfio::byte_socket_handle read_socket, write_socket;
      size_t received_for{0};

      explicit coroutine(llfio::byte_socket_handle &&r, llfio::byte_socket_handle &&w)
          : read_socket(std::move(r))
          , write_socket(std::move(w))
      {
      }
      llfio::eager<llfio::result<void>> operator()()
      {
        union
        {
          llfio::byte _buffer[sizeof(size_t)];
          size_t _index;
        };
        llfio::byte_socket_handle::buffer_type buffer;
        for(;;)
        {
          buffer = {_buffer, sizeof(_buffer)};
          // This will never return if the coroutine gets cancelled
          auto r = co_await read_socket.co_read({{&buffer, 1}, 0});
          if(!r)
          {
            co_return std::move(r).error();
          }
          BOOST_CHECK(r.value().size() == 1);
          BOOST_CHECK(r.value()[0].size() == sizeof(_buffer));
          received_for += _index;
        }
      }
    };
    auto serversocket =
    llfio::listening_socket_handle::listening_socket(true, llfio::listening_socket_handle::mode::write, llfio::byte_socket_handle::caching::all,
                                                     llfio::byte_socket_handle::flag::multiplexable)
    .value();
    serversocket.bind(llfio::ip::address_v4::loopback()).value();
    auto endpoint = serversocket.local_endpoint().value();
    std::cout << "Server socket is listening on " << endpoint << std::endl;
    if(endpoint.family() == llfio::ip::family::unknown && getenv("CI") != nullptr)
    {
      std::cout << "\nNOTE: Currently on CI and couldn't bind a listening socket to loopback, assuming it is CI host restrictions and skipping this test."
                << std::endl;
      return;
    }

    std::vector<coroutine> coroutines;
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      auto write_socket = llfio::byte_socket_handle::byte_socket(llfio::ip::family::v4, llfio::byte_socket_handle::mode::write,
                                                                 llfio::byte_socket_handle::caching::all, llfio::byte_socket_handle::flag::multiplexable)
                          .value();
      write_socket.connect(endpoint).value();
      std::pair<llfio::byte_socket_handle, llfio::ip::address> read_socket;
      serversocket.read({read_socket}).value();
      read_socket.first.set_multiplexer(multiplexer.get()).value();
      coroutines.push_back(coroutine(std::move(read_socket.first), std::move(write_socket)));
    }
    // Start the coroutines, all of whom will begin a read and then suspend
    std::vector<llfio::optional<llfio::eager<llfio::result<void>>>> states(MAX_SOCKETS);
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      states[n].emplace(coroutines[n]());
    }
    // Write to all the sockets, then pump coroutine resumption until all completions done
    size_t count = 0, failures = 0;
    for(size_t i = 1; i <= 10; i++)
    {
      std::cout << "\nWrite no " << i << std::endl;
      for(size_t n = MAX_SOCKETS - 1; n < MAX_SOCKETS; n--)
      {
        coroutines[n].write_socket.write(0, {{(llfio::byte *) &i, sizeof(i)}}).value();
      }
      // Take a copy of all pending i/o
      for(;;)
      {
        // Have the kernel tell me when an i/o completion is ready
        auto r = multiplexer->check_for_any_completed_io();
        if(r.value().initiated_ios_completed == 0 && r.value().initiated_ios_finished == 0)
        {
          break;
        }
      }
      count += i;
      for(size_t n = 0; n < MAX_SOCKETS; n++)
      {
        if(coroutines[n].received_for != count)
        {
          std::cout << "Coroutine " << n << " has count " << coroutines[n].received_for << " instead of " << count << std::endl;
          failures++;
        }
        BOOST_CHECK(coroutines[n].received_for == count);
      }
      BOOST_REQUIRE(failures == 0);
    }
    // Rethrow any failures
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      if(states[n]->await_ready())
      {
        states[n]->await_resume().value();
      }
    }
    // Destruction of coroutines when they are suspended must work.
    // This will cancel any pending i/o and immediately exit the
    // coroutines
    states.clear();
    // Now clear all the coroutines
    coroutines.clear();
  };
#ifdef _WIN32
  std::cout << "\nSingle threaded IOCP, immediate completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(1, false).value());
  std::cout << "\nSingle threaded IOCP, reactor completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(1, true).value());
  std::cout << "\nMultithreaded IOCP, immediate completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(2, false).value());
  std::cout << "\nMultithreaded IOCP, reactor completions:\n";
  test_multiplexer(llfio::test::multiplexer_win_iocp(2, true).value());
#else
#error Not implemented yet
#endif
}
#endif
#endif
#endif

static inline void TestPollingTLSSocketHandles()
{
  static constexpr size_t MAX_SOCKETS = 64;
  namespace llfio = LLFIO_V2_NAMESPACE;
  if(llfio::tls_socket_source_registry::empty())
  {
    std::cout << "\nNOTE: This platform has no TLS socket sources in its registry, skipping this test." << std::endl;
    return;
  }
  auto tls_socket_source = llfio::tls_socket_source_registry::default_source().instantiate().value();
  auto runtest = [](auto &&make_reader, auto &&make_writer)
  {
    std::vector<std::pair<llfio::listening_tls_socket_handle_ptr, llfio::ip::address>> listening;
    std::vector<llfio::tls_socket_handle_ptr> sockets;
    std::vector<size_t> idxs;
    for(size_t n = 0; n < MAX_SOCKETS; n++)
    {
      llfio::listening_tls_socket_handle_ptr s = make_reader(n);
      // Disable authentication
      s->set_authentication_certificates_path({}).value();
      s->bind(llfio::ip::address_v4::loopback()).value();
      auto endpoint = s->local_endpoint().value();
      if(endpoint.family() == llfio::ip::family::unknown && getenv("CI") != nullptr)
      {
        std::cout << "\nNOTE: Currently on CI and couldn't bind a listening socket to loopback, assuming it is CI host restrictions and skipping this test."
                  << std::endl;
        return;
      }
      listening.emplace_back(std::move(s), endpoint);
      sockets.push_back(make_writer(n));
      // Disable authentication
      sockets.back()->set_authentication_certificates_path({}).value();
      idxs.push_back(n);
    }
    QUICKCPPLIB_NAMESPACE::algorithm::small_prng::random_shuffle(idxs.begin(), idxs.end());
    std::mutex lock;
    std::atomic<size_t> currently_connecting{(size_t) -1};
    auto poll_listening_task = std::async(std::launch::async,
                                          [&]
                                          {
                                            std::vector<llfio::pollable_handle *> handles;
                                            std::vector<llfio::poll_what> what, out;
                                            for(size_t n = 0; n < MAX_SOCKETS; n++)
                                            {
                                              handles.push_back(listening[n].first.get());
                                              what.push_back(llfio::poll_what::is_readable);
                                              out.push_back(llfio::poll_what::none);
                                            }
                                            for(;;)
                                            {
                                              int ret = (int) llfio::poll(out, {handles}, what, std::chrono::seconds(30)).value();
                                              bool done = true;
                                              for(size_t n = 0; n < MAX_SOCKETS; n++)
                                              {
                                                auto idx = idxs[n];
                                                if(handles[idx] != nullptr)
                                                {
                                                  done = false;
                                                  if(out[idx] & llfio::poll_what::is_readable)
                                                  {
                                                    {
                                                      std::lock_guard<std::mutex> g(lock);
                                                      std::cout << "Poll listening sees readable (raw = " << (int) (uint8_t) out[idx] << ") on socket " << idx
                                                                << ". Currently connecting is " << currently_connecting << std::endl;
                                                    }
                                                    BOOST_CHECK(currently_connecting == idx);
                                                    std::pair<llfio::tls_socket_handle_ptr, llfio::ip::address> s;
                                                    listening[idx].first->read({s}).value();
                                                    llfio::byte buf;
                                                    llfio::tls_socket_handle::buffer_type b(&buf, 1);
                                                    s.first->read({{b}});
                                                    handles[idx] = nullptr;
                                                    ret--;
                                                  }
                                                  out[idx] = llfio::poll_what::none;
                                                }
                                              }
                                              BOOST_CHECK(ret == 0);
                                              if(done)
                                              {
                                                std::lock_guard<std::mutex> g(lock);
                                                std::cout << "Poll listening task exits." << std::endl;
                                                break;
                                              }
                                            }
                                          });
    auto poll_connecting_task = std::async(std::launch::async,
                                           [&]
                                           {
                                             std::vector<llfio::pollable_handle *> handles;
                                             std::vector<llfio::poll_what> what, out;
                                             for(size_t n = 0; n < MAX_SOCKETS; n++)
                                             {
                                               handles.push_back(sockets[n].get());
                                               what.push_back(llfio::poll_what::is_writable);
                                               out.push_back(llfio::poll_what::none);
                                             }
                                             for(;;)
                                             {
                                               int ret = (int) llfio::poll(out, {handles}, what, std::chrono::seconds(30)).value();
                                               bool done = true, saw_closed = false;
                                               size_t remaining = MAX_SOCKETS;
                                               for(size_t n = 0; n < MAX_SOCKETS; n++)
                                               {
                                                 auto idx = idxs[n];
                                                 if(handles[idx] != nullptr)
                                                 {
                                                   done = false;
                                                   // On Linux, a new socket not yet connected MAY appear as both writable and hanged up,
                                                   // so filter out the closed.
                                                   if(!(out[idx] & llfio::poll_what::is_closed) || (remaining == 1 && currently_connecting == idx))
                                                   {
                                                     if(out[idx] & llfio::poll_what::is_writable)
                                                     {
                                                       {
                                                         std::lock_guard<std::mutex> g(lock);
                                                         std::cout << "Poll connect sees writable (raw = " << (int) (uint8_t) out[idx] << ")  on socket " << idx
                                                                   << ". Currently connecting is " << currently_connecting << std::endl;
                                                       }
                                                       BOOST_CHECK(currently_connecting == idx);
                                                       handles[idx] = nullptr;
                                                       ret--;
                                                     }
                                                   }
                                                   else
                                                   {
                                                     saw_closed = true;
                                                   }
                                                   out[idx] = llfio::poll_what::none;
                                                 }
                                                 else
                                                 {
                                                   remaining--;
                                                 }
                                               }
                                               if(!saw_closed)
                                               {
                                                 BOOST_CHECK(ret == 0);
                                               }
                                               if(done)
                                               {
                                                 std::lock_guard<std::mutex> g(lock);
                                                 std::cout << "Poll connect task exits." << std::endl;
                                                 break;
                                               }
                                             }
                                           });
    auto connect_task = std::async(std::launch::async,
                                   [&]
                                   {
                                     for(size_t n = 0; n < MAX_SOCKETS; n++)
                                     {
                                       auto idx = idxs[n];
                                       {
                                         std::lock_guard<std::mutex> g(lock);
                                         std::cout << "Connecting " << idx << " ... " << std::endl;
                                       }
                                       currently_connecting = idx;
                                       sockets[idx]->connect(listening[idx].second).value();
                                       llfio::byte buf(llfio::to_byte(0));
                                       llfio::tls_socket_handle::const_buffer_type b(&buf, 1);
                                       sockets[idx]->write({{&b, 1}});
                                       std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                     }
                                     std::this_thread::sleep_for(std::chrono::seconds(1));
                                     std::lock_guard<std::mutex> g(lock);
                                     std::cout << "Connecting task exits." << std::endl;
                                   });
    connect_task.get();
    poll_listening_task.get();
    poll_connecting_task.get();
  };
  std::cout << "\nUnwrapped TLS socket:\n" << std::endl;
  runtest([&](size_t) { return tls_socket_source->multiplexable_listening_socket(llfio::ip::family::v4).value(); },
          [&](size_t) { return tls_socket_source->multiplexable_connecting_socket(llfio::ip::family::v4).value(); });

  std::cout << "\nWrapped TLS socket:\n" << std::endl;
  std::vector<llfio::listening_byte_socket_handle> rawlisteners;
  for(size_t n = 0; n < MAX_SOCKETS; n++)
  {
    rawlisteners.push_back(llfio::listening_byte_socket_handle::multiplexable_listening_byte_socket(llfio::ip::family::v4).value());
  }
  std::vector<llfio::byte_socket_handle> rawwriters;
  for(size_t n = 0; n < MAX_SOCKETS; n++)
  {
    rawwriters.push_back(llfio::byte_socket_handle::multiplexable_byte_socket(llfio::ip::family::v4).value());
  }
  runtest([&](size_t idx) { return tls_socket_source->wrap(&rawlisteners[idx]).value(); },
          [&](size_t idx) { return tls_socket_source->wrap(&rawwriters[idx]).value(); });
}

KERNELTEST_TEST_KERNEL(integration, llfio, tls_socket_handle, blocking, "Tests that blocking llfio::tls_byte_socket_handle works as expected",
                       TestBlockingTLSSocketHandles())
KERNELTEST_TEST_KERNEL(integration, llfio, tls_socket_handle, nonblocking, "Tests that nonblocking llfio::tls_byte_socket_handle works as expected",
                       TestNonBlockingTLSSocketHandles())
KERNELTEST_TEST_KERNEL(integration, llfio, tls_socket_handle, authenticating,
                       "Tests that connecting to an authenticating server using llfio::tls_byte_socket_handle works as expected",
                       TestAuthenticatingTLSSocketHandles())
#if 0
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
KERNELTEST_TEST_KERNEL(integration, llfio, tls_socket_handle, multiplexed, "Tests that multiplexed llfio::tls_byte_socket_handle works as expected",
                       TestMultiplexedTLSSocketHandles())
#if LLFIO_ENABLE_COROUTINES
KERNELTEST_TEST_KERNEL(integration, llfio, tls_socket_handle, coroutined, "Tests that coroutined llfio::tls_byte_socket_handle works as expected",
                       TestCoroutinedTLSSocketHandles())
#endif
#endif
#endif
KERNELTEST_TEST_KERNEL(integration, llfio, tls_socket_handle, poll, "Tests that polling llfio::tls_byte_socket_handle works as expected",
                       TestPollingTLSSocketHandles())
