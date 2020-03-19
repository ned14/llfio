/* Integration test kernel for large page support
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2018


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

static inline void TestLargeMemMappedPages()
{
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::byte;
  auto pagesizes = utils::page_sizes();
  if(pagesizes.size() == 1)
  {
    BOOST_TEST_MESSAGE("Large page support not available on this hardware, or to this privilege of user. So skipping this test.");
    return;
  }
  auto _mh(map_handle::map(1024 * 1024, false, section_handle::flag::readwrite | section_handle::flag::page_sizes_1));
#ifdef __APPLE__
  if(!_mh)
  {
    BOOST_TEST_MESSAGE("Large page support not available on this hardware, or to this privilege of user. So skipping this test.");
    return;
  }
#endif
  map_handle mh(std::move(_mh).value());
  BOOST_CHECK(mh.address() != nullptr);
  BOOST_CHECK(mh.page_size() == pagesizes[1]);
  BOOST_CHECK(mh.length() == pagesizes[1]);
  mh.write(0, {{(const byte *) "hello world", 11}}).value();
}

static inline void TestLargeKernelMappedPages()
{
#ifndef __APPLE__  // Mac OS only implements super pages for anonymous memory
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::file_handle;
  using LLFIO_V2_NAMESPACE::byte;
  auto pagesizes = utils::page_sizes();
  if(pagesizes.size() == 1)
  {
    BOOST_TEST_MESSAGE("Large page support not available on this hardware, or to this privilege of user. So skipping this test.");
    return;
  }
  // Windows appears to refuse section_handle::flag::page_sizes_1 during section creation when it is backed by a file
  auto _sh(section_handle::section(1024 * 1024, path_discovery::memory_backed_temporary_files_directory(), section_handle::flag::readwrite));
#ifdef _WIN32
  if(!_sh)
  {
    BOOST_TEST_MESSAGE("Failed to create kernel memory backed file on Windows with large pages which does not work until at least Windows 10 1803, so ignoring this test failure.");
    return;
  }
#endif
  section_handle sh(std::move(_sh).value());
  map_handle mh(map_handle::map(sh, 0, 0, section_handle::flag::readwrite | section_handle::flag::page_sizes_1).value());
  BOOST_CHECK(mh.address() != nullptr);
  BOOST_CHECK(mh.page_size() == pagesizes[1]);
  BOOST_CHECK(mh.length() == pagesizes[1]);
  mh.write(0, {{(const byte *) "hello world", 11}}).value();
#endif
}

static inline void TestLargeFileMappedPages()
{
#ifndef __APPLE__  // Mac OS only implements super pages for anonymous memory
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::file_handle;
  using LLFIO_V2_NAMESPACE::byte;
  auto pagesizes = utils::page_sizes();
  if(pagesizes.size() == 1)
  {
    BOOST_TEST_MESSAGE("Large page support not available on this hardware, or to this privilege of user. So skipping this test.");
    return;
  }
  file_handle fh = file_handle::file({}, "testfile", file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::all, file_handle::flag::unlink_on_first_close).value();
  fh.truncate(1024 * 1024).value();
  // Windows appears to refuse section_handle::flag::page_sizes_1 during section creation when it is backed by a file
  auto _sh(section_handle::section(fh, 0, section_handle::flag::readwrite));
  section_handle sh(std::move(_sh).value());
  auto _mh(map_handle::map(sh, 0, 0, section_handle::flag::readwrite | section_handle::flag::page_sizes_1));
#ifdef _WIN32
  if(!_mh)
  {
    BOOST_TEST_MESSAGE("Failed to create large page map of file which does not work until at least Windows 10 1803, so ignoring this test failure.");
    return;
  }
#endif
  map_handle mh(std::move(_mh).value());
  BOOST_CHECK(mh.address() != nullptr);
  BOOST_CHECK(mh.page_size() == pagesizes[1]);
  BOOST_CHECK(mh.length() == pagesizes[1]);
  mh.write(0, {{(const byte *) "hello world", 11}}).value();
#endif
}

KERNELTEST_TEST_KERNEL(integration, llfio, map_handle, large_mem_mapped_pages, "Tests that large page support for allocating memory works as expected", TestLargeMemMappedPages())
KERNELTEST_TEST_KERNEL(integration, llfio, map_handle, large_kernel_mapped_pages, "Tests that large page support for mapping kernel memory works as expected", TestLargeKernelMappedPages())
KERNELTEST_TEST_KERNEL(integration, llfio, map_handle, large_file_mapped_pages, "Tests that large page support for mapping files works as expected", TestLargeFileMappedPages())
