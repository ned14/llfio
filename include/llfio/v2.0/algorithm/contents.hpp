/* A filesystem algorithm which returns the contents of a directory tree
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: July 2020


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

#ifndef LLFIO_ALGORITHM_CONTENTS_HPP
#define LLFIO_ALGORITHM_CONTENTS_HPP

#include "traverse.hpp"

#include <memory>
#include <mutex>

//! \file contents.hpp Provides a directory tree contents algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  /*! \brief A visitor for the filesystem contents algorithm.
   */
  struct contents_visitor : traverse_visitor
  {
    //! Whether to include files in the contents.
    bool contents_include_files{true};
    //! Whether to include directories in the contents.
    bool contents_include_directories{true};
    //! Whether to include symlinks in the contents.
    bool contents_include_symlinks{true};
    //! What `stat_t::want` to include, if enumeration doesn't provide these, they will be additionally fetched.
    stat_t::want contents_include_metadata{stat_t::want::none};

    /*! \brief Enumerated contents, and what parts of their `stat_t` is valid.
     */
    struct contents_type : public std::vector<std::pair<filesystem::path, stat_t>>
    {
      //! The metadata valid within all the `stat_t` in the contents traversed.
      stat_t::want metadata{stat_t::want::none};
    };

    //! Default construtor
    contents_visitor() = default;
    //! Construct an instance
    explicit contents_visitor(stat_t::want _metadata, bool _include_files = true, bool _include_directories = true, bool _include_symlinks = true)
        : contents_include_files(_include_files)
        , contents_include_directories(_include_directories)
        , contents_include_symlinks(_include_symlinks)
        , contents_include_metadata(_metadata)
    {
    }

    friend inline result<contents_type> contents(const path_handle &dirh, contents_visitor *visitor, size_t threads, bool force_slow_path) noexcept;

  protected:
    struct _state_type
    {
      const path_handle &rootdirh;
      std::atomic<size_t> rootdirpathlen{0};
      std::atomic<stat_t::want> metadata{stat_t::want::all};
      contents_type contents;

      std::mutex lock;
      std::vector<std::shared_ptr<contents_type>> all_thread_contents;

      explicit _state_type(const path_handle &_rootdirh)
          : rootdirh(_rootdirh)
      {
      }
    };

    static std::shared_ptr<contents_type> _thread_contents(_state_type *state) noexcept
    {
      try
      {
        static thread_local std::weak_ptr<contents_type> mycontents;
        auto ret = mycontents.lock();
        if(ret)
        {
          return ret;
        }
        ret = std::make_unique<contents_type>();
        mycontents = ret;
        std::lock_guard<std::mutex> g(state->lock);
        state->all_thread_contents.push_back(ret);
        return ret;
      }
      catch(...)
      {
        return {};
      }
    }

  public:
    /*! \brief The default implementation accumulates the contents into thread
    local storage. At traverse end, all the thread local storages are coalesced
    into a single result, the member variable `contents`.
    */
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept
    {
      try
      {
        auto *state = (_state_type *) data;
        (void) depth;
        if(!contents.empty())
        {
          contents_type toadd;
          toadd.reserve(contents.size());
          filesystem::path dirhpath;
          for(;;)
          {
            OUTCOME_TRY(dirhpath, dirh.current_path());
            auto rootdirpathlen = state->rootdirpathlen.load(std::memory_order_relaxed);
            if(dirhpath.native().size() <= rootdirpathlen)
            {
              break;
            }
            dirhpath = dirhpath.native().substr(rootdirpathlen);
            auto r = directory_handle::directory(state->rootdirh, dirhpath);
            if(r && r.value().unique_id() == dirh.unique_id())
            {
              break;
            }
            OUTCOME_TRY(dirhpath, state->rootdirh.current_path());
            state->rootdirpathlen.store(dirhpath.native().size() + 1, std::memory_order_relaxed);
          }
          auto _metadata_ = state->metadata.load(std::memory_order_relaxed);
          if((_metadata_ & (contents_include_metadata | contents.metadata())) != _metadata_)
          {
            state->metadata.store(_metadata_ & (contents_include_metadata | contents.metadata()), std::memory_order_relaxed);
          }
          auto into = _thread_contents(state);
          for(auto &entry : contents)
          {
            auto need_stat = contents_include_metadata & ~contents.metadata();
            if((contents_include_files && entry.stat.st_type == filesystem::file_type::regular) ||
               (contents_include_directories && entry.stat.st_type == filesystem::file_type::directory) ||
               (contents_include_symlinks && entry.stat.st_type == filesystem::file_type::symlink))
            {
              if(!need_stat)
              {
                into->emplace_back(dirhpath / entry.leafname, entry.stat);
                continue;
              }
              auto r = file_handle::file(dirh, entry.leafname, file_handle::mode::attr_read);
              if(r)
              {
                OUTCOME_TRY(entry.stat.fill(r.value(), need_stat));
                into->emplace_back(dirhpath / entry.leafname, entry.stat);
              }
            }
          }
        }
        return success();
      }
      catch(...)
      {
        return error_from_exception();
      }
    }

    /*! \brief Called when a traversal finishes, this default implementation merges
    all the thread local results into `contents`, and deallocates the thread local
    results.
    */
    virtual result<size_t> finished(void *data, result<size_t> result) noexcept
    {
      try
      {
        auto *state = (_state_type *) data;
        state->contents.clear();
        state->contents.metadata = state->metadata.load(std::memory_order_relaxed);
        size_t count = 0;
        for(auto &i : state->all_thread_contents)
        {
          count += i->size();
        }
        state->contents.reserve(count);
        for(auto &i : state->all_thread_contents)
        {
          state->contents.insert(state->contents.end(), std::make_move_iterator(i->begin()), std::make_move_iterator(i->end()));
        }
        state->all_thread_contents.clear();
        return result;
      }
      catch(...)
      {
        return error_from_exception();
      }
    }
  };


  /*! \brief Calculate the contents of everything within and under `dirh`. What
  is returned is unordered.

  This is a very thin veneer over `traverse()` which came out of the fact that
  I kept writing "get me the contents" traversal visitors again and again, so
  eventually I just wrote a library edition. Its only "clever" bit is that it
  stores the contents in thread local storage, and merges the contents afterwards.

  It is race free to concurrent relocations of `dirh`. It is entirely implemented
  in header-only code, as it is very simple.
  */
  inline result<contents_visitor::contents_type> contents(const path_handle &dirh, contents_visitor *visitor = nullptr, size_t threads = 0,
                                                          bool force_slow_path = false) noexcept
  {
    contents_visitor default_visitor;
    if(visitor == nullptr)
    {
      visitor = &default_visitor;
    }
    contents_visitor::_state_type state(dirh);
    OUTCOME_TRY(auto &&dirhpath, dirh.current_path());
    state.rootdirpathlen.store(dirhpath.native().size() + 1, std::memory_order_relaxed);
    OUTCOME_TRY(traverse(dirh, visitor, threads, &state, force_slow_path));
    return {std::move(state.contents)};
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#endif
