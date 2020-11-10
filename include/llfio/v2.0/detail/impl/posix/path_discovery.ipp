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

#ifndef LLFIO_PATH_DISCOVERY_INCLUDING
#error Must be included by ../path_discovery.ipp only
#endif

#include "../../../mapped_file_handle.hpp"

#include <pwd.h>

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace path_discovery
{
  std::vector<std::pair<discovered_path::source_type, _store::_discovered_path>> _all_temporary_directories()
  {
    std::vector<std::pair<discovered_path::source_type, _store::_discovered_path>> ret;
    filesystem::path::string_type buffer;
    buffer.resize(PATH_MAX);
    // Only observe environment variables if not a SUID or SGID situation
    // FIXME? Is this actually enough? What about the non-standard saved uid/gid?
    // Should I be checking if my executable is SUGID and its owning user is not mine?
    if(getuid() == geteuid() && getgid() == getegid())
    {
      // Note that XDG_RUNTIME_DIR is the systemd runtime directory for the current user, usually mounted with tmpfs
      // XDG_CACHE_HOME  is the systemd cache directory for the current user, usually at $HOME/.cache
      static const char *variables[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR", "XDG_RUNTIME_DIR", "XDG_CACHE_HOME"};
      for(auto &variable : variables)
      {
        const char *env = getenv(variable);
        if(env != nullptr)
        {
          ret.emplace_back(discovered_path::source_type::environment, env);
        }
      }
      // Also try $HOME/.cache
      const char *env = getenv("HOME");
      if(env != nullptr)
      {
        buffer = env;
        buffer.append("/.cache");
        ret.emplace_back(discovered_path::source_type::environment, buffer);
      }
    }

    // Parse /etc/passwd for the effective user's home directory
    // We do it by hand seeing as, amazingly, getpwent_r() isn't actually threadsafe :(
    {
      auto _passwdh = mapped_file_handle::mapped_file({}, "/etc/passwd");
      if(!_passwdh)
      {
        std::string msg("path_discovery::all_temporary_directories() failed to open /etc/passwd due to ");
        msg.append(_passwdh.error().message().c_str());
        LLFIO_LOG_WARN(nullptr, msg.c_str());
      }
      else
      {
        attached<const char> passwd(_passwdh.value());
        /* This will consist of lines of the form:

        jsmith:x:1001:1000:Joe Smith,Room 1007,(234)555-8910,(234)555-0044,email:/home/jsmith:/bin/sh
        ned:x:1000:1000:"",,,:/home/ned:/bin/bash
        # Comments and blank lines also possible

        */
        string_view passwdfile(passwd.data(), passwd.size());
        size_t linestart = 0;
        do
        {
          string_view line(passwdfile.substr(linestart));
          if(auto lineend = line.find(10))
          {
            line = line.substr(0, lineend);
          }
          linestart += line.size() + 1;
          // uid is two colons in
          size_t colon = line.find(':');
          if(colon != string_view::npos)
          {
            colon = line.find(':', colon + 1);
            if(colon != string_view::npos)
            {
              long uid = strtol(line.data() + 1, nullptr, 10);
              if(uid == (long) geteuid())
              {
                // home directory is two colons from end
                size_t homeend = line.rfind(':');
                if(homeend != string_view::npos)
                {
                  colon = line.rfind(':', homeend - 1);
                  if(colon != string_view::npos)
                  {
                    auto homedir = line.substr(colon + 1, homeend - colon - 1);
                    buffer.assign(homedir.data(), homedir.size());
                    buffer.append("/.cache");
                    ret.emplace_back(discovered_path::source_type::system, buffer);
                    if(-1 == ::access(buffer.c_str(), F_OK))
                    {
                      ::mkdir(buffer.c_str(), 0700);  // only user has access
                    }
                    break;
                  }
                }
              }
            }
          }
        } while(linestart < passwd.size());
      }
    }

    // If everything earlier failed e.g. if our environment block is zeroed,
    // fall back to /tmp and then /var/tmp, the last of which should succeed even if tmpfs is not mounted
    ret.emplace_back(discovered_path::source_type::hardcoded, "/tmp");
    ret.emplace_back(discovered_path::source_type::hardcoded, "/var/tmp");
    // Effective user, not real user, to not create files owned by a different user
    buffer = "/run/user/" + std::to_string(geteuid());
    ret.emplace_back(discovered_path::source_type::hardcoded, buffer);
    ret.emplace_back(discovered_path::source_type::hardcoded, "/run/shm");
    // Finally, on some Docker images there is no /tmp, /var/tmp, /run nor anywhere sane
    ret.emplace_back(discovered_path::source_type::hardcoded, "/");
    return ret;
  }

  const path_handle &temporary_named_pipes_directory() noexcept { return storage_backed_temporary_files_directory(); }
}  // namespace path_discovery

LLFIO_V2_NAMESPACE_END
