/* Test kernel for map_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

namespace directory_handle_enumerate
{
  LLFIO_TEST_KERNEL_DECL LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle::enumerate_info> test_kernel_directory_handle_enumerate(LLFIO_V2_NAMESPACE::span<LLFIO_V2_NAMESPACE::directory_entry> *buffers, LLFIO_V2_NAMESPACE::path_view glob, LLFIO_V2_NAMESPACE::directory_handle::filter filtering)
  {
    OUTCOME_TRY(h, LLFIO_V2_NAMESPACE::directory_handle::directory({}, "."));
    auto ret = h.enumerate(*buffers, glob, filtering);
    h.close().value();
    return ret;
  }
}
