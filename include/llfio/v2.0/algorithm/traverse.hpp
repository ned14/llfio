/* A filesystem algorithm which traverses a directory tree
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

#ifndef LLFIO_ALGORITHM_TRAVERSE_HPP
#define LLFIO_ALGORITHM_TRAVERSE_HPP

#include "../directory_handle.hpp"


//! \file remove_all.hpp Provides a directory tree traversal algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  /*! \struct traverse_visitor A visitor for the filesystem traversal algorithm.

  Note that at any time, returning a failure causes `traverse()` to exit as soon
  as possible with the same failure.
  */
  struct traverse_visitor
  {
    virtual ~traverse_visitor() {}

    /*! \brief Called when we failed to open a directory for enumeration.
    The default fails the traversal with that error. Return a default constructed
    instance to ignore the failure.
    */
    virtual result<directory_handle> directory_open_failed(result<void>::error_type &&error, const directory_handle &dirh, path_view leaf) noexcept
    {
      (void) dirh;
      (void) leaf;
      return failure(std::move(error));
    }

    /*! \brief Called to decide whether to enumerate a directory.

    Note that it is more efficient to remove the directory entry in
    `post_enumeration()` than to ignore it here, as a handle is opened
    for the directory before this callback. Equally, if you need that
    handle to inspect the directory, here is best.

    The default returns `true`.
    */
    virtual result<bool> pre_enumeration(const directory_handle &dirh) noexcept
    {
      (void) dirh;
      return true;
    }

    /*! \brief Called just after each directory enumeration. You can
    modify the results to affect the traversal.
    */
    virtual result<void> post_enumeration(const directory_handle &dirh, directory_handle::buffers_type &contents) noexcept
    {
      (void) dirh;
      (void) contents;
      return success();
    }

    /*! \brief Called whenever the traversed stack of directory hierarchy is updated.
    This can act as an estimated progress indicator, or to give an
    accurate progress indicator by matching it against a previous
    traversal.
    \param dirs_processed The total number of directories traversed so far.
    \param known_dirs_remaining The currently known number of directories
    awaiting traversal.
    \param depth_processed How many levels deep we have already completely
    traversed.
    \param known_depth_remaining The currently known number of levels we
    shall traverse.
    */
    virtual result<void> stack_updated(size_t dirs_processed, size_t known_dirs_remaining, size_t depth_processed, size_t known_depth_remaining) noexcept
    {
      (void) dirs_processed;
      (void) known_dirs_remaining;
      (void) depth_processed;
      (void) known_depth_remaining;
      return success();
    }
  };


  /*! \brief Traverse everything within and under `dirh`.

  The algorithm is as follows:

  1. Call `pre_enumeration()` of the visitor on the `directory_handle` about to be enumerated.

  2. Enumerate the contents of the directory.

  3. Call `post_enumeration()` of the visitor on the contents just enumerated.

  4. For each directory in the contents, append the directory handle and each directory
  leafname to its hierarchy depth level in a stack of lists.

  5. Loop, using the least deep available item in the stack, until the stack is empty.

  If `known_dirs_remaining` exceeds four, a threadpool of not more than `threads` threads
  is spun up in order to traverse the hierarchy more quickly.

  The number returned is the total number of directories traversed.

  ## Notes

  The implementation tries hard to not open too many file descriptors at a time in order to
  not exceed the system limit, which may be as low as 1024 on POSIX. Inevitably the open file
  descriptor count is a function of the thread count, with approximately two open file
  descriptors possible per thread.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> traverse(const path_handle &dirh, traverse_visitor *visitor, size_t threads = 0) noexcept;

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../detail/impl/traverse.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif


#endif
