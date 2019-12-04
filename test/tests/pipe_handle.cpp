/* Integration test kernel for whether pipe handles work
(C) 2019 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Nov 2019


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

static inline void TestBlockingPipeHandle()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto reader = std::async([] {  // This immediately blocks in blocking mode
    llfio::pipe_handle reader = llfio::pipe_handle::pipe_create("llfio-pipe-handle-test").value();
    llfio::byte buffer[64];
    auto read = reader.read(0, {{buffer, 64}}).value();
    BOOST_REQUIRE(read == 5);
    BOOST_CHECK(0 == memcmp(buffer, "hello", 5));
    reader.close().value();
  });
  auto begin = std::chrono::steady_clock::now();
  while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() < 100)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  if(std::future_status::ready == reader.wait_for(std::chrono::seconds(0)))
  {
    reader.get();
  }
  llfio::pipe_handle writer;
  begin = std::chrono::steady_clock::now();
  while(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - begin).count() < 1000)
  {
    auto r = llfio::pipe_handle::pipe_open("llfio-pipe-handle-test");
    if(r)
    {
      writer = std::move(r.value());
      break;
    }
  }
  BOOST_REQUIRE(writer.is_valid());
  auto written = writer.write(0, {{(const llfio::byte *) "hello", 5}}).value();
  BOOST_REQUIRE(written == 5);
  writer.barrier().value();
  writer.close().value();
  reader.get();
}

KERNELTEST_TEST_KERNEL(integration, llfio, path_handle, works, "Tests that blocking llfio::pipe_handle works as expected", TestBlockingPipeHandle())
