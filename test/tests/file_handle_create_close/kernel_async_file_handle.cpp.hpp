/* Test kernel for file_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (4 commits)
File Created: May 2016


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

namespace file_handle_create_close
{
  LLFIO_TEST_KERNEL_DECL LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::async_file_handle> test_kernel_async_file_handle_absolute(LLFIO_V2_NAMESPACE::async_file_handle::mode m, LLFIO_V2_NAMESPACE::async_file_handle::creation c, LLFIO_V2_NAMESPACE::async_file_handle::flag f)
  {
    LLFIO_V2_NAMESPACE::io_service service;
    auto h = LLFIO_V2_NAMESPACE::async_file_handle::async_file(service, {}, "testfile.txt", m, c, LLFIO_V2_NAMESPACE::async_file_handle::caching::all, f);
    if(h) {
      h.value().close().value(); }
    return h;
  }
  LLFIO_TEST_KERNEL_DECL LLFIO_V2_NAMESPACE::result<LLFIO_V2_NAMESPACE::async_file_handle> test_kernel_async_file_handle_relative(LLFIO_V2_NAMESPACE::async_file_handle::mode m, LLFIO_V2_NAMESPACE::async_file_handle::creation c, LLFIO_V2_NAMESPACE::async_file_handle::flag f)
  {
    LLFIO_V2_NAMESPACE::io_service service;
    OUTCOME_TRY(b, LLFIO_V2_NAMESPACE::path_handle::path("."));
    auto h = LLFIO_V2_NAMESPACE::async_file_handle::async_file(service, b, "testfile.txt", m, c, LLFIO_V2_NAMESPACE::async_file_handle::caching::all, f);
    if(h) {
      h.value().close().value(); }
    b.close().value();
    return h;
  }
}  // namespace file_handle_create_close
