/* Integration test kernel for map_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2016


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

static inline void TestAsyncFileHandleCoroutines()
{
#if defined(LLFIO_ENABLE_COROUTINES)
  //! [coroutines_example]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Create an i/o service for this thread
  llfio::io_service service;

  // Create an async file i/o handle attached to the i/o service for this thread
  llfio::async_file_handle h = llfio::async_file_handle::async_file(service, {}, "temp", llfio::file_handle::mode::write, llfio::file_handle::creation::if_needed, llfio::file_handle::caching::only_metadata, llfio::file_handle::flag::unlink_on_first_close).value();

  // Truncate to 1Mb
  (void) h.truncate(1024 * 4096);

  // Launch 8 coroutines, each writing 4Kb of chars 0-8 to every 32Kb block
  auto coroutine = [&h](size_t no) -> llfio::eager<void> {
    std::vector<llfio::byte, llfio::utils::page_allocator<llfio::byte>> buffer(4096);
    memset(buffer.data(), (int) ('0' + no), 4096);
    llfio::async_file_handle::const_buffer_type bt{buffer.data(), buffer.size()};
    for(size_t n = 0; n < 128; n++)
    {
      // This will initiate the i/o, and suspend the coroutine until completion.
      // The caller will thus resume execution with a valid unsignaled future.
      auto written = co_await h.co_write({{&bt, 1}, n * 32768 + no * 4096}).value();
      written.value();
    }
  };
  std::vector<llfio::eager<void>> coroutines;
  for(size_t n = 0; n < 8; n++)
  {
    // Construct each coroutine, initiating the i/o, then suspending.
    coroutines.push_back(coroutine(n));
  }
  // Pump the i/o, multiplexing the coroutines, until no more work remains.
  while(service.run().value())
    ;
  //! [coroutines_example]

  // Check that the file has the right contents
  alignas(4096) llfio::byte buffer1[4096], buffer2[4096];
  llfio::async_file_handle::extent_type offset = 0;
  for(size_t n = 0; n < 128; n++)
  {
    for(size_t m = 0; m < 8; m++)
    {
      memset(buffer2, (int) ('0' + m), 4096);
      h.read(offset, {{buffer1, 4096}}).value();
      BOOST_CHECK(!memcmp(buffer1, buffer2, 4096));
      offset += 4096;
    }
  }
#endif
}

static inline void TestPostSelfToRunCoroutines()
{
#if defined(LLFIO_ENABLE_COROUTINES)
  namespace llfio = LLFIO_V2_NAMESPACE;
  llfio::io_service service;
  std::atomic<bool> ready(false);
  auto runthreadid = QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id();
  // Start a coroutine which immediately suspends
  auto coroutine = [&]() -> llfio::lazy<void> {
    auto thisthreadid = QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id();
    BOOST_CHECK(thisthreadid != runthreadid);
    ready = true;
    co_await llfio::io_service::awaitable_post_to_self(service);
    thisthreadid = QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id();
    BOOST_CHECK(thisthreadid == runthreadid);
    // std::cout << "Coroutine exiting" << std::endl;
    ready = false;
  }();
  // Run the coroutine's body in another thread
  auto coroutinethread = [&]() -> void {
    // std::cout << "Thread waiting on coroutine" << std::endl;
    coroutine.await_suspend({});
    // std::cout << "Thread exiting" << std::endl;
  };
  auto asynch = std::async(std::launch::async, coroutinethread);
  while(!ready)
  {
    std::this_thread::yield();
  }
  while(ready)
  {
    service.run().value();
  }
  asynch.get();
#endif
}

KERNELTEST_TEST_KERNEL(integration, llfio, coroutines, async_file_handle, "Tests that llfio::async_file_handle works as expected with Coroutines", TestAsyncFileHandleCoroutines())
KERNELTEST_TEST_KERNEL(integration, llfio, coroutines, co_post_self_to_run, "Tests that llfio::io_service::co_post_self_to_run() works as expected with Coroutines", TestPostSelfToRunCoroutines())
