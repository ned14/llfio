/* Integration test kernel for issue #28 map_handle is calling NtMapViewOfSection with an invalid ViewSize
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Jan 2020


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

static inline void TestIssue28()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  {
    auto oh = llfio::mapped_temp_file(4200).value();
    BOOST_CHECK_EQUAL(oh.capacity(), 4200);
    BOOST_CHECK_EQUAL(oh.map().capacity(), 0);
    oh.truncate(4100).value();
    BOOST_CHECK_EQUAL(oh.capacity(), 4200);
    BOOST_CHECK_EQUAL(oh.map().capacity(), llfio::utils::round_up_to_page_size(4200u, oh.page_size()));
    BOOST_CHECK_EQUAL(oh.map().length(), 4100);
    BOOST_CHECK_EQUAL(oh.maximum_extent().value(), 4100);  // calls update_map()
    BOOST_CHECK_EQUAL(oh.capacity(), 4200);
    BOOST_CHECK_EQUAL(oh.map().capacity(), llfio::utils::round_up_to_page_size(4200u, oh.page_size()));
    BOOST_CHECK_EQUAL(oh.map().length(), 4100);

    auto ih = llfio::mapped_file(8100, {}, oh.current_path().value(), llfio::mapped_file_handle::mode::write).value();
    BOOST_CHECK_EQUAL(ih.capacity(), 8100);
    BOOST_CHECK_EQUAL(ih.map().capacity(), llfio::utils::round_up_to_page_size(8100u, ih.page_size()));
    BOOST_CHECK_EQUAL(ih.map().length(), 4100);
    BOOST_CHECK_EQUAL(ih.maximum_extent().value(), 4100);  // calls update_map()
    BOOST_CHECK_EQUAL(ih.capacity(), 8100);
    BOOST_CHECK_EQUAL(ih.map().capacity(), llfio::utils::round_up_to_page_size(8100u, ih.page_size()));
    BOOST_CHECK_EQUAL(ih.map().length(), 4100);

    oh.truncate(12100).value();
    BOOST_CHECK_EQUAL(oh.capacity(), 12100);
    BOOST_CHECK_EQUAL(oh.map().capacity(), llfio::utils::round_up_to_page_size(12100u, oh.page_size()));
    BOOST_CHECK_EQUAL(oh.map().length(), 12100);
    BOOST_CHECK_EQUAL(oh.maximum_extent().value(), 12100);  // calls update_map()
    BOOST_CHECK_EQUAL(oh.capacity(), 12100);
    BOOST_CHECK_EQUAL(oh.map().capacity(), llfio::utils::round_up_to_page_size(12100u, oh.page_size()));
    BOOST_CHECK_EQUAL(oh.map().length(), 12100);

    BOOST_CHECK_EQUAL(ih.capacity(), 8100);
    BOOST_CHECK_EQUAL(ih.map().capacity(), llfio::utils::round_up_to_page_size(8100u, ih.page_size()));
    BOOST_CHECK_EQUAL(ih.map().length(), 4100);
    BOOST_CHECK_EQUAL(ih.maximum_extent().value(), 8100);  // must not exceed reservation
    BOOST_CHECK_EQUAL(ih.capacity(), 8100);
    BOOST_CHECK_EQUAL(ih.map().capacity(), llfio::utils::round_up_to_page_size(8100u, ih.page_size()));
    BOOST_CHECK_EQUAL(ih.map().length(), 8100);
  }  // namespace llfio=LLFIO_V2_NAMESPACE;
  {
    auto oh = llfio::mapped_temp_file(4200).value();
    auto ih = llfio::mapped_file(8100, {}, oh.current_path().value(), llfio::mapped_file_handle::mode::write).value();
    oh.truncate(6100).value();
    BOOST_CHECK_EQUAL(oh.maximum_extent().value(), 6100);
    BOOST_CHECK_EQUAL(ih.maximum_extent().value(), 6100);
  }  // namespace llfio=LLFIO_V2_NAMESPACE;
  {
    // This line will fail to set file to sparse due to Access Denied, caused by using a read-only mode to create a file :)
    auto oh = llfio::mapped_temp_file(8100, {}, llfio::mapped_file_handle::mode::read, llfio::mapped_file_handle::creation::if_needed,
                                      llfio::mapped_file_handle::caching::temporary, llfio::mapped_file_handle::flag::none)
              .value();
    auto ih = llfio::mapped_file(8100, {}, oh.current_path().value(), llfio::mapped_file_handle::mode::write, llfio::mapped_file_handle::creation::if_needed,
                                 llfio::mapped_file_handle::caching::temporary, llfio::mapped_file_handle::flag::unlink_on_first_close)
              .value();
    ih.truncate(6100).value();
    BOOST_CHECK_EQUAL(oh.maximum_extent().value(), 6100);
    BOOST_CHECK_EQUAL(ih.maximum_extent().value(), 6100);
  }  // namespace llfio=LLFIO_V2_NAMESPACE;
  {
    auto oh = llfio::mapped_temp_file(4200).value();
    oh.truncate(4100).value();
    BOOST_CHECK_EQUAL(oh.maximum_extent().value(), 4100);
    auto ih = llfio::mapped_file(8100, {}, oh.current_path().value()).value();
    oh.truncate(12100).value();
    BOOST_CHECK_EQUAL(ih.maximum_extent().value(), 8100);  // must not exceed reservation
  }  // namespace llfio=LLFIO_V2_NAMESPACE;
  {
    auto oh = llfio::mapped_temp_file(4200).value();
    auto ih = llfio::mapped_file(8100, {}, oh.current_path().value()).value();
    oh.truncate(6100).value();
    BOOST_CHECK_EQUAL(oh.maximum_extent().value(), 6100);
    BOOST_CHECK_EQUAL(ih.maximum_extent().value(), 6100);
  }  // namespace llfio=LLFIO_V2_NAMESPACE;
}

KERNELTEST_TEST_KERNEL(regression, llfio, issues, 28, "Tests issue #28 map_handle is calling NtMapViewOfSection with an invalid ViewSize", TestIssue28())
