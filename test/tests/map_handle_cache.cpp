/* Integration test kernel for whether the map handle cache works
(C) 2021 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2021


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

#include <deque>
#include <list>

static inline void TestMapHandleCache()
{
  static constexpr size_t ITEMS_COUNT = 10000;
  namespace llfio = LLFIO_V2_NAMESPACE;
  bool free_cache_immediately = false;
  auto test = [&] {
    auto fault = [](llfio::map_handle &mh) {
      for(auto *p = (volatile char *) mh.address(); p < (volatile char *) mh.address() + mh.length(); p += mh.page_size())
      {
        *p = 1;
      }
    };
    QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng rand;
    std::vector<llfio::map_handle> maps;
    for(size_t n = 0; n < ITEMS_COUNT; n++)
    {
      auto v = rand();
      auto toallocate = (v >> 2) & (128 * 1024 - 1);
      if(toallocate == 0)
      {
        toallocate = 1;
      }
      maps.push_back(llfio::map_handle::map(toallocate, free_cache_immediately).value());
      fault(maps.back());
    }
    if(free_cache_immediately)
    {
      auto stats = llfio::map_handle::trim_cache(std::chrono::steady_clock::now());
      BOOST_REQUIRE(stats.bytes_in_cache == 0);
      BOOST_REQUIRE(stats.items_in_cache == 0);
    }
    auto begin = std::chrono::steady_clock::now();
    for(size_t n = 0; n < ITEMS_COUNT * 10; n++)
    {
      auto v = rand();
      auto toallocate = (v >> 2) & (128 * 1024 - 1);
      if(toallocate == 0)
      {
        toallocate = 1;
      }
      if(v & 1)
      {
        maps[n % ITEMS_COUNT].close().value();
      }
      else
      {
        fault((maps[n % ITEMS_COUNT] = llfio::map_handle::map(toallocate, false).value()));
      }
      if(free_cache_immediately)
      {
        auto stats = llfio::map_handle::trim_cache(std::chrono::steady_clock::now());
        BOOST_CHECK(stats.bytes_in_cache == 0);
        BOOST_CHECK(stats.items_in_cache == 0);
      }
    }
    auto end = std::chrono::steady_clock::now();
    {
      auto stats = llfio::map_handle::trim_cache();
      std::cout << "\nIn the map_handle cache after churn there are " << stats.bytes_in_cache << " bytes in the cache in " << stats.items_in_cache << " items."
                << std::endl;
    }
    for(auto &i : maps)
    {
      i.close().value();
    }
    {
      auto stats = llfio::map_handle::trim_cache();
      std::cout << "\nIn the map_handle cache after releasing everything there are " << stats.bytes_in_cache << " bytes in the cache in "
                << stats.items_in_cache << " items." << std::endl;
    }
    std::cout << "With free_cache_immediately = " << free_cache_immediately << " it took "
              << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1000.0 / ITEMS_COUNT) << " us per allocation-free." << std::endl;
  };
  test();
  free_cache_immediately = true;
  test();
}

KERNELTEST_TEST_KERNEL(integration, llfio, map_handle, cache, "Tests that the map_handle cache works as expected", TestMapHandleCache())
