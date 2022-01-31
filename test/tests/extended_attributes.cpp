/* Integration test kernel for whether extended_attributes() works
(C) 2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Feb 2022


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

static inline void TestExtendedAttributes()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  llfio::byte buffer[65536];
  auto fh = llfio::file_handle::temp_file().value();
  std::cout << "NOTE: The temporary file for this test can be found at " << fh.current_path().value() << std::endl;
#ifdef _WIN32
  static const llfio::path_view_component name[] = {L"user.llfiotest.name2", L"user.llfiotest.name1", L"user.llfiotest.name3"};
#else
  static const llfio::path_view_component name[] = {"user.llfiotest.name2", "user.llfiotest.name1", "user.llfiotest.name3"};
#endif
  {
    auto attribs = fh.list_extended_attributes(buffer).value();
    BOOST_CHECK(attribs.size() == 0);
  }
  {
    fh.set_extended_attribute(name[2], llfio::span<const llfio::byte>{(const llfio::byte *) "I love you!", 11}).value();
  }
  {
    auto attribs = fh.list_extended_attributes(buffer).value();
    BOOST_REQUIRE(attribs.size() == 1);
    BOOST_CHECK(attribs.end() == std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[0]; }));
    BOOST_CHECK(attribs.end() == std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[1]; }));
    BOOST_CHECK(attribs.end() != std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[2]; }));
  }
  {
    fh.set_extended_attribute(name[0], llfio::span<const llfio::byte>{(const llfio::byte *) "World", 5}).value();
    fh.set_extended_attribute(name[1], llfio::span<const llfio::byte>{(const llfio::byte *) "Hello", 5}).value();
  }
  {
    auto attribs = fh.list_extended_attributes(buffer).value();
    BOOST_REQUIRE(attribs.size() == 3);
    BOOST_CHECK(attribs.end() != std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[0]; }));
    BOOST_CHECK(attribs.end() != std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[1]; }));
    BOOST_CHECK(attribs.end() != std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[2]; }));
  }
  {
    auto value1 = fh.get_extended_attribute(buffer, name[1]).value();
    BOOST_REQUIRE(value1.size() == 5);
    BOOST_CHECK(0 == memcmp(value1.data(), "Hello", 5));
    auto value2 = fh.get_extended_attribute(buffer, name[0]).value();
    BOOST_REQUIRE(value2.size() == 5);
    BOOST_CHECK(0 == memcmp(value2.data(), "World", 5));
    auto value3 = fh.get_extended_attribute(buffer, name[2]).value();
    BOOST_REQUIRE(value3.size() == 11);
    BOOST_CHECK(0 == memcmp(value3.data(), "I love you!", 11));
  }
  {
    fh.remove_extended_attribute(name[0]).value();
    auto attribs = fh.list_extended_attributes(buffer).value();
    BOOST_REQUIRE(attribs.size() == 2);
    BOOST_CHECK(attribs.end() == std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[0]; }));
    BOOST_CHECK(attribs.end() != std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[1]; }));
    BOOST_CHECK(attribs.end() != std::find_if(attribs.begin(), attribs.end(), [](const auto &i) { return i == name[2]; }));
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, extended_attributes, works, "Tests that extended_attributes() works as expected", TestExtendedAttributes())
