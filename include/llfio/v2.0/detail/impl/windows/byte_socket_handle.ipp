/* A handle to a byte-orientated socket
(C) 2021-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Dec 2021


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

#include "byte_io_handle.hpp"

LLFIO_V2_NAMESPACE_BEGIN
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle> byte_socket_handle::byte_socket(const ip::address &addr, mode _mode, creation _creation,
                                                                                             caching _caching, flag flags) noexcept
  {
  }
  
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<std::pair<byte_socket_handle, byte_socket_handle>> byte_socket_handle::anonymous_socket(caching _caching,
                                                                                                                                 flag flags) noexcept
  {
  }


LLFIO_V2_NAMESPACE_END
