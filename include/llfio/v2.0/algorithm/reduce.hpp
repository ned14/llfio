/* A filesystem algorithm which reduces a directory tree
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: Mar 2020


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

#ifndef LLFIO_ALGORITHM_REDUCE_HPP
#define LLFIO_ALGORITHM_REDUCE_HPP

#include "traverse.hpp"

//! \file reduce.hpp Provides a directory tree reduction algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#pragma warning(disable : 4275)  // dll interface
#endif
  /*! \brief A visitor for the filesystem traversal and reduction algorithm.

  Note that at any time, returning a failure causes `reduce()` to exit as soon
  as possible with the same failure.

  You can override the members here inherited from `traverse_visitor`, however note
  that `reduce()` is entirely implemented using `traverse()`, so not calling the
  implementations here will affect operation.
  */
  struct LLFIO_DECL reduce_visitor : public traverse_visitor
  {
    std::chrono::steady_clock::duration timeout{std::chrono::seconds(10)};
    std::chrono::steady_clock::time_point begin;

    //! Constructs an instance with the default timeout of ten seconds.
    constexpr reduce_visitor() {}
    //! Constructs an instance with the specified timeout.
    constexpr explicit reduce_visitor(std::chrono::steady_clock::duration _timeout)
        : timeout(_timeout)
    {
    }

    //! This override ignores failures to traverse into the directory, and tries renaming the item into the base directory.
    virtual result<directory_handle> directory_open_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, path_view leaf, size_t depth) noexcept override;
    //! This override invokes deletion of all non-directory items. If there are no directory items, also deletes the directory.
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept override;

    /*! \brief Called when the unlink of a file entry failed. The default
    implementation attempts to rename the entry into the base directory.
    If your reimplementation achieves the unlink, return true.

    \note May be called from multiple kernel threads concurrently.
    */
    virtual result<bool> unlink_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, directory_entry &entry, size_t depth) noexcept;
    /*! \brief Called when the rename of a file entry into the base directory
    failed. The default implementation ignores the failure. If your
    reimplementation achieves the rename, return true.

    \note May be called from multiple kernel threads concurrently.
    */
    virtual result<bool> rename_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, directory_entry &entry, size_t depth) noexcept
    {
      (void) data;
      (void) error;
      (void) dirh;
      (void) entry;
      (void) depth;
      return false;
    }
    /*! \brief Called when we have performed a single full round of reduction.

    \note Always called from the original kernel thread.
    */
    virtual result<void> reduction_round(void *data, size_t round_completed, size_t items_unlinked, size_t items_remaining) noexcept
    {
      (void) data;
      (void) round_completed;
      (void) items_unlinked;
      if(items_remaining > 0)
      {
        if(begin == std::chrono::steady_clock::time_point())
        {
          begin = std::chrono::steady_clock::now();
        }
        else if((std::chrono::steady_clock::now() - begin) > timeout)
        {
          return errc::timed_out;
        }
      }
      return success();
    }
  };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  /*! \brief Reduce the directory identified `dirh`, and everything therein, to the null set.

  You might be surprised to learn that most directory tree removal implementations are
  of poor quality, not leveraging the filesystem facilities that are available,
  not handling concurrent modification of the filesystem correctly, having poor
  performance, or failing to handle not uncommon corner cases. This implementation is
  considerably better quality, indeed it is to my knowledge the highest quality
  possible on at least Linux and Microsoft Windows.

  The algorithm is as follows:

  1. Attempt to rename `dirh` to a uniquely named directory. If successful, this causes
  concurrent users to no longer see the directory tree. It also usefully detects if
  permissions problems would prevent whole directory tree removal. Note that on Windows,
  if any process has a handle open to anything inside the directory tree, it is usually
  the case that all renames will be prevented.

  2. `algorithm::traverse()` is begun, using the visitor supplied. This will unlink all
  items using a breadth-first algorithm, from root to tips. With the default visitor,
  directories which cannot be opened for traversal are ignored; entries which cannot be
  unlinked are attempted to be renamed into the base directory; entries which cannot be
  renamed are ignored.

  3. Except for unrenameable files, now the entire directory tree will have been reduced
  to a minimum possible set of uniquely named items in the base directory, all of which
  by definition must be undeletable. We now loop attempting to reduce the remaining
  entries. The default visitor implementation takes a timeout, which if exceeded, an
  error code comparing equal to `errc::timed_out` is returned.

  Even on slow filesystems such as those on Windows, or networked filesystems, this
  algorithm performs very well. We do not currently inspect the filing system to see
  if bisect unlinking directories with millions of entries will perform well (some filing
  systems can store very large directories with multiple independent inode
  locks, thus using multiple kernel threads on the same directory sees a large performance
  increase for very large directories). We also remove items based on enumerated order,
  under the assumption that filesystems will have optimised for this specific use case.

  If the function succeeds, `dirh` is moved into the function, and the total number of
  filesystem entries removed is returned.

  If the function fails, `dirh` is NOT moved into the function, and continues to refer
  to the (likely renamed) directory you passed in. You might do something like try to rename
  it into `storage_backed_temporary_files_directory()`, or some other hail mary action.

  You should review the documentation for `algorithm::traverse()`, as this algorithm is
  entirely implemented using that algorithm.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> reduce(directory_handle &&dirh, reduce_visitor *visitor = nullptr, size_t threads = 0, bool force_slow_path = false) noexcept;

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../detail/impl/reduce.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif


#endif
