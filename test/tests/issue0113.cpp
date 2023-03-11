/* Integration test kernel for issue #73 Windows directory junctions cannot be opened with symlink_handle
(C) 2023 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Mar 2023


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

static inline void TestIssue0113()
{
#if QUICKCPPLIB_USE_STD_SPAN
  namespace llfio = LLFIO_V2_NAMESPACE;
  std::span<llfio::byte> x;
  std::span<const llfio::byte> y;
  llfio::file_handle::buffer_type a(x);
  llfio::file_handle::buffer_type b = x;
  llfio::file_handle::buffer_type c = {x};
  llfio::file_handle::buffer_type d[] = {{x}};
  llfio::file_handle::const_buffer_type e[] = {{y}};
  llfio::file_handle::const_buffer_type f[] = {{x}, {y}};
  (void) a;
  (void) b;
  (void) c;
  (void) d;
  (void) e;
  (void) f;
#endif
  BOOST_CHECK(true);
}

KERNELTEST_TEST_KERNEL(regression, llfio, issues, 0113, "Tests issue #0113 const_buffer_type has ambiguous construction from std::span<const byte>",
                       TestIssue0113())
