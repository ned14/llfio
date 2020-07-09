/* A filesystem algorithm which generates the difference between two directory trees
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

#ifndef LLFIO_ALGORITHM_DIFFERENCE_HPP
#define LLFIO_ALGORITHM_DIFFERENCE_HPP

#include "traverse.hpp"

//! \file compare.hpp Provides a directory tree difference algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  /*!
   */
  struct difference_item
  {
    enum change_t : uint8_t
    {
      unknown,
      content_metadata_changed,     //!< Maximum extent or modified timestamp metadata has changed
      noncontent_metadata_changed,  //!< Non-content metadata (perms, non-modified timestamps etc) has changed
      file_added,                   //!< A new file was added
      file_renamed,                 //!< A file was renamed within this tree
      file_linked,                  //!< A hard link to something else also in this tree was added
      file_removed,                 //!< A file was removed
      directory_added,              //!< A new directory was added
      directory_renamed,            //!< A directory was renamed within this tree
      directory_removed,            //!< A directory was removed
      symlink_added,                //!< A symlink was added
      symlink_removed               //!< A symlink was removed
    } changed{change_t::unknown};
    int8_t content_comparison{0};  //!< `memcmp()` of content, if requested
  };
  /*! \brief A visitor for the filesystem traversal and comparison algorithm.

  Note that at any time, returning a failure causes `compare()` to exit as soon
  as possible with the same failure.

  You can override the members here inherited from `traverse_visitor`, however note
  that `compare()` is entirely implemented using `traverse()`, so not calling the
  implementations here will affect operation.
  */
  struct compare_visitor : public traverse_visitor
  {
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

  /*! \brief Summarise the directory identified `dirh`, and everything therein.

  It can be useful to summarise a directory hierarchy, especially to determine how
  much storage it occupies, but also how many mounted filesystems it straddles etc.
  You should specify what metadata you wish to summarise, if this is a subset of
  what metadata `directory_handle::read()` returns, performance will be considerably
  better. The default summarises all possible metadata.

  This is a trivial implementation on top of `algorithm::traverse()`, indeed it is
  implemented entirely as header code. You should review the documentation for
  `algorithm::traverse()`, as this algorithm is entirely implemented using that algorithm.
  */
  inline result<traversal_summary> summarize(const path_handle &dirh, stat_t::want want = traversal_summary::default_metadata(),
                                             summarize_visitor *visitor = nullptr, size_t threads = 0, bool force_slow_path = false) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(&dirh);
    summarize_visitor default_visitor;
    if(visitor == nullptr)
    {
      visitor = &default_visitor;
    }
    result<traversal_summary> state(in_place_type<traversal_summary>);
    state.assume_value().want = want;
    directory_entry entry{{}, stat_t(nullptr)};
    OUTCOME_TRY(entry.stat.fill(dirh, want));
    OUTCOME_TRY(summarize_visitor::accumulate(state.assume_value(), &state.assume_value(), nullptr, entry, want));
    OUTCOME_TRY(traverse(dirh, visitor, threads, &state.assume_value(), force_slow_path));
    return state;
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#endif
