/* A handle to a filesystem location
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Jul 2017


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

#include "../../../path_handle.hpp"

#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

result<path_handle> path_handle::path(const path_handle &base, path_handle::path_view_type path) noexcept
{
  result<path_handle> ret{path_handle(native_handle_type())};
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::directory;
  int attribs = O_CLOEXEC | O_RDONLY;
#ifdef O_DIRECTORY
  attribs |= O_DIRECTORY;
#endif
#ifdef O_PATH
  // Linux provides this extension opening a super light weight fd to just an anchor on the filing system
  attribs |= O_PATH;
#endif
  path_view::c_str<> zpath(path);
  if(base.is_valid())
  {
    nativeh.fd = ::openat(base.native_handle().fd, zpath.buffer, attribs);
  }
  else
  {
    nativeh.fd = ::open(zpath.buffer, attribs);
  }
  if(-1 == nativeh.fd)
  {
    return posix_error();
  }
  return ret;
}

LLFIO_V2_NAMESPACE_END
