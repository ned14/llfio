/* Integration test kernel for whether socket handles work
(C) 2021-2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

#include "../test_kernel_decl.hpp"

#include <future>
#include <sstream>
#include <unordered_set>

static inline void TestSocketAddress()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  {
    auto a = llfio::ip::address_v4::loopback();
    BOOST_CHECK(a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(a.is_v4());
    BOOST_CHECK(!a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 4);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(127));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(1));
    BOOST_CHECK(a.to_uint() == 0x7f000001);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "127.0.0.1:0");
  }  // namespace ;
  {
    auto a = llfio::ip::address_v6::loopback();
    BOOST_CHECK(a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[4] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[5] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[6] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[7] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[8] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[9] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[10] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[11] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[12] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[13] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[14] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[15] == llfio::to_byte(1));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[::1]:0");
  }
  {
    auto a = llfio::ip::address_v4::any();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_any());
    BOOST_CHECK(a.is_v4());
    BOOST_CHECK(!a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 4);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0));
    BOOST_CHECK(a.to_uint() == 0x0000000);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "0.0.0.0:0");
  }
  {
    auto a = llfio::ip::address_v6::any();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_any());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[4] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[5] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[6] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[7] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[8] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[9] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[10] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[11] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[12] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[13] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[14] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[15] == llfio::to_byte(0));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[::]:0");
  }
  {
    auto a = llfio::ip::make_address("78.68.1.255:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(a.is_v4());
    BOOST_CHECK(!a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 4);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(78));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(68));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(1));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(255));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "78.68.1.255:1234");
  }
  {
    auto a = llfio::ip::make_address("[78aa:bb68::1:255]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0x78));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0xaa));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0xbb));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0x68));
    BOOST_CHECK(a.to_bytes()[4] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[5] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[6] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[7] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[8] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[9] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[10] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[11] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[12] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[13] == llfio::to_byte(0x1));
    BOOST_CHECK(a.to_bytes()[14] == llfio::to_byte(0x2));
    BOOST_CHECK(a.to_bytes()[15] == llfio::to_byte(0x55));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[78aa:bb68::1:255]:1234");
  }

  {
    auto a = llfio::ip::make_address("[78aa:0:0:bb68:0:0:0:255]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0x78));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0xaa));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[4] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[5] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[6] == llfio::to_byte(0xbb));
    BOOST_CHECK(a.to_bytes()[7] == llfio::to_byte(0x68));
    BOOST_CHECK(a.to_bytes()[8] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[9] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[10] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[11] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[12] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[13] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[14] == llfio::to_byte(0x2));
    BOOST_CHECK(a.to_bytes()[15] == llfio::to_byte(0x55));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[78aa:0:0:bb68::255]:1234");
  }
  {
    auto a = llfio::ip::make_address("[78aa:0:0:0:bb68:0:0:255]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0x78));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0xaa));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[4] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[5] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[6] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[7] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[8] == llfio::to_byte(0xbb));
    BOOST_CHECK(a.to_bytes()[9] == llfio::to_byte(0x68));
    BOOST_CHECK(a.to_bytes()[10] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[11] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[12] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[13] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[14] == llfio::to_byte(0x2));
    BOOST_CHECK(a.to_bytes()[15] == llfio::to_byte(0x55));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[78aa::bb68:0:0:255]:1234");
  }
  {
    auto a = llfio::ip::make_address("[1000::]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_any());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == llfio::to_byte(0x10));
    BOOST_CHECK(a.to_bytes()[1] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[2] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[3] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[4] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[5] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[6] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[7] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[8] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[9] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[10] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[11] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[12] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[13] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[14] == llfio::to_byte(0));
    BOOST_CHECK(a.to_bytes()[15] == llfio::to_byte(0));
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[1000::]:1234");
  }
}

static inline void TestBlockingSocketHandles()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto serversocket = llfio::listening_socket_handle::listening_socket(llfio::ip::family::v4, llfio::listening_socket_handle::mode::read).value();
  BOOST_REQUIRE(serversocket.is_valid());
  BOOST_CHECK(serversocket.is_socket());
  BOOST_CHECK(serversocket.is_readable());
  BOOST_CHECK(!serversocket.is_writable());
  serversocket.bind(llfio::ip::address_v4::loopback()).value();
  auto endpoint = serversocket.local_endpoint().value();
  std::cout << "Server socket is listening on " << endpoint << std::endl;
  auto readerthread = std::async([serversocket = std::move(serversocket)]() mutable {
    std::pair<llfio::byte_socket_handle, llfio::ip::address> s;
    serversocket.read({s}).value();  // This immediately blocks in blocking mode
    BOOST_REQUIRE(s.first.is_valid());
    BOOST_CHECK(s.first.is_socket());
    BOOST_CHECK(s.first.is_readable());
    BOOST_CHECK(!s.first.is_writable());
    std::cout << "Server thread sees incoming connection from " << s.second << std::endl;
    serversocket.close().value();
    llfio::byte buffer[64];
    auto read = s.first.read(0, {{buffer, 64}}).value();
    BOOST_REQUIRE(read == 5);
    BOOST_CHECK(0 == memcmp(buffer, "hello", 5));
    s.first.close().value();
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
  llfio::byte_socket_handle writer;
  begin = std::chrono::steady_clock::now();
  while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() < 1000)
  {
    writer =
    llfio::byte_socket_handle::byte_socket(llfio::ip::family::v4, llfio::byte_socket_handle::mode::append, llfio::byte_socket_handle::caching::reads).value();
    auto r = writer.connect(endpoint);
    if(r)
    {
      break;
    }
  }
  BOOST_REQUIRE(writer.is_valid());
  BOOST_CHECK(writer.is_socket());
  BOOST_CHECK(!writer.is_readable());
  BOOST_CHECK(writer.is_writable());
  auto written = writer.write(0, {{(const llfio::byte *) "hello", 5}}).value();
  BOOST_REQUIRE(written == 5);
  writer.shutdown_and_close().value();
  readerthread.get();
}

static inline void TestNonBlockingSocketHandles()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto serversocket = llfio::listening_socket_handle::listening_socket(llfio::ip::family::v4, llfio::listening_socket_handle::mode::read,
                                                                       llfio::byte_socket_handle::caching::all, llfio::byte_socket_handle::flag::multiplexable)
                      .value();
  BOOST_REQUIRE(serversocket.is_valid());
  BOOST_CHECK(serversocket.is_socket());
  BOOST_CHECK(serversocket.is_readable());
  BOOST_CHECK(!serversocket.is_writable());
  serversocket.bind(llfio::ip::address_v4::loopback()).value();
  auto endpoint = serversocket.local_endpoint().value();
  std::cout << "Server socket is listening on " << endpoint << std::endl;

  std::pair<llfio::byte_socket_handle, llfio::ip::address> reader;
  {  // no incoming, so non-blocking read should time out
    auto read = serversocket.read({reader}, std::chrono::milliseconds(0));
    BOOST_REQUIRE(read.has_error());
    BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
  }
  {  // no incoming, so blocking read should time out
    auto read = serversocket.read({reader}, std::chrono::seconds(1));
    BOOST_REQUIRE(read.has_error());
    BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
  }

  // Form the connection.
  auto writer = llfio::byte_socket_handle::byte_socket(llfio::ip::family::v4, llfio::byte_socket_handle::mode::append,
                                                       llfio::byte_socket_handle::caching::reads, llfio::byte_socket_handle::flag::multiplexable)
                .value();
  writer.connect(endpoint).value();
  serversocket.read({reader}, std::chrono::seconds(1)).value();
  std::cout << "Server socket sees incoming connection from " << reader.second << std::endl;

  llfio::byte buffer[64];
  {  // no data, so non-blocking read should time out
    auto read = reader.first.read(0, {{buffer, 64}}, std::chrono::milliseconds(0));
    BOOST_REQUIRE(read.has_error());
    BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
  }
  {  // no data, so blocking read should time out
    auto read = reader.first.read(0, {{buffer, 64}}, std::chrono::seconds(1));
    BOOST_REQUIRE(read.has_error());
    BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
  }
  auto written = writer.write(0, {{(const llfio::byte *) "hello", 5}}).value();
  BOOST_REQUIRE(written == 5);
  // writer.shutdown_and_close().value();  // would block until socket drained by reader
  // writer.close().value();  // would cause all further reads to fail due to socket broken
  auto read = reader.first.read(0, {{buffer, 64}}, std::chrono::milliseconds(1));
  BOOST_REQUIRE(read.value() == 5);
  BOOST_CHECK(0 == memcmp(buffer, "hello", 5));
  writer.shutdown_and_close().value();  // must not block nor fail
  writer.close().value();
  reader.first.close().value();
}

#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
static inline void TestMultiplexedSocketHandles()
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
static inline void TestCoroutinedSocketHandles()
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

KERNELTEST_TEST_KERNEL(integration, llfio, ip, address, "Tests that llfio::ip::address works as expected", TestSocketAddress())
KERNELTEST_TEST_KERNEL(integration, llfio, socket_handle, blocking, "Tests that blocking llfio::byte_socket_handle works as expected",
                       TestBlockingSocketHandles())
KERNELTEST_TEST_KERNEL(integration, llfio, socket_handle, nonblocking, "Tests that nonblocking llfio::byte_socket_handle works as expected",
                       TestNonBlockingSocketHandles())
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
KERNELTEST_TEST_KERNEL(integration, llfio, socket_handle, multiplexed, "Tests that multiplexed llfio::byte_socket_handle works as expected",
                       TestMultiplexedSocketHandles())
#if LLFIO_ENABLE_COROUTINES
KERNELTEST_TEST_KERNEL(integration, llfio, socket_handle, coroutined, "Tests that coroutined llfio::byte_socket_handle works as expected",
                       TestCoroutinedSocketHandles())
#endif
#endif
