/* Configures LLFIO
(C) 2015-2020 Niall Douglas <http://www.nedproductions.biz/> (24 commits)
File Created: Dec 2015


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

#include "../../config.hpp"

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace detail
{
#if defined(LLFIO_SOURCE) && !defined(LLFIO_DISABLE_SIZEOF_FILESYSTEM_PATH_CHECK)
  LLFIO_HEADERS_ONLY_FUNC_SPEC size_t sizeof_filesystem_path() noexcept { return sizeof(filesystem::path); }
#endif

  LLFIO_HEADERS_ONLY_FUNC_SPEC char &thread_local_log_level()
  {
    static LLFIO_THREAD_LOCAL char v;
    return v;
  }
}  // namespace detail

LLFIO_V2_NAMESPACE_END
