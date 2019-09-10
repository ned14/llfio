/* Integration test kernel for issue #27 Enumerating empty directories causes infinite loop
(C) 2019 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Sept 2019


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

static inline void TestIssue27()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto dh = llfio::directory({}, "testdir", llfio::directory_handle::mode::write, llfio::directory_handle::creation::if_needed).value();
  llfio::directory_entry buffer[16];
  auto contents = dh.read({buffer}).value();
  BOOST_CHECK_EQUAL(contents.size(), 0);
}

KERNELTEST_TEST_KERNEL(regression, llfio, issues, 27, "Tests issue #27 Enumerating empty directories causes infinite loop", TestIssue27())
