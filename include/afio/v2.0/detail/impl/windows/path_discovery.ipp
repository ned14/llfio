/* Discovery of various useful filesystem paths
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Sept 2017


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

#ifndef AFIO_PATH_DISCOVERY_INCLUDING
#error Must be included by ../path_discovery.ipp only
#endif

#include "import.hpp"

#include <ShlObj.h>

AFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace path_discovery
{
  std::vector<std::pair<discovered_path::source_type, _store::_discovered_path>> _all_temporary_directories()
  {
    std::vector<std::pair<discovered_path::source_type, _store::_discovered_path>> ret;
    filesystem::path::string_type buffer;
    buffer.resize(32768);
    // Only observe environment variables if not a SUID or SGID situation
    if(!running_under_suid_gid())
    {
      // We don't use it as it's nearly useless, but GetTempPath() returns one of (in order):
      // %TMP%, %TEMP%, %USERPROFILE%, GetWindowsDirectory()\Temp
      static const wchar_t *variables[] = {L"TMP", L"TEMP", L"LOCALAPPDATA", L"USERPROFILE"};
      for(size_t n = 0; n < sizeof(variables) / sizeof(variables[0]); n++)
      {
        buffer.resize(32768);
        DWORD len = GetEnvironmentVariable(variables[n], (LPWSTR) buffer.data(), (DWORD) buffer.size());
        if(len && len < buffer.size())
        {
          buffer.resize(len);
          if(variables[n][0] == 'L')
          {
            buffer.append(L"\\Temp");
            ret.emplace_back(discovered_path::source_type::environment, buffer);
            buffer.resize(len);
          }
          else if(variables[n][0] == 'U')
          {
            buffer.append(L"\\AppData\\Local\\Temp");
            ret.emplace_back(discovered_path::source_type::environment, buffer);
            buffer.resize(len);
            buffer.append(L"\\Local Settings\\Temp");
            ret.emplace_back(discovered_path::source_type::environment, buffer);
            buffer.resize(len);
          }
          else
          {
            ret.emplace_back(discovered_path::source_type::environment, buffer);
          }
        }
      }
    }

    // Ask the shell
    {
      PWSTR out;
      if(S_OK == SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, NULL, &out))
      {
        buffer = out;
        CoTaskMemFree(out);
        buffer.append(L"\\Temp");
        ret.emplace_back(discovered_path::source_type::system, buffer);
      }
      if(S_OK == SHGetKnownFolderPath(FOLDERID_Profile, 0, NULL, &out))
      {
        buffer = out;
        CoTaskMemFree(out);
        auto len = buffer.size();
        buffer.append(L"\\AppData\\Local\\Temp");
        ret.emplace_back(discovered_path::source_type::system, buffer);
        buffer.resize(len);
        buffer.append(L"\\Local Settings\\Temp");
        ret.emplace_back(discovered_path::source_type::system, buffer);
      }
    }

    // Finally if everything earlier failed e.g. if our environment block is zeroed,
    // fall back to Win3.1 era "the Windows directory" which definitely won't be
    // C:\Windows nowadays
    buffer.resize(32768);
    DWORD len = GetWindowsDirectory((LPWSTR) buffer.data(), (UINT) buffer.size());
    if(len && len < buffer.size())
    {
      buffer.resize(len);
      buffer.append(L"\\Temp");
      ret.emplace_back(discovered_path::source_type::system, buffer);
    }
    // And if even that fails, try good old %SYSTEMDRIVE%\Temp
    buffer.resize(32768);
    len = GetSystemWindowsDirectory((LPWSTR) buffer.data(), (UINT) buffer.size());
    if(len && len < buffer.size())
    {
      buffer.resize(len);
      buffer.resize(buffer.find_last_of(L"\\"));
      buffer.append(L"\\Temp");
      ret.emplace_back(discovered_path::source_type::hardcoded, buffer);
    }
    return ret;
  }
}

AFIO_V2_NAMESPACE_END
