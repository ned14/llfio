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

result<bool> path_handle::exists(path_view_type path) const noexcept
{
  try
  {
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    LLFIO_LOG_FUNCTION_CALL(this);
    path_view::not_zero_terminated_rendered_path<> zpath(path);
    UNICODE_STRING _path{};
    _path.Buffer = const_cast<wchar_t *>(zpath.data());
    _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);
    if(zpath.size() >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '!' && _path.Buffer[2] == '!' && _path.Buffer[3] == '\\')
    {
      _path.Buffer += 3;
      _path.Length -= 3 * sizeof(wchar_t);
      _path.MaximumLength -= 3 * sizeof(wchar_t);
    }

    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.RootDirectory = native_handle().h;
    oa.Attributes = 0;  // 0x40 /*OBJ_CASE_INSENSITIVE*/;
    // if(!!(flags & file_flags::int_opening_link))
    //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;

    FILE_BASIC_INFORMATION fbi;
    memset(&fbi, 0, sizeof(fbi));
    NTSTATUS ntstat = NtQueryAttributesFile(&oa, &fbi);
    if(ntstat < 0)
    {
      auto ret = ntkernel_error(ntstat);
      if(ret == errc::no_such_file_or_directory)
      {
        return false;
      }
      return failure(std::move(ret));
    }
    return true;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<path_handle> path_handle::path(const path_handle &base, path_handle::path_view_type path) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<path_handle> ret{path_handle(native_handle_type(), caching::none, flag::none)};
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::path;
  DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  // Open directory with no access requested, this is much faster than asking for access
  OUTCOME_TRY(auto &&access, access_mask_from_handle_mode(nativeh, mode::none, flag::none));
  OUTCOME_TRY(auto &&attribs, attributes_from_handle_caching_and_flags(nativeh, caching::all, flag::none));
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  /* It is super important that we remove the DELETE permission for directories as otherwise relative renames
  will always fail due to an unfortunate design choice by Microsoft.
  */
  access &= ~DELETE;
  if(base.is_valid() || path.is_ntpath())
  {
    DWORD creatdisp = 0x00000001 /*FILE_OPEN*/;
    attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
    OUTCOME_TRY(auto &&ntflags, ntflags_from_handle_caching_and_flags(nativeh, caching::all, flag::none));
    ntflags |= 0x01 /*FILE_DIRECTORY_FILE*/;  // required to open a directory
    IO_STATUS_BLOCK isb = make_iostatus();

    path_view::not_zero_terminated_rendered_path<> zpath(path);
    UNICODE_STRING _path{};
    _path.Buffer = const_cast<wchar_t *>(zpath.data());
    _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);
    if(zpath.size() >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '!' && _path.Buffer[2] == '!' && _path.Buffer[3] == '\\')
    {
      _path.Buffer += 3;
      _path.Length -= 3 * sizeof(wchar_t);
      _path.MaximumLength -= 3 * sizeof(wchar_t);
    }

    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
    oa.Attributes = 0;  // 0x40 /*OBJ_CASE_INSENSITIVE*/;
    // if(!!(flags & file_flags::int_opening_link))
    //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;

    LARGE_INTEGER AllocationSize{};
    memset(&AllocationSize, 0, sizeof(AllocationSize));
    NTSTATUS ntstat = NtCreateFile(&nativeh.h, access, &oa, &isb, &AllocationSize, attribs, fileshare, creatdisp, ntflags, nullptr, 0);
    if(STATUS_PENDING == ntstat)
    {
      ntstat = ntwait(nativeh.h, isb, deadline());
    }
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
  }
  else
  {
    DWORD creation = OPEN_EXISTING;
    attribs |= FILE_FLAG_BACKUP_SEMANTICS;  // required to open a directory
    path_view::zero_terminated_rendered_path<> zpath(path);
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateFileW_(zpath.data(), access, fileshare, nullptr, creation, attribs, nullptr)))  // NOLINT
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return win32_error(errcode);
    }
  }
  return ret;
}

LLFIO_V2_NAMESPACE_END
