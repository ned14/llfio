/* Test kernel for section_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (3 commits)
File Created: August 2016


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

#include "../../test_kernel_decl.hpp"

namespace section_handle_create_close
{
  AFIO_TEST_KERNEL_DECL AFIO_V2_NAMESPACE::result<AFIO_V2_NAMESPACE::section_handle> test_kernel_section_handle(AFIO_V2_NAMESPACE::file_handle &backing, AFIO_V2_NAMESPACE::section_handle::extent_type maximum_size, AFIO_V2_NAMESPACE::section_handle::flag m)
  {
    auto h = backing.is_valid() ? AFIO_V2_NAMESPACE::section_handle::section(backing, maximum_size, m) : AFIO_V2_NAMESPACE::section_handle::section(maximum_size, AFIO_V2_NAMESPACE::path_discovery::storage_backed_temporary_files_directory(), m);
    return h;
  }
}
