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


//! \file traverse.hpp Provides a directory tree traversal algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  /*! \brief A visitor for the filesystem traversal algorithm.

  Note that at any time, returning a failure causes `traverse()` to exit as soon
  as possible with the same failure. Depth is how deep into the directory
  hierarchy we are, with zero being the base path handle being traversed.
  */
  struct traverse_visitor
  {
    virtual ~traverse_visitor() {}

    /*! \brief Called when we failed to open a directory for enumeration.
    The default fails the traversal with that error. Return a default constructed
    instance to ignore the failure.

    \note May be called from multiple kernel threads concurrently.
    */
    virtual result<directory_handle> directory_open_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, path_view leaf, size_t depth) noexcept
    {
      (void) data;
      (void) dirh;
      (void) leaf;
      (void) depth;
      return failure(std::move(error));
    }

    /*! \brief Called to decide whether to enumerate a directory.

    Note that it is more efficient to ignore the directory entry in
    `post_enumeration()` than to ignore it here, as a handle is opened
    for the directory before this callback. Equally, if you need that
    handle to inspect the directory e.g. to check if one is entering
    a different filesystem from the root, here is best.

    The default returns `true`.

    \note May be called from multiple kernel threads concurrently.
    */
    virtual result<bool> pre_enumeration(void *data, const directory_handle &dirh, size_t depth) noexcept
    {
      (void) data;
      (void) dirh;
      (void) depth;
      return true;
    }

    /*! \brief Called just after each directory enumeration. You can
    modify the results to affect the traversal, typically you would
    set the stat to default constructed to cause it to be ignored.
    You are guaranteed that at least `stat.st_type` is valid for
    every entry in `contents`, and items whose type is a directory will
    be enqueued after this call for later traversal.

    \note May be called from multiple kernel threads concurrently.
    */
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept
    {
      (void) data;
      (void) dirh;
      (void) contents;
      (void) depth;
      return success();
    }

    /*! \brief Called whenever the traversed stack of directory hierarchy is updated.
    This can act as an estimated progress indicator, or to give an
    accurate progress indicator by matching it against a previous
    traversal.
    \data The third party data pointer passed to `traverse()`.
    \param dirs_processed The total number of directories traversed so far.
    \param known_dirs_remaining The currently known number of directories
    awaiting traversal.
    \param depth_processed How many levels deep we have already completely
    traversed.
    \param known_depth_remaining The currently known number of levels we
    shall traverse.

    \note May be called from multiple kernel threads concurrently.
    */
    virtual result<void> stack_updated(void *data, size_t dirs_processed, size_t known_dirs_remaining, size_t depth_processed, size_t known_depth_remaining) noexcept
    {
      (void) data;
      (void) dirs_processed;
      (void) known_dirs_remaining;
      (void) depth_processed;
      (void) known_depth_remaining;
      return success();
    }

    /*! \brief Called when a traversal finishes, whether due to success
    or failure. Always called from the original thread.
    */
    virtual result<size_t> finished(void *data, result<size_t> result) noexcept
    {
      (void) data;
      return result;
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

  This algorithm is therefore primarily a breadth-first algorithm, in that we proceed from
  root, level by level, to the tips. The number returned is the total number of directories
  traversed.

  ## Notes

  The implementation tries hard to not open too many file descriptors at a time in order to
  not exceed the system limit, which may be as low as 1024 on POSIX. On POSIX, it checks
  `getrlimit(RLIMIT_NOFILE)` for the soft limit on open file descriptors, and if the remaining
  unused open file descriptors is less than 65536, it will prefer a slow path implementation
  which exchanges file descriptor usage for lots more dynamic memory allocation and memory
  copying. You can force this slow path on any platform using `force_slow_path`, and in
  correct code you should always check for failures in `directory_open_failed()` comparing
  equal to `errc::too_many_files_open`, and if encountered restart the traversal using the
  slow path forced.

  Almost every modern POSIX system allows a `RLIMIT_NOFILE` of over a million nowadays,
  so you should `setrlimit(RLIMIT_NOFILE)` appropriately in your program if you are absolutely
  sure that doing so will not break code in your program (e.g. `select()` fails spectacularly
  if file descriptors exceed 1024 on most POSIX).

  To give an idea of the difference slow path makes, for Linux ext4:

  - Slow path, 1 thread, traversed 131,915 directories and 8,254,162 entries in 3.10 seconds.

  - Slow path, 16 threads, traversed 131,915 directories and 8,254,162 entries in 0.966 seconds.

  - Fast path, 1 thread, traversed 131,915 directories and 8,254,162 entries in 2.73 seconds (+12%).

  - Fast path, 16 threads, traversed 131,915 directories and 8,254,162 entries in 0.525 seconds (+46%).
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> traverse(const path_handle &dirh, traverse_visitor *visitor, size_t threads = 0, void *data = nullptr, bool force_slow_path = false) noexcept;

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../detail/impl/traverse.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif


#endif
