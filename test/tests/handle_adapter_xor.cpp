/* Integration test kernel for the xor handle adapter
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Nov 2018


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

#ifdef __has_include
#if __has_include("../../../include/llfio/v2.0/quickcpplib/include/algorithm/small_prng.hpp")
#include "../../../include/llfio/v2.0/quickcpplib/include/algorithm/small_prng.hpp"
#else
#include "quickcpplib/include/algorithm/small_prng.hpp"
#endif
#else
#include "../../../include/llfio/v2.0/quickcpplib/include/algorithm/small_prng.hpp"
#endif


static inline void TestXorHandleAdapterWorks()
{
  static constexpr size_t testbytes = 1024 * 1024UL;
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::byte;
  using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
  fast_random_file_handle h1 = fast_random_file_handle::fast_random_file(testbytes).value();
  mapped_file_handle h2 = mapped_file_handle::mapped_temp_inode().value();
  // Make temp inode have same contents as random file
  h2.truncate(testbytes).value();
  h1.read(0, {{h2.address(), testbytes}}).value();
  
  // Configure the XOR handle adapter for the two handles
  algorithm::xor_handle_adapter<mapped_file_handle, fast_random_file_handle> h(&h2, &h1);
  BOOST_CHECK(h.is_readable());
  BOOST_CHECK(h.is_writable());
  BOOST_CHECK(h.maximum_extent().value() == testbytes);
  
  // Read the whole of the XOR handle adapter, it should return all bits zero
  mapped<byte> tempbuffer(testbytes);
  BOOST_CHECK(h.read(0, {{tempbuffer.data(), tempbuffer.size()}}).value() == testbytes);
  for(size_t n = 0; n < testbytes / 8; n++)
  {
    uint64_t *p = (uint64_t *) tempbuffer.data();
    BOOST_CHECK(p[n] == 0);
  }

#if 0
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
#endif
}

#if 0
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
#endif

KERNELTEST_TEST_KERNEL(integration, llfio, xor_handle_adapter, works, "Tests that the xor handle adapter works as expected", TestXorHandleAdapterWorks())
//KERNELTEST_TEST_KERNEL(integration, llfio, fast_random_file_handle, performance, "Tests the performance of the fast random file handle", TestFastRandomFileHandlePerformance())
