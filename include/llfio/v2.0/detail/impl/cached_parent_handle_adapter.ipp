/* A parent handle caching adapter
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
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

#include "../../algorithm/handle_adapter/cached_parent.hpp"

#include <mutex>
#include <unordered_map>

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace detail
  {
    struct cached_path_handle_map_
    {
      std::mutex lock;
      size_t gc_count{0};
      std::unordered_map<filesystem::path, std::weak_ptr<cached_path_handle>, path_hasher> by_path;
    };
    inline cached_path_handle_map_ &cached_path_handle_map()
    {
      static cached_path_handle_map_ map;
      return map;
    }
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<filesystem::path> cached_path_handle::current_path(const filesystem::path &append) noexcept
    {
      try
      {
        auto ret = h.current_path();
        auto &map = cached_path_handle_map();
        std::lock_guard<std::mutex> g(map.lock);
        if(!ret)
        {
          std::string msg("cached_path_handle::current_path() failed to retrieve current path of cached handle due to ");
          msg.append(ret.error().message().c_str());
          LLFIO_LOG_WARN(nullptr, msg.c_str());
        }
        else if(!ret.value().empty() && ret.value() != _lastpath)
        {
          map.by_path.erase(_lastpath);
          _lastpath = std::move(ret).value();
          map.by_path.emplace(_lastpath, shared_from_this());
        }
        return _lastpath / append;
      }
      catch(...)
      {
        return error_from_exception();
      }
    }
    LLFIO_HEADERS_ONLY_FUNC_SPEC std::pair<cached_path_handle_ptr, filesystem::path> get_cached_path_handle(const path_handle &base, path_view path)
    {
      path_view leaf(path.filename());
      path = path.remove_filename();
      filesystem::path dirpath;
      if(base.is_valid())
      {
        dirpath = base.current_path().value() / path;
      }
      else
      {
        dirpath = path.path();
        if(path.empty())
        {
          dirpath = filesystem::current_path();
        }
#ifdef _WIN32
        // On Windows, only use the kernel path form
        dirpath = path_handle::path(dirpath).value().current_path().value();
#endif
        if(path.empty())
        {
          path = dirpath;
        }
      }
      auto &map = cached_path_handle_map();
      std::lock_guard<std::mutex> g(map.lock);
      auto rungc = make_scope_exit([&map]() noexcept {
        if(map.gc_count++ >= 1024)
        {
          for(auto it = map.by_path.begin(); it != map.by_path.end();)
          {
            if(it->second.expired())
            {
              it = map.by_path.erase(it);
            }
            else
            {
              ++it;
            }
          }
          map.gc_count = 0;
        }
      });
      (void) rungc;
      auto it = map.by_path.find(dirpath);
      if(it != map.by_path.end())
      {
        cached_path_handle_ptr ret = it->second.lock();
        if(ret)
        {
          return {ret, leaf.path()};
        }
      }
      cached_path_handle_ptr ret = std::make_shared<cached_path_handle>(directory_handle::directory(base, path).value());
      auto _currentpath = ret->h.current_path();
      if(_currentpath)
      {
        ret->_lastpath = std::move(_currentpath).value();
      }
      else
      {
        ret->_lastpath = std::move(dirpath);
      }
      it = map.by_path.find(ret->_lastpath);
      if(it != map.by_path.end())
      {
        it->second = ret;
      }
      else
      {
        map.by_path.emplace(ret->_lastpath, ret);
      }
      return {ret, leaf.path()};
    }
  }  // namespace detail
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
