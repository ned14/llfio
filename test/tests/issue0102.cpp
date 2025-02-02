/* Integration test kernel for issue #73 Windows directory junctions cannot be opened with symlink_handle
(C) 2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Sept 2022


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

static inline void TestIssue0102()
{
#if !LLFIO_EXPERIMENTAL_STATUS_CODE || !defined(__cpp_exceptions)
  return;
#else
  namespace llfio = LLFIO_V2_NAMESPACE;
  namespace outcome_e = OUTCOME_V2_NAMESPACE::experimental;

  auto page = llfio::utils::page_allocator<unsigned char>().allocate(llfio::utils::page_size());

  llfio::file_io_error ioError = llfio::generic_error(llfio::errc::state_not_recoverable);

  // (I know this is very likely to be misaligned on 64Bit systems, but
  //  otherwise the problem/segfault is only triggered on 32Bit systems)
  auto erasedError = new(page + llfio::utils::page_size() - sizeof(outcome_e::status_code_domain *) - sizeof(int))
  outcome_e::erased_errored_status_code<int>(outcome_e::in_place, ioError);

  // the following line must not segfault
  std::cout << erasedError->message() << std::endl;
#endif
}

KERNELTEST_TEST_KERNEL(regression, llfio, issues, 0102, "Tests issue #0102 Using a status code converted from a file_io_error segfaults", TestIssue0102())
