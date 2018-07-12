/* Integration test kernel for mapped view
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

static inline void TestMappedView1()
{
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::file_handle;
  file_handle fh = file_handle::file({}, "testfile", file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::all, file_handle::flag::unlink_on_first_close).value();
  fh.truncate(10000 * sizeof(int)).value();
  section_handle sh(section_handle::section(fh).value());
  mapped<int> v1(sh, 5);
  mapped<int> v2(sh);
  mapped<int> v3(50);
  BOOST_CHECK(v1.size() == 5);
  BOOST_CHECK(v2.size() == 10000);
  BOOST_CHECK(v3.size() == 50);
  v1[0] = 78;
  BOOST_CHECK(v2[0] == 78);
  v3[0] = 78;
  v3[49] = 5;
  BOOST_CHECK(v3[0] == 78);
  BOOST_CHECK(v3[49] == 5);
  try
  {
    // Overly large views must not extend the file until written to
    mapped<int> v4(sh, 20000);
    BOOST_CHECK(fh.maximum_extent().value() == 10000 * sizeof(int));
  }
  catch(...)
  {
#ifdef _WIN32
    // Unlike POSIX, Windows refuses to map a view exceeding the length of the file
    BOOST_CHECK(true);
#else
    BOOST_CHECK(false);
#endif
  }
}

static inline void TestMappedView2()
{
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::file_handle;
  using LLFIO_V2_NAMESPACE::byte;
  {
    std::error_code ec;
    filesystem::remove("testfile", ec);
  }
  mapped_file_handle mfh = mapped_file_handle::mapped_file(1024 * 1024, {}, "testfile", file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::all, file_handle::flag::unlink_on_first_close).value();
  BOOST_CHECK(mfh.address() == nullptr);
  mfh.truncate(10000 * sizeof(int)).value();
  byte *addr = mfh.address();
  BOOST_CHECK(addr != nullptr);

  map_view<int> v1(mfh);
  BOOST_CHECK(v1.size() == 10000);
  v1[0] = 78;
  v1[9999] = 79;
  mfh.truncate(20000 * sizeof(int)).value();
  BOOST_CHECK(addr == mfh.address());
  BOOST_CHECK(mfh.maximum_extent().value() == 20000 * sizeof(int));
  BOOST_CHECK(mfh.underlying_file_maximum_extent().value() == 20000 * sizeof(int));
  v1 = map_view<int>(mfh);
  BOOST_CHECK(v1.size() == 20000);
  BOOST_CHECK(v1[0] == 78);
  BOOST_CHECK(v1[9999] == 79);
  mfh.truncate(2 * 1024 * 1024).value();  // exceed reservation, cause hidden reserve
  BOOST_CHECK(addr != nullptr);
  v1 = map_view<int>(mfh);
  BOOST_CHECK(v1.size() == 2 * 1024 * 1024 / sizeof(int));
  BOOST_CHECK(v1[0] == 78);
  BOOST_CHECK(v1[9999] == 79);
  mfh.reserve(2 * 1024 * 1024).value();
  BOOST_CHECK(mfh.address() != nullptr);
  v1 = map_view<int>(mfh);
  BOOST_CHECK(v1.size() == 2 * 1024 * 1024 / sizeof(int));
  BOOST_CHECK(v1[0] == 78);
  BOOST_CHECK(v1[9999] == 79);
  mfh.truncate(1 * sizeof(int)).value();
  BOOST_CHECK(mfh.address() != nullptr);
  v1 = map_view<int>(mfh);
  BOOST_CHECK(v1.size() == 1);
  BOOST_CHECK(v1[0] == 78);

  // Use a different handle to extend the file
  mapped_file_handle mfh2 = mapped_file_handle::mapped_file(1024 * 1024, {}, "testfile", file_handle::mode::write, file_handle::creation::open_existing, file_handle::caching::all, file_handle::flag::unlink_on_first_close).value();
  mfh2.truncate(10000 * sizeof(int)).value();
  v1 = map_view<int>(mfh2);
  BOOST_CHECK(v1.size() == 10000);
  v1[0] = 78;
  v1[9999] = 79;
  // On Windows this will have updated the mapping, on POSIX it will not, so prod POSIX
  mfh.update_map().value();
  v1 = map_view<int>(mfh);
  BOOST_CHECK(v1.size() == 10000);
  BOOST_CHECK(v1[0] == 78);
  BOOST_CHECK(v1[9999] == 79);
  mfh2.map().close().value();
  mfh2.section().close().value();
  mfh.truncate(1 * sizeof(int)).value();

  // Use a normal file handle to extend the file
  file_handle fh = file_handle::file({}, "testfile", file_handle::mode::write, file_handle::creation::open_existing, file_handle::caching::all, file_handle::flag::unlink_on_first_close).value();
  fh.truncate(10000 * sizeof(int)).value();
  // On POSIX this will have updated the mapping, on Windows it will not, so prod Windows
  mfh.update_map().value();
  v1 = map_view<int>(mfh);
  BOOST_REQUIRE(v1.size() == 10000);
  BOOST_CHECK(v1[0] == 78);
  BOOST_CHECK(v1[9999] == 0);

  mfh.truncate(0).value();
  BOOST_CHECK(mfh.address() == nullptr);
}

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, mapped_span1, "Tests that llfio::map_view works as expected", TestMappedView1())
KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, mapped_span2, "Tests that llfio::map_view works as expected", TestMappedView2())
