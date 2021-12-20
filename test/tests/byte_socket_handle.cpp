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
#include <unordered_set>
#include <sstream>

static inline void TestSocketAddress()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  {
    auto a = llfio::ip::address_v4::loopback();
    BOOST_CHECK(a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_unspecified());
    BOOST_CHECK(a.is_v4());
    BOOST_CHECK(!a.is_v6());
    BOOST_CHECK(a.port()==0);
    BOOST_CHECK(a.to_bytes().size() == 4);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 127);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 1);
    BOOST_CHECK(a.to_uint() == 0x7f000001);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "127.0.0.1:0");
  } 
  {
    auto a = llfio::ip::address_v6::loopback();
    BOOST_CHECK(a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(!a.is_unspecified());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[4] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[5] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[6] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[7] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[8] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[9] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[10] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[11] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[12] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[13] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[14] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[15] == (llfio::byte) 1);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[::1]:0");
  }
  {
    auto a = llfio::ip::address_v4::any();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_unspecified());
    BOOST_CHECK(a.is_v4());
    BOOST_CHECK(!a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 4);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 0);
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
    BOOST_CHECK(a.is_unspecified());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 0);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[4] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[5] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[6] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[7] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[8] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[9] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[10] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[11] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[12] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[13] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[14] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[15] == (llfio::byte) 0);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[::]:0");
  }
  {
    auto a = llfio::ip::make_address("78.68.1.255:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_unspecified());
    BOOST_CHECK(a.is_v4());
    BOOST_CHECK(!a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 4);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 78);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 68);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 1);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 255);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "78.68.1.255:1234");
  }
  {
    auto a = llfio::ip::make_address("[78aa:bb68::1:255]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_unspecified());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 0x78);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0xaa);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0xbb);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 0x68);
    BOOST_CHECK(a.to_bytes()[4] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[5] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[6] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[7] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[8] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[9] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[10] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[11] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[12] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[13] == (llfio::byte) 0x1);
    BOOST_CHECK(a.to_bytes()[14] == (llfio::byte) 0x2);
    BOOST_CHECK(a.to_bytes()[15] == (llfio::byte) 0x55);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[78aa:bb68::1:255]:1234");
  }

  {
    auto a = llfio::ip::make_address("[78aa:0:0:bb68:0:0:0:255]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_unspecified());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 0x78);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0xaa);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[4] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[5] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[6] == (llfio::byte) 0xbb);
    BOOST_CHECK(a.to_bytes()[7] == (llfio::byte) 0x68);
    BOOST_CHECK(a.to_bytes()[8] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[9] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[10] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[11] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[12] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[13] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[14] == (llfio::byte) 0x2);
    BOOST_CHECK(a.to_bytes()[15] == (llfio::byte) 0x55);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[78aa:0:0:bb68::255]:1234");
  }
  {
    auto a = llfio::ip::make_address("[78aa:0:0:0:bb68:0:0:255]:1234").value();
    BOOST_CHECK(!a.is_loopback());
    BOOST_CHECK(!a.is_multicast());
    BOOST_CHECK(a.is_unspecified());
    BOOST_CHECK(!a.is_v4());
    BOOST_CHECK(a.is_v6());
    BOOST_CHECK(a.port() == 1234);
    BOOST_CHECK(a.to_bytes().size() == 16);
    BOOST_CHECK(a.to_bytes()[0] == (llfio::byte) 0x78);
    BOOST_CHECK(a.to_bytes()[1] == (llfio::byte) 0xaa);
    BOOST_CHECK(a.to_bytes()[2] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[3] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[4] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[5] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[6] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[7] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[8] == (llfio::byte) 0xbb);
    BOOST_CHECK(a.to_bytes()[9] == (llfio::byte) 0x68);
    BOOST_CHECK(a.to_bytes()[10] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[11] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[12] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[13] == (llfio::byte) 0);
    BOOST_CHECK(a.to_bytes()[14] == (llfio::byte) 0x2);
    BOOST_CHECK(a.to_bytes()[15] == (llfio::byte) 0x55);
    std::stringstream ss;
    ss << a;
    std::cout << ss.str() << std::endl;
    BOOST_CHECK(ss.str() == "[78aa::bb68:0:0:255]:1234");
  }
}

  static inline void TestBlockingSocketHandles()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto serversocket = llfio::listening_socket_handle::listening_socket(true, llfio::listening_socket_handle::mode::read).value();
  BOOST_REQUIRE(serversocket.is_valid());
  BOOST_REQUIRE(serversocket.is_socket());
  BOOST_REQUIRE(serversocket.is_readable());
  BOOST_REQUIRE(!serversocket.is_writable());
  serversocket.bind(llfio::ip::address_v6::loopback()).value();
  auto endpoint = serversocket.local_endpoint().value();
  std::cout << "Server socket is listening on " << endpoint << std::endl;
  auto readerthread = std::async([serversocket = std::move(serversocket)]() mutable {
    std::pair<llfio::byte_socket_handle, llfio::ip::address> s;
    serversocket.read({s}).value();  // This immediately blocks in blocking mode
    BOOST_REQUIRE(s.first.is_valid());
    BOOST_REQUIRE(s.first.is_socket());
    BOOST_REQUIRE(s.first.is_readable());
    BOOST_REQUIRE(!s.first.is_writable());
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
    auto r = llfio::byte_socket_handle::byte_socket(endpoint, llfio::byte_socket_handle::mode::write);
    if(r)
    {
      writer = std::move(r.value());
      break;
    }
  }
  BOOST_REQUIRE(writer.is_valid());
  BOOST_REQUIRE(writer.is_socket());
  BOOST_REQUIRE(!writer.is_readable());
  BOOST_REQUIRE(writer.is_writable());
  auto written = writer.write(0, {{(const llfio::byte *) "hello", 5}}).value();
  BOOST_REQUIRE(written == 5);
  writer.barrier().value();
  writer.close().value();
  readerthread.get();
}

#if 0
static inline void TestNonBlockingPipeHandle()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  llfio::pipe_handle reader = llfio::pipe_handle::pipe_create("llfio-pipe-handle-test", llfio::pipe_handle::caching::all, llfio::pipe_handle::flag::multiplexable).value();
  llfio::byte buffer[64];
  {  // no writer, so non-blocking read should time out
    auto read = reader.read(0, {{buffer, 64}}, std::chrono::milliseconds(0));
    BOOST_REQUIRE(read.has_error());
    BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
  }
  {  // no writer, so blocking read should time out
    auto read = reader.read(0, {{buffer, 64}}, std::chrono::seconds(1));
    BOOST_REQUIRE(read.has_error());
    BOOST_REQUIRE(read.error() == llfio::errc::timed_out);
  }
  llfio::pipe_handle writer = llfio::pipe_handle::pipe_open("llfio-pipe-handle-test", llfio::pipe_handle::caching::all, llfio::pipe_handle::flag::multiplexable).value();
  auto written = writer.write(0, {{(const llfio::byte *) "hello", 5}}).value();
  BOOST_REQUIRE(written == 5);
  // writer.barrier().value();  // would block until pipe drained by reader
  // writer.close().value();  // would cause all further reads to fail due to pipe broken
  auto read = reader.read(0, {{buffer, 64}}, std::chrono::milliseconds(0));
  BOOST_REQUIRE(read.value() == 5);
  BOOST_CHECK(0 == memcmp(buffer, "hello", 5));
  writer.barrier().value();  // must not block nor fail
  writer.close().value();
  reader.close().value();
}

#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
static inline void TestMultiplexedPipeHandle()
{
  static constexpr size_t MAX_PIPES = 64;
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto test_multiplexer = [](llfio::byte_io_multiplexer_ptr multiplexer) {
    std::vector<llfio::pipe_handle> read_pipes, write_pipes;
    std::vector<size_t> received_for(MAX_PIPES);
    struct checking_receiver final : public llfio::byte_io_multiplexer::io_operation_state_visitor
    {
      size_t myindex;
      std::unique_ptr<llfio::byte[]> io_state_ptr;
      std::vector<size_t> &received_for;
      union {
        llfio::byte _buffer[sizeof(size_t)];
        size_t _index;
      };
      llfio::pipe_handle::buffer_type buffer;
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
        io_state = multiplexer->construct_and_init_io_operation({io_state_ptr.get(), 4096 /*lies*/}, &h, this, {}, {}, llfio::pipe_handle::io_request<llfio::pipe_handle::buffers_type>({&buffer, 1}, 0));
        return llfio::success();
      }

      // Called if the read did not complete immediately
      virtual void read_initiated(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type /*former*/) override { std::cout << "   Pipe " << myindex << " will complete read later" << std::endl; }

      // Called when the read completes
      virtual bool read_completed(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type former, llfio::pipe_handle::io_result<llfio::pipe_handle::buffers_type> &&res) override
      {
        if(is_initialised(former))
        {
          std::cout << "   Pipe " << myindex << " read completes immediately" << std::endl;
        }
        else
        {
          std::cout << "   Pipe " << myindex << " read completes asynchronously" << std::endl;
        }
        BOOST_CHECK(res.has_value());
        if(res)
        {
          BOOST_REQUIRE(res.value().size() == 1);
          BOOST_CHECK(res.value()[0].data() == _buffer);
          BOOST_CHECK(res.value()[0].size() == sizeof(size_t));
          BOOST_REQUIRE(_index < MAX_PIPES);
          BOOST_CHECK(_index == myindex);
          received_for[_index]++;
        }
        return true;
      }

      // Called when the state for the read can be disposed
      virtual void read_finished(llfio::byte_io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type former) override
      {
        std::cout << "   Pipe " << myindex << " read finishes" << std::endl;
        BOOST_REQUIRE(former == llfio::io_operation_state_type::read_completed);
      }
    };
    std::vector<checking_receiver> async_reads;
    for(size_t n = 0; n < MAX_PIPES; n++)
    {
      auto ret = llfio::pipe_handle::anonymous_pipe(llfio::pipe_handle::caching::reads, llfio::pipe_handle::flag::multiplexable).value();
      ret.first.set_multiplexer(multiplexer.get()).value();
      async_reads.push_back(checking_receiver(n, multiplexer, received_for));
      read_pipes.push_back(std::move(ret.first));
      write_pipes.push_back(std::move(ret.second));
    }
    auto writerthread = std::async([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      for(size_t n = MAX_PIPES - 1; n < MAX_PIPES; n--)
      {
        auto r = write_pipes[n].write(0, {{(llfio::byte *) &n, sizeof(n)}});
        if(!r)
        {
          abort();
        }
      }
    });
    // Start the pipe reads. They cannot move in memory until complete
    for(size_t n = 0; n < MAX_PIPES; n++)
    {
      async_reads[n].read_begin(multiplexer, read_pipes[n]).value();
    }
    // Wait for all reads to complete
    for(size_t n = 0; n < MAX_PIPES; n++)
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
    for(size_t n = 0; n < MAX_PIPES; n++)
    {
      BOOST_CHECK(received_for[n] == 1);
    }
    // Wait for all reads to finish
    for(size_t n = 0; n < MAX_PIPES; n++)
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
static inline void TestCoroutinedPipeHandle()
{
  static constexpr size_t MAX_PIPES = 70;
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto test_multiplexer = [](llfio::byte_io_multiplexer_ptr multiplexer) {
    struct coroutine
    {
      llfio::pipe_handle read_pipe, write_pipe;
      size_t received_for{0};

      explicit coroutine(llfio::pipe_handle &&r, llfio::pipe_handle &&w)
          : read_pipe(std::move(r))
          , write_pipe(std::move(w))
      {
      }
      llfio::eager<llfio::result<void>> operator()()
      {
        union {
          llfio::byte _buffer[sizeof(size_t)];
          size_t _index;
        };
        llfio::pipe_handle::buffer_type buffer;
        for(;;)
        {
          buffer = {_buffer, sizeof(_buffer)};
          // This will never return if the coroutine gets cancelled
          auto r = co_await read_pipe.co_read({{&buffer, 1}, 0});
          if(!r)
          {
            co_return std::move(r).error();
          }
          BOOST_CHECK(r.value().size() == 1);
          BOOST_CHECK(r.value()[0].size() == sizeof(_buffer));
          received_for+=_index;
        }
      }
    };
    std::vector<coroutine> coroutines;
    for(size_t n = 0; n < MAX_PIPES; n++)
    {
      auto ret = llfio::pipe_handle::anonymous_pipe(llfio::pipe_handle::caching::reads, llfio::pipe_handle::flag::multiplexable).value();
      ret.first.set_multiplexer(multiplexer.get()).value();
      coroutines.push_back(coroutine(std::move(ret.first), std::move(ret.second)));
    }
    // Start the coroutines, all of whom will begin a read and then suspend
    std::vector<llfio::optional<llfio::eager<llfio::result<void>>>> states(MAX_PIPES);
    for(size_t n = 0; n < MAX_PIPES; n++)
    {
      states[n].emplace(coroutines[n]());
    }
    // Write to all the pipes, then pump coroutine resumption until all completions done
    size_t count = 0, failures = 0;
    for(size_t i = 1; i <= 10; i++)
    {
      std::cout << "\nWrite no " << i << std::endl;
      for(size_t n = MAX_PIPES - 1; n < MAX_PIPES; n--)
      {
        coroutines[n].write_pipe.write(0, {{(llfio::byte *) &i, sizeof(i)}}).value();
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
      for(size_t n = 0; n < MAX_PIPES; n++)
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
    for(size_t n = 0; n < MAX_PIPES; n++)
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

KERNELTEST_TEST_KERNEL(integration, llfio, ip, address, "Tests that llfio::ip::address works as expected", TestSocketAddress())
KERNELTEST_TEST_KERNEL(integration, llfio, socket_handle, blocking, "Tests that blocking llfio::byte_socket_handle works as expected",
                       TestBlockingSocketHandles())
#if 0
KERNELTEST_TEST_KERNEL(integration, llfio, pipe_handle, nonblocking, "Tests that nonblocking llfio::pipe_handle works as expected", TestNonBlockingPipeHandle())
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
KERNELTEST_TEST_KERNEL(integration, llfio, pipe_handle, multiplexed, "Tests that multiplexed llfio::pipe_handle works as expected", TestMultiplexedPipeHandle())
#if LLFIO_ENABLE_COROUTINES
KERNELTEST_TEST_KERNEL(integration, llfio, pipe_handle, coroutined, "Tests that coroutined llfio::pipe_handle works as expected", TestCoroutinedPipeHandle())
#endif
#endif
#endif
