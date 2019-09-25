/* Integration test kernel for fast random file handle
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Oct 2018


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

#include "quickcpplib/algorithm/small_prng.hpp"


static inline void TestFastRandomFileHandleWorks()
{
  static constexpr size_t testbytes = 1024 * 1024UL;
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::byte;
  using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
  mapped<byte> store(testbytes);
  fast_random_file_handle h = fast_random_file_handle::fast_random_file(testbytes).value();
  // Bulk read
  BOOST_CHECK(h.read(0, {{store.begin(), store.size()}}).value() == store.size());

  // Now make sure that random read offsets and lengths match exactly
  small_prng rand;
  for(size_t n = 0; n < 10000; n++)
  {
    byte buffer[512];
    size_t offset = rand() % testbytes, length = rand() % 512;
    auto bytesread = h.read(offset, {{buffer, length}}).value();
    if(offset + length > testbytes)
    {
      BOOST_CHECK(offset + bytesread == testbytes);
    }
    else
    {
      BOOST_CHECK(bytesread == length);
    }
    BOOST_CHECK(!memcmp(buffer, store.begin() + offset, bytesread));
  }
}

static inline void TestFastRandomFileHandlePerformance()
{
  static constexpr size_t testbytes = 1024 * 1024 * 1024UL;
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::byte;
  mapped<byte> store(testbytes);
  memset(store.begin(), 1, store.size());
  fast_random_file_handle h = fast_random_file_handle::fast_random_file(testbytes).value();
  auto begin = std::chrono::high_resolution_clock::now();
  while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 1)
    ;
  QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng prng;
  begin = std::chrono::high_resolution_clock::now();
  for(size_t n = 0; n < 10; n++)
  {
    for(size_t i = 0; i < store.size(); i += 64)
    {
      *(uint32_t *) (&store[i + 0]) = prng();
      *(uint32_t *) (&store[i + 4]) = prng();
      *(uint32_t *) (&store[i + 8]) = prng();
      *(uint32_t *) (&store[i + 12]) = prng();
      *(uint32_t *) (&store[i + 16]) = prng();
      *(uint32_t *) (&store[i + 20]) = prng();
      *(uint32_t *) (&store[i + 24]) = prng();
      *(uint32_t *) (&store[i + 28]) = prng();
      *(uint32_t *) (&store[i + 32]) = prng();
      *(uint32_t *) (&store[i + 36]) = prng();
      *(uint32_t *) (&store[i + 40]) = prng();
      *(uint32_t *) (&store[i + 44]) = prng();
      *(uint32_t *) (&store[i + 48]) = prng();
      *(uint32_t *) (&store[i + 52]) = prng();
      *(uint32_t *) (&store[i + 56]) = prng();
      *(uint32_t *) (&store[i + 60]) = prng();
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  auto diff1 = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
  begin = std::chrono::high_resolution_clock::now();
  for(size_t n = 0; n < 10; n++)
  {
    h.read(0, {{store.begin(), store.size()}}).value();
  }
  end = std::chrono::high_resolution_clock::now();
  auto diff2 = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
  std::cout << "small_prng produces randomness at " << ((testbytes / 1024.0 / 1024.0) / (diff1.count() / 10000000.0)) << " Mb/sec" << std::endl;
  std::cout << "fast_random_file_handle produces randomness at " << ((testbytes / 1024.0 / 1024.0) / (diff2.count() / 10000000.0)) << " Mb/sec" << std::endl;
}

KERNELTEST_TEST_KERNEL(integration, llfio, fast_random_file_handle, works, "Tests that fast random file handle works as expected", TestFastRandomFileHandleWorks())
KERNELTEST_TEST_KERNEL(integration, llfio, fast_random_file_handle, performance, "Tests the performance of the fast random file handle", TestFastRandomFileHandlePerformance())
