/* A filesystem algorithm which summarises a directory tree
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: May 2020


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

#ifndef LLFIO_ALGORITHM_SUMMARIZE_HPP
#define LLFIO_ALGORITHM_SUMMARIZE_HPP

#include "traverse.hpp"

#include "../stat.hpp"

#include <unordered_map>

//! \file summarize.hpp Provides a directory tree summary algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  /*! \brief A summary of a directory tree
   */
  struct traversal_summary
  {
    //! The default metadata summarised
    static constexpr stat_t::want default_metadata()
    {
      return stat_t::want::dev | stat_t::want::type | stat_t::want::size | stat_t::want::allocated | stat_t::want::blocks;
    }
    template <class T> using map_type = std::unordered_map<T, size_t>;
    spinlock _lock;
    size_t directory_opens_failed{0};  //!< The number of directories which could not be opened.

    stat_t::want want{stat_t::want::none};    //!< The summary items desired
    map_type<uint64_t> devs;                  //!< The number of items with the given device id
    map_type<filesystem::file_type> types;    //!< The number of items with the given type
    handle::extent_type size{0};              //!< The sum of maximum extents. On Windows, is for file content only.
    handle::extent_type allocated{0};         //!< The sum of allocated extents. On Windows, is for file content only.
    handle::extent_type file_blocks{0};       //!< The sum of file allocated blocks.
    handle::extent_type directory_blocks{0};  //!< The sum of directory allocated blocks.
    size_t max_depth{0};                      //!< The maximum depth of the hierarchy

    traversal_summary() {}
    traversal_summary(const traversal_summary &o)
        : directory_opens_failed(o.directory_opens_failed)
        , want(o.want)
        , devs(o.devs)
        , types(o.types)
        , size(o.size)
        , allocated(o.allocated)
        , file_blocks(o.file_blocks)
        , directory_blocks(o.directory_blocks)
        , max_depth(o.max_depth)
    {
      assert(!is_lockable_locked(o._lock));
    }
    traversal_summary(traversal_summary &&o) noexcept
        : directory_opens_failed(o.directory_opens_failed)
        , want(o.want)
        , devs(std::move(o.devs))
        , types(std::move(o.types))
        , size(o.size)
        , allocated(o.allocated)
        , file_blocks(o.file_blocks)
        , directory_blocks(o.directory_blocks)
        , max_depth(o.max_depth)
    {
      assert(!is_lockable_locked(o._lock));
    }
    traversal_summary &operator=(const traversal_summary &o)
    {
      if(this != &o)
      {
        directory_opens_failed = o.directory_opens_failed;
        want = o.want;
        devs = o.devs;
        types = o.types;
        size = o.size;
        allocated = o.allocated;
        file_blocks = o.file_blocks;
        directory_blocks = o.directory_blocks;
        max_depth = o.max_depth;
      }
      return *this;
    }
    traversal_summary &operator=(traversal_summary &&o) noexcept
    {
      if(this != &o)
      {
        directory_opens_failed = o.directory_opens_failed;
        want = o.want;
        devs = std::move(o.devs);
        types = std::move(o.types);
        size = o.size;
        allocated = o.allocated;
        file_blocks = o.file_blocks;
        directory_blocks = o.directory_blocks;
        max_depth = o.max_depth;
      }
      return *this;
    }
    //! Adds another summary to this
    traversal_summary &operator+=(const traversal_summary &o)
    {
      lock_guard<spinlock> g(_lock);
      directory_opens_failed += o.directory_opens_failed;
      for(auto &i : o.devs)
      {
        devs[i.first] += i.second;
      }
      for(auto &i : o.types)
      {
        types[i.first] += i.second;
      }
      size += o.size;
      allocated += o.allocated;
      file_blocks += o.file_blocks;
      directory_blocks += o.directory_blocks;
      max_depth = std::max(max_depth, o.max_depth);
      return *this;
    }
  };

  /*! \brief A visitor for the filesystem traversal and summary algorithm.

  Note that at any time, returning a failure causes `summarize()` to exit as soon
  as possible with the same failure.

  You can override the members here inherited from `traverse_visitor`, however note
  that `summarize()` is entirely implemented using `traverse()`, so not calling the
  implementations here will affect operation.
  */
  struct summarize_visitor : public traverse_visitor
  {
    static result<void> accumulate(traversal_summary &acc, traversal_summary *state, const directory_handle *dirh, directory_entry &entry,
                                   stat_t::want already_have_metadata)
    {
      if((state->want & already_have_metadata) != state->want)
      {
        // Fetch any missing metadata
        if(entry.stat.st_type == filesystem::file_type::directory)
        {
          OUTCOME_TRY(auto &&fh, directory_handle::directory(*dirh, entry.leafname, file_handle::mode::attr_read));
          OUTCOME_TRY(entry.stat.fill(fh, state->want & ~already_have_metadata));
        }
        else
        {
          OUTCOME_TRY(auto &&fh, file_handle::file(*dirh, entry.leafname, file_handle::mode::attr_read));
          OUTCOME_TRY(entry.stat.fill(fh, state->want & ~already_have_metadata));
        }
      }
      if(state->want & stat_t::want::dev)
      {
        acc.devs[entry.stat.st_dev]++;
      }
      if(state->want & stat_t::want::type)
      {
        acc.types[entry.stat.st_type]++;
      }
      if(state->want & stat_t::want::size)
      {
        acc.size += entry.stat.st_size;
      }
      if(state->want & stat_t::want::allocated)
      {
        acc.allocated += entry.stat.st_allocated;
      }
      if(state->want & stat_t::want::blocks)
      {
        if(entry.stat.st_type == filesystem::file_type::directory)
        {
          acc.directory_blocks += entry.stat.st_blocks;
        }
        else
        {
          acc.file_blocks += entry.stat.st_blocks;
        }
      }
      return success();
    }

    //! This override ignores failures to traverse into the directory.
    virtual result<directory_handle> directory_open_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, path_view leaf,
                                                           size_t depth) noexcept override
    {
      (void) error;
      (void) dirh;
      (void) leaf;
      (void) depth;
      auto *state = (traversal_summary *) data;
      lock_guard<spinlock> g(state->_lock);
      state->directory_opens_failed++;
      return success();  // ignore failure to enter
    }
    //! This override implements the summary
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept override
    {
      try
      {
        auto *state = (traversal_summary *) data;
        traversal_summary acc;
        acc.max_depth = depth;
        for(auto &entry : contents)
        {
          OUTCOME_TRY(accumulate(acc, state, &dirh, entry, contents.metadata()));
        }
        state->operator+=(acc);
        return success();
      }
      catch(...)
      {
        return error_from_exception();
      }
    }
  };

  /*! \brief Summarise the directory identified `topdirh`, and everything therein.

  It can be useful to summarise a directory hierarchy, especially to determine how
  much storage it occupies, but also how many mounted filesystems it straddles etc.
  You should specify what metadata you wish to summarise, if this is a subset of
  what metadata `directory_handle::read()` returns, performance will be considerably
  better. The default summarises all possible metadata.

  This is a trivial implementation on top of `algorithm::traverse()`, indeed it is
  implemented entirely as header code. You should review the documentation for
  `algorithm::traverse()`, as this algorithm is entirely implemented using that algorithm.
  */
  inline result<traversal_summary> summarize(const path_handle &topdirh, stat_t::want want = traversal_summary::default_metadata(),
                                             summarize_visitor *visitor = nullptr, size_t threads = 0, bool force_slow_path = false) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(&topdirh);
    summarize_visitor default_visitor;
    if(visitor == nullptr)
    {
      visitor = &default_visitor;
    }
    result<traversal_summary> state(in_place_type<traversal_summary>);
    state.assume_value().want = want;
    directory_entry entry{{}, stat_t(nullptr)};
    directory_handle _dirh;
    if(!topdirh.is_directory())
    {
      OUTCOME_TRY(_dirh, directory_handle::directory(topdirh, {}));
    }
    const path_handle &dirh = _dirh.is_valid() ? _dirh : topdirh;
    OUTCOME_TRY(entry.stat.fill(dirh, want));
    OUTCOME_TRY(summarize_visitor::accumulate(state.assume_value(), &state.assume_value(), nullptr, entry, want));
    OUTCOME_TRY(traverse(dirh, visitor, threads, &state.assume_value(), force_slow_path));
    return state;
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#endif
