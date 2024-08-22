/* Examples of LLFIO use
(C) 2024 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2024


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

#include "../include/llfio.hpp"

#if defined(_WIN32) && _HAS_CXX20
#include "../include/llfio/v2.0/detail/impl/windows/import.hpp"

namespace path_view_openat_example
{
  using namespace LLFIO_V2_NAMESPACE;
  using namespace LLFIO_V2_NAMESPACE::windows_nt_kernel;
  using namespace std;

  using LLFIO_V2_NAMESPACE::windows_nt_kernel::IO_STATUS_BLOCK;
  using LLFIO_V2_NAMESPACE::windows_nt_kernel::NtCreateFile;
  using LLFIO_V2_NAMESPACE::windows_nt_kernel::OBJECT_ATTRIBUTES;
  using LLFIO_V2_NAMESPACE::windows_nt_kernel::UNICODE_STRING;

  HANDLE openat(HANDLE base, path_view path)
  {
    static constexpr DWORD access = GENERIC_READ;
    static constexpr DWORD share =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    static constexpr DWORD creation = OPEN_EXISTING;
    static constexpr DWORD flags = FILE_ATTRIBUTE_NORMAL;
    return visit(
    [&](auto sv) -> HANDLE
    {
      using type = typename decltype(sv)::value_type;
      if constexpr(is_same_v<type, byte>)
      {
        if(sv.size() == 16)
        {
          FILE_ID_DESCRIPTOR fid{};
          fid.dwSize = sizeof(fid);
          fid.Type = 1;
          memcpy(&fid.ObjectId, sv.data(), 16);
          return OpenFileById(base, &fid, access, share, nullptr, flags);
        }
        throw std::runtime_error("binary key must be exactly 16 bytes long");
      }
      // A "\!!\" or "\??\" prefix enables direct use of the NT kernel API
      // or setting base, as the Win32 API doesn't support paths relative to a
      // base
      const bool is_ntpath =
      sv.size() >= 4 && sv[0] == '\\' && sv[3] == '\\' &&
      ((sv[1] == '!' && sv[2] == '!') || (sv[1] == '?' && sv[2] == '?'));
      if(base != nullptr || is_ntpath)
      {
        // The NT kernel always takes the system wide encoding
        auto zpath = path.render_unterminated<wchar_t>(path);
        UNICODE_STRING _path{};
        _path.Buffer = const_cast<wchar_t *>(zpath.data());
        _path.MaximumLength =
        (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) +
        sizeof(wchar_t);
        // The "\!!\" prefix is library local and the NT kernel doesn't
        // understand it unlike "\??\", so strip it off
        if(zpath.size() >= 4 && _path.Buffer[0] == '\\' &&
           _path.Buffer[1] == '!' && _path.Buffer[2] == '!' &&
           _path.Buffer[3] == '\\')
        {
          _path.Buffer += 3;
          _path.Length -= 3 * sizeof(wchar_t);
          _path.MaximumLength -= 3 * sizeof(wchar_t);
        }

        OBJECT_ATTRIBUTES oa{};
        oa.Length = sizeof(OBJECT_ATTRIBUTES);
        oa.ObjectName = &_path;
        oa.RootDirectory = base;
        oa.Attributes = 0;

        IO_STATUS_BLOCK isb{};
        LARGE_INTEGER AllocationSize{};
        HANDLE ret = INVALID_HANDLE_VALUE;
        NTSTATUS ntstat =
        NtCreateFile(&ret, access, &oa, &isb, &AllocationSize, 0, share,
                     FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0);
        if(ntstat < 0)
        {
          // Might want to call RtlNtStatusToDosError(ntstat)?
        }
        return ret;
      }
      if constexpr(is_same_v<type, char>)
      {
        // Render to the system narrow encoding null terminated
        auto zpath = path.render_null_terminated<char>(path);
        return CreateFileA(zpath.c_str(), access, share, nullptr, creation,
                           flags, nullptr);
      }
      else // char8_t, char16_t, wchar_t
      {
        // Render to the system wide encoding null terminated
        auto zpath = path.render_null_terminated<wchar_t>(path);
        return CreateFileW(zpath.c_str(), access, share, nullptr, creation,
                           flags, nullptr);
      }
    },
    path);
  }
}  // namespace path_view_openat_example
#endif

// clang-format off

int main()
{
  return 0;
}
