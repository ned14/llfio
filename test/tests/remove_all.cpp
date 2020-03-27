/* Integration test kernel for whether remove_all() works
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Mar 2020


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

static inline void TestRemoveAll()
{
  static constexpr size_t rounds = 100;
  static constexpr size_t total_entries = 100;  // create 100 directories in each random directory tree
  using namespace LLFIO_V2_NAMESPACE;
  using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
  using QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string;
  small_prng rand;
  std::vector<directory_handle> dirhs;
  dirhs.reserve(total_entries);
  for(size_t round = 0; round < rounds; round++)
  {
    size_t entries_created = 0;
    dirhs.clear();
    dirhs.emplace_back(directory_handle::temp_directory().value());
    std::cout << "\n\nCreating a random directory tree containing " << total_entries << " directories ..." << std::endl;
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
    std::cout << "Created " << entries_created << " filesystem entries in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (entries_created / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0)) << " entries/sec).\n";

    std::cout << "\nCalling llfio::algorithm::remove_all() on that randomised directory tree ..." << std::endl;
    begin = std::chrono::high_resolution_clock::now();
    algorithm::remove_all(std::move(dirhs.front())).value();
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Removed " << entries_created << " filesystem entries in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (entries_created / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0)) << " entries/sec).\n";
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, remove_all, "Tests that llfio::algorithm::remove_all() works as expected", TestRemoveAll())
