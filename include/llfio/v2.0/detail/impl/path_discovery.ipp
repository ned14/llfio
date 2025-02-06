/* Discovery of various useful filesystem paths
(C) 2017 - 2020 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#include "../../path_discovery.hpp"

#include "../../directory_handle.hpp"
#include "../../statfs.hpp"

#include <algorithm>
#include <mutex>
#include <new>
#include <regex>
#include <sstream>
#include <vector>

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace path_discovery
{
  namespace detail
  {
    struct _store
    {
      std::mutex lock;
      std::vector<discovered_path> all;
      span<discovered_path> verified;
      struct _discovered_path
      {
        filesystem::path path;
        size_t priority{0};
        std::string fstypename;
        bool is_networked{false};
        directory_handle h;  // not retained after verification
        explicit _discovered_path(filesystem::path _path)
            : path(std::move(_path))
        {
        }
      };
      std::vector<_discovered_path> _all;
      directory_handle storage_backed, memory_backed;
      bool storage_backed_is_networked{false}, memory_backed_is_networked{false};
    };
    _store &path_store()
    {
      static _store s;
      return s;
    }
  }  // namespace detail

  inline std::vector<std::pair<discovered_path::source_type, detail::_store::_discovered_path>> _all_temporary_directories(span<path_view> overrides,
                                                                                                                           span<path_view> fallbacks);

  span<discovered_path> all_temporary_directories(bool refresh, span<path_view> fallbacks, span<path_view> overrides) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(nullptr);
    auto &ps = detail::path_store();
    if(!refresh && !ps.all.empty())
    {
      return ps.all;
    }
    std::lock_guard<std::mutex> g(ps.lock);
    if(refresh)
    {
      ps.all.clear();
      ps._all.clear();
      ps.verified = {};
      ps.storage_backed = {};
      ps.memory_backed = {};
    }
    if(!ps.all.empty())
    {
      return ps.all;
    }
    LLFIO_EXCEPTION_TRY
    {
      std::vector<std::pair<discovered_path::source_type, detail::_store::_discovered_path>> raw = _all_temporary_directories(overrides, fallbacks);
      if(raw.empty())
      {
        LLFIO_LOG_FATAL(nullptr, "path_discovery::all_temporary_directories() sees no possible temporary directories, something has gone very wrong");
        abort();
      }
      for(size_t n = 0; n < raw.size(); n++)
      {
        raw[n].second.priority = n;
      }
      // Firstly sort by source and path so duplicates are side by side
      std::sort(raw.begin(), raw.end(),
                [](const auto &a, const auto &b) { return (a.first < b.first) || (a.first == b.first && a.second.path < b.second.path); });
      // Remove duplicates
      raw.erase(std::unique(raw.begin(), raw.end(), [](const auto &a, const auto &b) { return a.first == b.first && a.second.path == b.second.path; }),
                raw.end());
      // Put them back into the order in which they were returned
      std::sort(raw.begin(), raw.end(), [](const auto &a, const auto &b) { return a.second.priority < b.second.priority; });
      ps.all.reserve(raw.size());
      ps._all.reserve(raw.size());
      for(auto &i : raw)
      {
        ps._all.push_back(std::move(i.second));
        discovered_path dp;
        dp.path = ps._all.back().path;
        dp.source = i.first;
        ps.all.push_back(std::move(dp));
      }
    }
    LLFIO_EXCEPTION_CATCH({}, const std::exception &e)
    {
      std::string msg("path_discovery::all_temporary_directories() saw exception thrown: ");
      msg.append(e.what());
      LLFIO_LOG_WARN(nullptr, msg.c_str());
    }
    LLFIO_EXCEPTION_CATCH_ALL
    {
      LLFIO_LOG_WARN(nullptr, "path_discovery::all_temporary_directories() saw unknown exception throw");
    }
    return ps.all;
  }

  span<discovered_path> verified_temporary_directories(const char *_storage_backed_regex, const char *_memory_backed_regex) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(nullptr);
    auto &ps = detail::path_store();
    if(!ps.verified.empty())
    {
      return ps.verified;
    }
    (void) all_temporary_directories();
    std::lock_guard<std::mutex> g(ps.lock);
    if(!ps.verified.empty())
    {
      return ps.verified;
    }
    LLFIO_EXCEPTION_TRY
    {
      // Firstly go try to open and stat all items
      for(size_t n = 0; n < ps.all.size(); n++)
      {
        {
#if LLFIO_LOGGING_LEVEL
          log_level_guard logg(log_level::fatal);  // suppress log printing of failure
          (void) logg;
#endif
          auto _h = directory_handle::directory({}, ps.all[n].path);
          if(!_h)
          {
            // Error during opening
#if 0
            fprintf(stderr, "path_discovery::verified_temporary_directories() failed to open %s due to %s\n", ps.all[n].path.path().c_str(),
                    _h.error().message().c_str());
            path_view::zero_terminated_rendered_path<> zpath(ps.all[n].path);
            fprintf(stderr, "path_view::c_str says buffer = %p (%s) length = %u\n", zpath.c_str(), zpath.c_str(), (unsigned) zpath.size());
            visit(ps.all[n].path, [](auto _sv) {
              char buffer[1024];
              memcpy(buffer, (const char *) _sv.data(), _sv.size());
              buffer[_sv.size()] = 0;
              fprintf(stderr, "path_view has pointer %p content %s length = %u\n", _sv.data(), buffer, (unsigned) _sv.size());
            });
#endif
            continue;
          }
          ps._all[n].h = std::move(_h).value();
        }
        // Try to create a small file in that directory
        auto _fh =
        file_handle::uniquely_named_file(ps._all[n].h, file_handle::mode::write, file_handle::caching::temporary, file_handle::flag::unlink_on_first_close);
        if(!_fh)
        {
#if LLFIO_LOGGING_LEVEL >= 3
          std::string msg("path_discovery::verified_temporary_directories() failed to create a file in ");
          msg.append(ps._all[n].path.string());
          msg.append(" due to ");
          msg.append(_fh.error().message().c_str());
          LLFIO_LOG_WARN(nullptr, msg.c_str());
#endif
          ps._all[n].h = {};
          continue;
        }
        ps.all[n].stat = stat_t(nullptr);
        auto r = ps.all[n].stat->fill(ps._all[n].h);
        if(!r)
        {
          LLFIO_LOG_WARN(nullptr, "path_discovery::verified_temporary_directories() failed to stat an open handle to a temp directory");
          ps.all[n].stat = {};
          ps._all[n].h = {};
          continue;
        }
        statfs_t statfs;
        auto statfsres = statfs.fill(_fh.value(), statfs_t::want::fstypename);
        if(statfsres)
        {
          ps._all[n].fstypename = std::move(statfs.f_fstypename);
          ps._all[n].is_networked = statfs.f_flags.networked;
        }
        else
        {
#if LLFIO_LOGGING_LEVEL >= 3
          std::string msg("path_discovery::verified_temporary_directories() failed to statfs the temp directory ");
          msg.append(ps._all[n].path.string());
          msg.append(" due to ");
          msg.append(statfsres.error().message().c_str());
          LLFIO_LOG_WARN(nullptr, msg.c_str());
#endif
          ps.all[n].stat = {};
          ps._all[n].h = {};
          continue;
        }
      }
      // Now partition into those with valid stat directories and those without
      std::stable_partition(ps._all.begin(), ps._all.end(), [](const detail::_store::_discovered_path &a) { return a.h.is_valid(); });
      auto it = std::stable_partition(ps.all.begin(), ps.all.end(), [](const discovered_path &a) { return a.stat; });
      ps.verified = span<discovered_path>(ps.all.data(), it - ps.all.begin());
      if(ps.verified.empty())
      {
        std::stringstream msg;
        msg << "path_discovery::verified_temporary_directories() could not find at least one writable temporary directory. Directories probed were:\n";
        for(size_t n = 0; n < ps.all.size(); n++)
        {
          msg << "\n   " << ps.all[n].path << " (" << ps.all[n].source << ") fs type = " << ps._all[n].fstypename << " valid = " << (bool) ps.all[n].stat;
        }
        msg << "\n";
        LLFIO_LOG_FATAL(nullptr, msg.str().c_str());
        abort();
      }

      // Finally, need to choose storage and memory backed directories
      std::regex storage_backed_regex_(_storage_backed_regex, std::regex::icase);
      std::regex memory_backed_regex_(_memory_backed_regex, std::regex::icase);
      for(size_t n = 0; n < ps.verified.size(); n++)
      {
        if(!ps.storage_backed.is_valid() && std::regex_match(ps._all[n].fstypename, storage_backed_regex_))
        {
          ps.storage_backed = std::move(ps._all[n].h);
          ps.storage_backed_is_networked = ps._all[n].is_networked;
        }
        if(!ps.memory_backed.is_valid() && std::regex_match(ps._all[n].fstypename, memory_backed_regex_))
        {
          ps.memory_backed = std::move(ps._all[n].h);
          ps.memory_backed_is_networked = ps._all[n].is_networked;
        }
        ps.all[n].path = ps._all[n].path;
        (void) ps._all[n].h.close();
      }
    }
    LLFIO_EXCEPTION_CATCH({}, const std::exception &e)
    {
      std::string msg("path_discovery::verified_temporary_directories() saw exception thrown: ");
      msg.append(e.what());
      LLFIO_LOG_FATAL(nullptr, msg.c_str());
      abort();
    }
    LLFIO_EXCEPTION_CATCH_ALL
    {
      LLFIO_LOG_FATAL(nullptr, "path_discovery::verified_temporary_directories() saw unknown exception throw");
      abort();
    }
#ifndef LLFIO_PATH_DISCOVERY_DISABLE_STORAGE_BACKED_TEMPORARY_FILES_DIRECTORY_WARNING
    if(!ps.storage_backed.is_valid())
    {
      LLFIO_LOG_WARN(nullptr, "path_discovery:verified_temporary_directories() found no suitable path for storage_backed_temporary_files_directory(), creation "
                              "of storage backed temporary files will fail with bad_file_descriptor!");
    }
#endif
#ifndef LLFIO_PATH_DISCOVERY_DISABLE_MEMORY_BACKED_TEMPORARY_FILES_DIRECTORY_WARNING
    if(!ps.memory_backed.is_valid())
    {
      LLFIO_LOG_WARN(nullptr, "path_discovery:verified_temporary_directories() found no suitable path for memory_backed_temporary_files_directory(), creation "
                              "of memory backed temporary files will fail with bad_file_descriptor!");
    }
#endif
    return ps.verified;
  }

  const path_handle &storage_backed_temporary_files_directory() noexcept
  {
    (void) verified_temporary_directories();
    auto &ps = detail::path_store();
    return ps.storage_backed;
  }
  bool storage_backed_temporary_files_directory_is_networked() noexcept
  {
    (void) verified_temporary_directories();
    auto &ps = detail::path_store();
    return ps.storage_backed_is_networked;
  }
  const path_handle &memory_backed_temporary_files_directory() noexcept
  {
    (void) verified_temporary_directories();
    auto &ps = detail::path_store();
    return ps.memory_backed;
  }
  bool memory_backed_temporary_files_directory_is_networked() noexcept
  {
    (void) verified_temporary_directories();
    auto &ps = detail::path_store();
    return ps.memory_backed_is_networked;
  }

  namespace detail
  {
    struct _capture_starting_working_directory_t
    {
      path_handle dirh;
      _capture_starting_working_directory_t()
      {
        auto r = current_working_directory();
        if(r)
        {
          dirh = std::move(r).assume_value();
        }
      }
    };
    _capture_starting_working_directory_t &capture_starting_working_directory()
    {
      static _capture_starting_working_directory_t v;
      return v;
    }
  }  // namespace detail
  const path_handle &starting_working_directory() noexcept
  {
    return detail::capture_starting_working_directory().dirh;
  }
}  // namespace path_discovery

LLFIO_V2_NAMESPACE_END

#define LLFIO_PATH_DISCOVERY_INCLUDING
#ifdef _WIN32
#include "windows/path_discovery.ipp"
#else
#include "posix/path_discovery.ipp"
#endif
#undef LLFIO_PATH_DISCOVERY_INCLUDING
