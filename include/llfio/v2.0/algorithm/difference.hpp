/* A filesystem algorithm which generates the difference between two directory trees
(C) 2020 - 2023 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
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

#include "summarize.hpp"

//! \file difference.hpp Provides a directory tree difference algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  struct comparison_summary
  {
    struct _lr : traversal_summary
    {
      //! The kind of difference
      enum difference_type : uint8_t
      {
        entry,                //!< Entry name is here but not there
        entry_kind,           //!< Same entry name has different kind e.g. file vs directory
        content_metadata,     //!< Maximum extent or modified timestamp metadata is different
        noncontent_metadata,  //!< Non-content metadata (perms, non-modified timestamps etc) is different
        content,              //!< The content is different (if contents comparison enabled)
        none,                 //!< There is no difference
      };
      traversal_summary::map_type<difference_type> differences;  //!< The number of items with the given difference
    } left, right;
  };
  /*! \brief A visitor for the filesystem traversal and comparison algorithm.

  Note that at any time, returning a failure causes `compare()` to exit as soon
  as possible with the same failure.

  You can override the members here inherited from `traverse_visitor`, however note
  that `compare()` is entirely implemented using `traverse()`, so not calling the
  implementations here will affect operation.
  */
  struct compare_visitor : public summarize_visitor
  {
    static void accumulate(comparison_summary &acc, comparison_summary *state, const directory_handle *dirh, directory_entry &entry,
                           stat_t::want already_have_metadata)
    {
      summarize_visitor::accumulate(acc, state, dirh, entry, already_have_metadata);
    }
    //! This override implements the summary
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept override
    {
      LLFIO_TRY
      {
        auto *state = (comparison_summary *) data;
        comparison_summary acc;
        acc.max_depth = depth;
        for(auto &entry : contents)
        {
          OUTCOME_TRY(accumulate(acc, state, &dirh, entry, contents.metadata()));
        }
        state->operator+=(acc);
        return success();
      }
      LLFIO_CATCH(...)
      {
        return error_from_exception();
      }
    }
  };

  /*! \brief Compares the directories identified by `ldirh` and `rdirh`, and everything therein.

  This extends `summarize()` to summarise two directory hierarchies, also summarising
  the number of types of differences between them.
   
  It is trivially easy to further extend this implementation to synchronise the contents of a
  directory tree such that after completion, both trees shall be identical. See the examples
  directory for an example of this use case.

  This is a trivial implementation on top of `algorithm::traverse()`, indeed it is
  implemented entirely as header code. You should review the documentation for
  `algorithm::traverse()`, as this algorithm is entirely implemented using that algorithm.
  */
  inline result<comparison_summary> compare(const path_handle &ldirh, const path_handle &rdirh, stat_t::want want = comparison_summary::default_metadata(),
                                              compare_visitor *visitor = nullptr, size_t threads = 0, bool force_slow_path = false) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(&dirh);
    compare_visitor default_visitor;
    if(visitor == nullptr)
    {
      visitor = &default_visitor;
    }
    result<comparison_summary> state(in_place_type<comparison_summary>);
    state.assume_value().want = want;
    directory_entry entry{{}, stat_t(nullptr)};
    OUTCOME_TRY(entry.stat.fill(dirh, want));
    OUTCOME_TRY(compare_visitor::accumulate(state.assume_value(), &state.assume_value(), nullptr, entry, want));
    OUTCOME_TRY(traverse(dirh, visitor, threads, &state.assume_value(), force_slow_path));
    return state;
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#endif
