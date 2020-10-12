/* Integration test kernel for issue #9 map_handle and mapped_file_handle are very slow with large address reservations on Windows
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Sept 2020


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

static inline void TestIssue09a()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto fh = llfio::file_handle::temp_file().value();
  fh.truncate(1).value();
  auto sh = llfio::section_handle::section(fh).value();
  sh.truncate(1ULL << 40ULL).value();
#ifdef _WIN32
  auto mh = llfio::map_handle::map(sh, 1ULL << 35ULL).value();
  auto *addr1 = mh.address();
#else
  auto mh = llfio::map_handle::map(sh, 1ULL << 40ULL).value();
  auto *addr1 = mh.address();
  mh.truncate(1ULL << 35ULL).value();
  {
    auto *addr0 = mh.address();
    BOOST_CHECK(addr1 == addr0);
  }
#endif
  mh.truncate(1ULL << 36ULL).value();
  {
    auto *addr2 = mh.address();
    BOOST_CHECK(addr1 == addr2);
  }
  mh.truncate(1ULL << 37ULL).value();
  {
    auto *addr2 = mh.address();
    BOOST_CHECK(addr1 == addr2);
  }
  mh.truncate(1ULL << 38ULL).value();
  {
    auto *addr2 = mh.address();
    BOOST_CHECK(addr1 == addr2);
  }
  mh.truncate(1ULL << 39ULL).value();
  {
    auto *addr2 = mh.address();
    BOOST_CHECK(addr1 == addr2);
  }
  mh.truncate(1ULL << 40ULL).value();
  {
    auto *addr2 = mh.address();
    BOOST_CHECK(addr1 == addr2);
  }
  auto begin = std::chrono::steady_clock::now();
  mh.close().value();
  sh.close().value();
  fh.close().value();
  auto end = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
  std::cout << "Closing a map_handle (file) with six, appended, very large reservations up to 2^40 took " << diff << "ms." << std::endl;
  BOOST_CHECK(diff < 250);
}

static inline void TestIssue09b()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto mh = llfio::map_handle::reserve(1ULL << 43ULL).value();
  const auto sixth = mh.length() / 6;
  mh.commit({mh.address() + sixth * 0, 1}).value();
  mh.commit({mh.address() + sixth * 1, 1}).value();
  mh.commit({mh.address() + sixth * 2, 1}).value();
  mh.commit({mh.address() + sixth * 3, 1}).value();
  mh.commit({mh.address() + sixth * 4, 1}).value();
  mh.commit({mh.address() + sixth * 5, 1}).value();
  auto begin = std::chrono::steady_clock::now();
  mh.do_not_store({mh.address(), mh.length()}).value();
  auto end = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
  std::cout << "Discard a map_handle (reservation) with six small commits within a reservation of 2^43 took " << diff << "ms." << std::endl;
  BOOST_CHECK(diff < 250);
}

static inline void TestIssue09c()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto begin = std::chrono::steady_clock::now();
  auto mfh = llfio::mapped_file_handle::mapped_temp_file(1ULL << 40ULL).value();
  mfh.truncate(100 * 1024).value();
  mfh.close().value();
  auto end = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
  std::cout << "Construct and tear down a small mapped_file_handle with a reservation of 2^40 took " << diff << "ms." << std::endl;
#ifdef __APPLE_
  BOOST_CHECK(diff < 2500);
#else
  BOOST_CHECK(diff < 250);
#endif
}

KERNELTEST_TEST_KERNEL(regression, llfio, issues, 9a,
                       "Tests issue #9 map_handle and mapped_file_handle are very slow with large address reservations on Windows", TestIssue09a())
KERNELTEST_TEST_KERNEL(regression, llfio, issues, 9b,
                       "Tests issue #9 map_handle and mapped_file_handle are very slow with large address reservations on Windows", TestIssue09b())
KERNELTEST_TEST_KERNEL(regression, llfio, issues, 9c,
                       "Tests issue #9 map_handle and mapped_file_handle are very slow with large address reservations on Windows", TestIssue09c())
