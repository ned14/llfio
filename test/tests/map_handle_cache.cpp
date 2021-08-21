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

inline QUICKCPPLIB_NOINLINE void fault(LLFIO_V2_NAMESPACE::map_handle &mh)
{
  for(auto *p = (volatile char *) mh.address(); p < (volatile char *) mh.address() + mh.length(); p += mh.page_size())
  {
    *p = 1;
  }
};

static inline void TestMapHandleCache()
{
  static constexpr size_t ITEMS_COUNT = 10000;
  namespace llfio = LLFIO_V2_NAMESPACE;
  bool free_cache_immediately = true;
  auto test = [&] {
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
    size_t ops = 0;
    for(size_t n = 0; n < ITEMS_COUNT * 100; n++)
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
        ops++;
      }
      else
      {
        fault((maps[n % ITEMS_COUNT] = llfio::map_handle::map(toallocate, false).value()));
        ops += 2;
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
      auto usage = llfio::utils::current_process_memory_usage().value();
      std::cout << "\n\nIn the map_handle cache after churn there are " << (stats.bytes_in_cache / 1024.0 / 1024.0) << " Mb in the cache in "
                << stats.items_in_cache << " items. There were " << stats.hits << " hits and " << stats.misses
                << " misses. Process virtual address space used is " << (usage.total_address_space_in_use / 1024.0 / 1024.0 / 1024.0)
                << " Gb and commit charge is " << (usage.private_committed / 1024.0 / 1024.0) << " Mb." << std::endl;
    }
    for(auto &i : maps)
    {
      i.close().value();
    }
    {
      auto stats = llfio::map_handle::trim_cache();
      auto usage = llfio::utils::current_process_memory_usage().value();
      std::cout << "\nIn the map_handle cache after releasing everything there are " << (stats.bytes_in_cache / 1024.0 / 1024.0) << " Mb in the cache in "
                << stats.items_in_cache << " items. Process virtual address space used is " << (usage.total_address_space_in_use / 1024.0 / 1024.0 / 1024.0)
                << " Gb and commit charge is " << (usage.private_committed / 1024.0 / 1024.0) << " Mb." << std::endl;
    }
    std::cout << "\nWith free_cache_immediately = " << free_cache_immediately << " it took "
              << (std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin).count() / 1000.0 / ops) << " us per allocation-free."
              << std::endl;
  };
  test();
  free_cache_immediately = false;
  test();
}

KERNELTEST_TEST_KERNEL(integration, llfio, map_handle, cache, "Tests that the map_handle cache works as expected", TestMapHandleCache())
