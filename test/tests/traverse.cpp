/* Integration test kernel for whether traverse() works
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: May 2020


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

#include <chrono>

#include "quickcpplib/algorithm/small_prng.hpp"
#include "quickcpplib/algorithm/string.hpp"

static inline void TestTraverse()
{
  using namespace LLFIO_V2_NAMESPACE;
#ifdef _WIN32
  static constexpr path_view to_traverse_path("c:\\");
#else
  static constexpr path_view to_traverse_path("/");
#endif
  auto to_traverse = path_handle::path(to_traverse_path).value();
  struct my_traverse_visitor final : algorithm::traverse_visitor
  {
    directory_handle::dev_t root_dev_id{0};
    std::atomic<size_t> failed_to_open{0}, items_enumerated{0}, max_depth{0}, count{0};
    virtual result<directory_handle> directory_open_failed(result<void>::error_type &&error, const directory_handle &dirh, path_view leaf) noexcept override
    {
      if(error == errc::too_many_files_open)
      {
        return std::move(error);
      }
      failed_to_open.fetch_add(1, std::memory_order_relaxed);
      (void) error;
      (void) dirh;
      (void) leaf;
      //std::cerr << "  Failed to open " << (dirh.current_path().value() / leaf.path()) << " due to " << error.message() << std::endl;
      return success();
    }
    virtual result<bool> pre_enumeration(const directory_handle &dirh) noexcept override
    {
      if(root_dev_id == 0)
      {
        root_dev_id = dirh.st_dev();
      }
      else
      {
        // Don't enter other filesystems
        if(dirh.st_dev()!=root_dev_id)
        {
          return false;
        }
      }
      return true;
    }
    virtual result<void> post_enumeration(const directory_handle &dirh, directory_handle::buffers_type &contents) noexcept override
    {
      (void) dirh;
      items_enumerated.fetch_add(contents.size(), std::memory_order_relaxed);
      //std::cerr << "  " << dirh.current_path().value() << std::endl;
      return success();
    }
    virtual result<void> stack_updated(size_t dirs_processed, size_t known_dirs_remaining, size_t depth_processed, size_t known_depth_remaining) noexcept override
    {
      (void) dirs_processed;
      (void) known_dirs_remaining;
      auto m = depth_processed + known_depth_remaining;
      if(m > max_depth.load(std::memory_order_relaxed))
      {
        max_depth.store(m, std::memory_order_relaxed);
      }
      if((count.fetch_add(1, std::memory_order_relaxed) & 65535) == 0)
      {
        auto total = dirs_processed + known_dirs_remaining;
        std::cout << "  " << (100.0 * dirs_processed / total) << "%" << std::endl;
      }
      return success();
    }
  };

  std::cout << "Traversing " << to_traverse_path << " using a single thread ..." << std::endl;
  my_traverse_visitor visitor_st;
  auto begin = std::chrono::high_resolution_clock::now();
  size_t items_st = algorithm::traverse(to_traverse, &visitor_st, 1).value();
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "  Traversed " << items_st << " directories on " << to_traverse_path << " in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (items_st / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0))
            << " directories/sec).\n";
  std::cout << "     Enumerated a total of " << visitor_st.items_enumerated << " items. " << visitor_st.failed_to_open << " directories could not be opened. Maximum hierarchy depth was " << visitor_st.max_depth << std::endl;
  BOOST_CHECK(items_st > 0);

  std::cout << "Traversing " << to_traverse_path << " using many threads ..." << std::endl;
  my_traverse_visitor visitor_mt;
  begin = std::chrono::high_resolution_clock::now();
  auto items_mt = algorithm::traverse(to_traverse, &visitor_mt).value();
  end = std::chrono::high_resolution_clock::now();
  std::cout << "  Traversed " << items_mt << " directories on " << to_traverse_path << " in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (items_mt / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0))
            << " directories/sec).\n";
  std::cout << "     Enumerated a total of " << visitor_mt.items_enumerated << " items. " << visitor_mt.failed_to_open << " directories could not be opened. Maximum hierarchy depth was " << visitor_mt.max_depth << std::endl;
  BOOST_CHECK(abs((int) items_st - (int) items_mt) < 5);
  BOOST_CHECK(visitor_st.failed_to_open == visitor_mt.failed_to_open);
  BOOST_CHECK(abs((int) visitor_st.items_enumerated - (int) visitor_mt.items_enumerated) < 5);
  BOOST_CHECK(visitor_st.max_depth == visitor_mt.max_depth);
}

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, traverse, "Tests that llfio::algorithm::traverse() works as expected", TestTraverse())
