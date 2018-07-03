/* A view of a path to something
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

#include "../../../path_view.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC void path_view::c_str::_from_utf8(const path_view &view) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  ULONG written = 0;
  NTSTATUS ntstat = RtlUTF8ToUnicodeN(_buffer, static_cast<ULONG>(sizeof(_buffer) - sizeof(wchar_t)), &written, view._state._utf8.data(), static_cast<ULONG>(view._state._utf8.size()));
  if(ntstat < 0)
  {
    LLFIO_LOG_FATAL(ntstat, ntkernel_error(ntstat).message().c_str());
    abort();
  }
  length = static_cast<uint16_t>(written / sizeof(wchar_t));
  _buffer[length] = 0;
  wchar_t *p = _buffer;
  do
  {
    p = wcschr(p, '/');
    if(p != nullptr)
    {
      *p = '\\';
    }
  } while(p != nullptr);
  buffer = _buffer;
}

LLFIO_V2_NAMESPACE_END
