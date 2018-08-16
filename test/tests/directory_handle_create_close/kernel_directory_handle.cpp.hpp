/* Test kernel for directory_handle create and close
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (5 commits)
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

#include "../../test_kernel_decl.hpp"

namespace directory_handle_create_close
{
  LLFIO_TEST_KERNEL_DECL LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle> test_kernel_directory_handle_absolute(LLFIO_V2_NAMESPACE::directory_handle::mode m, LLFIO_V2_NAMESPACE::directory_handle::creation c, LLFIO_V2_NAMESPACE::directory_handle::flag f,
                                                                                                                             LLFIO_V2_NAMESPACE::directory_handle::buffers_type *entries, LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle::enumerate_info> *info)
  {
    auto h = LLFIO_V2_NAMESPACE::directory_handle::directory({}, "testdir", m, c, LLFIO_V2_NAMESPACE::directory_handle::caching::all, f);
    if(h)
    {
      // git needs a file in a directory to create it, so any directory created needs an empty file called pin.txt
      (void) LLFIO_V2_NAMESPACE::file_handle::file(h.value(), "pin.txt", LLFIO_V2_NAMESPACE::file_handle::mode::write, LLFIO_V2_NAMESPACE::file_handle::creation::if_needed);
      *info = LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle::enumerate_info>(h.value().enumerate(std::move(*entries)));
      h.value().close().value();
    }
    return h;
  }
  LLFIO_TEST_KERNEL_DECL LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle> test_kernel_directory_handle_relative(LLFIO_V2_NAMESPACE::directory_handle::mode m, LLFIO_V2_NAMESPACE::directory_handle::creation c, LLFIO_V2_NAMESPACE::directory_handle::flag f,
                                                                                                                             LLFIO_V2_NAMESPACE::directory_handle::buffers_type *entries, LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle::enumerate_info> *info)
  {
    OUTCOME_TRY(b, LLFIO_V2_NAMESPACE::path_handle::path("."));
    auto h = LLFIO_V2_NAMESPACE::directory_handle::directory(b, "testdir", m, c, LLFIO_V2_NAMESPACE::directory_handle::caching::all, f);
    if(h)
    {
      // git needs a file in a directory to create it, so any directory created needs an empty file called pin.txt
      (void) LLFIO_V2_NAMESPACE::file_handle::file(h.value(), "pin.txt", LLFIO_V2_NAMESPACE::file_handle::mode::write, LLFIO_V2_NAMESPACE::file_handle::creation::if_needed);
      *info = LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::directory_handle::enumerate_info>(h.value().enumerate(std::move(*entries)));
      h.value().close().value();
    }
    b.close().value();
    return h;
  }
}  // namespace directory_handle_create_close
