/* Integration test kernel for whether path discovery works
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2017


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

static inline void TestPathDiscovery()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto raw_list = llfio::path_discovery::all_temporary_directories();
  std::cout << "The raw list of temporary directory paths on this system are:" << std::endl;
  for(auto &i : raw_list)
  {
    std::cout << "   " << i.path << " (" << i.source << ")" << std::endl;
  }

  auto verified_list = llfio::path_discovery::verified_temporary_directories();
  std::cout << "\nThe verified list of temporary directory paths on this system are:" << std::endl;
  for(auto &i : verified_list)
  {
    std::cout << "   " << i.path << " (" << i.source << ")" << std::endl;
  }

  auto &storage_backed = llfio::path_discovery::storage_backed_temporary_files_directory();
  if(storage_backed.is_valid())
  {
    std::cout << "\nThe storage backed temporary files directory chosen is:\n   " << storage_backed.current_path().value() << std::endl;
  }
  else
  {
    std::cout << "\nNo storage backed temporary files directory found!" << std::endl;
    BOOST_CHECK(false);  // this is extremely unlikely to occur, so fail the test
  }

  auto &memory_backed = llfio::path_discovery::memory_backed_temporary_files_directory();
  if(memory_backed.is_valid())
  {
    std::cout << "\nThe memory backed temporary files directory chosen is:\n   " << memory_backed.current_path().value() << std::endl;
  }
  else
  {
    std::cout << "\nNo memory backed temporary files directory found!" << std::endl;
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, path_discovery, temp_directories, "Tests that llfio::path_discovery works as expected", TestPathDiscovery())
