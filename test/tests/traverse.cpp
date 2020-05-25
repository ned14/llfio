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

#ifndef _WIN32
#include <sys/resource.h>
#endif

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
    virtual result<directory_handle> directory_open_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, path_view leaf, size_t depth) noexcept override
    {
      if(error == errc::too_many_files_open)
      {
        return std::move(error);
      }
      failed_to_open.fetch_add(1, std::memory_order_relaxed);
      (void) data;
      (void) error;
      (void) dirh;
      (void) leaf;
      (void) depth;
      // std::cerr << "  Failed to open " << (dirh.current_path().value() / leaf.path()) << " due to " << error.message() << std::endl;
      return success();
    }
    virtual result<bool> pre_enumeration(void *data, const directory_handle &dirh, size_t depth) noexcept override
    {
      (void) data;
      (void) dirh;
      (void) depth;
#ifndef _WIN32
      if(root_dev_id == 0)
      {
        root_dev_id = dirh.st_dev();
      }
      else
      {
        // Don't enter other filesystems
        if(dirh.st_dev() != root_dev_id)
        {
          return false;
        }
      }
#endif
      return true;
    }
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept override
    {
      (void) data;
      (void) dirh;
      (void) depth;
      items_enumerated.fetch_add(contents.size(), std::memory_order_relaxed);
      // std::cerr << "  " << dirh.current_path().value() << std::endl;
      return success();
    }
    virtual result<void> stack_updated(void *data, size_t dirs_processed, size_t known_dirs_remaining, size_t depth_processed, size_t known_depth_remaining) noexcept override
    {
      (void) data;
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
#ifndef _WIN32
  {
    struct rlimit r
    {
      1024 * 1024, 1024 * 1024
    };
    setrlimit(RLIMIT_NOFILE, &r);
  }
#endif
#if 0
  {
    static constexpr size_t total_entries = 100000;  // create 100000 directories in each random directory tree
    using LLFIO_V2_NAMESPACE::file_handle;
    using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
    using QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string;
    small_prng rand;
    std::vector<directory_handle> dirhs;
    dirhs.reserve(total_entries);
    size_t entries_created = 1;
    dirhs.clear();
    dirhs.emplace_back(directory_handle::temp_directory().value());
    auto dirhpath = dirhs.front().current_path().value();
    std::cout << "\n\nCreating a random directory tree containing " << total_entries << " directories at " << dirhpath << " ..." << std::endl;
    auto begin = std::chrono::high_resolution_clock::now();
    for(size_t entries = 0; entries < total_entries; entries++)
    {
      const auto v = rand();
      const auto dir_idx = ((v >> 8) % dirhs.size());
      const auto file_entries = (v & 255);
      const auto &dirh = dirhs[dir_idx];
      filesystem::path::value_type buffer[10];
      {
        auto c = (uint32_t) entries;
        buffer[0] = 'd';
        to_hex_string(buffer + 1, 8, (const char *) &c, 4);
        buffer[9] = 0;
      }
      auto h = directory_handle::directory(dirh, path_view(buffer, 9, true), directory_handle::mode::write, directory_handle::creation::if_needed).value();
      for(size_t n = 0; n < file_entries; n++)
      {
        auto c = (uint8_t) n;
        buffer[0] = 'f';
        to_hex_string(buffer + 1, 2, (const char *) &c, 1);
        buffer[3] = 0;
        file_handle::file(h, path_view(buffer, 3, true), file_handle::mode::write, file_handle::creation::if_needed).value();
        entries_created++;
      }
      entries_created++;
      dirhs.emplace_back(std::move(h));
    }
    auto end = std::chrono::high_resolution_clock::now();
    dirhs.resize(1);
    std::cout << "Created " << entries_created << " filesystem entries in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (entries_created / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0))
              << " entries/sec).\n";
  }
#endif

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
