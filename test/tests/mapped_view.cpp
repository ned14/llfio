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

#include "../../include/afio/afio.hpp"
#include "kerneltest/include/kerneltest.hpp"

static inline void TestMappedView()
{
  using namespace AFIO_V2_NAMESPACE;
  using AFIO_V2_NAMESPACE::file_handle;
  file_handle fh = file_handle::file({}, "testfile", file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::all, file_handle::flag::unlink_on_close).value();
  fh.truncate(10000 * sizeof(int)).value();
  section_handle sh(section_handle::section(fh).value());
  algorithm::mapped_view<int> v1(sh, 5);
  algorithm::mapped_view<int> v2(sh);
  algorithm::mapped_view<int> v3(50);
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
    algorithm::mapped_view<int> v4(sh, 20000);
    BOOST_CHECK(fh.length().value() == 10000 * sizeof(int));
  }
  catch(...)
  {
    // Unlike POSIX, Windows refuses to map a view exceeding the length of the file
    BOOST_CHECK(true);
  }
}

KERNELTEST_TEST_KERNEL(integration, afio, algorithm, mapped_view, "Tests that afio::algorithm::mapped_view works as expected", TestMappedView())
