/* A filesystem algorithm which removes a directory tree
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

#ifndef LLFIO_ALGORITHM_REMOVE_ALL_HPP
#define LLFIO_ALGORITHM_REMOVE_ALL_HPP

#include "../directory_handle.hpp"


//! \file remove_all.hpp Provides a reliable directory tree removal algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  //! The type of an argument for a `remove_all()` callback.
  union remove_all_callback_arg {
    void *obj;
    uint64_t count;

    //! Construct a null pointer
    constexpr remove_all_callback_arg(std::nullptr_t /*unused*/)
        : obj(nullptr)
    {
    }
    //! Construct a pointer to a `directory_handle`
    constexpr remove_all_callback_arg(directory_handle &h)
        : obj(&h)
    {
    }
    //! Construct a pointer to a `path_view`
    constexpr remove_all_callback_arg(path_view &h)
        : obj(&h)
    {
    }
    //! Construct a count
    constexpr remove_all_callback_arg(uint64_t c)
        : count(c)
    {
    }
  };

  //! The event for which the `remove_all()` callback is being invoked
  enum class remove_all_callback_reason
  {
    begin_enumeration,     //!< Called just before beginning enumeration. arg1 is non zero if the base directory rename was successful.
    progress_enumeration,  //!< Called after the items in a directory have all been unlinked. arg1 is the total number of items not removed for this kernel thread (which can include subdirectories). arg2 is the total number of items removed for this kernel thread.
    end_enumeration,       //!< Called just after ending enumeration. arg1 is the total number of items not removed. arg2 is the total number of items removed.
    finished,              //!< Called just before returning. arg1 is the total number of items not removed. arg2 is the total number of items removed.
    unrenameable,          //!< Called when it was not possible to rename a file entry. arg1 is a pointer to a `directory_handle` which is the directory containing the problem item. arg2 is a pointer to the `path_view` of the leafname.
    unremoveable           //!< Called when it was not possible to unlink a file entry. arg1 is a pointer to a `directory_handle` which is the directory containing the problem item. arg2 is a pointer to the `path_view` of the leafname.
  };

  namespace detail
  {
    LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> remove_all(directory_handle &&dirh, LLFIO_V2_NAMESPACE::function_ptr<result<void>(remove_all_callback_reason reason, remove_all_callback_arg arg1, remove_all_callback_arg arg2)> callback, size_t threads) noexcept;
  }  // namespace detail

  /*! \brief Reliably removes from the filesystem `dirh` and everything under `dirh`.

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

  2. The entire directory tree is enumerated using the requested number of kernel threads.
  All non-directory items are unlinked after each directory enumeration. This is done
  in-line by the kernel thread, as it prevents contention on the kernel mutex on each
  directory inode, and the just-enumerated contents of the directory are fresh in cache.
  If an item cannot be unlinked, it is renamed into a uniquely named file just inside `dirh`.

  3. We recurse into each subdirectory, always trying to prevent each kernel thread from
  touching any part of the filesystem which other kernel threads might be touching, in
  order to prevent contention on the kernel mutex on each directory inode.

  4. Except for unrenameable files, now the entire directory tree will have been removed.
  We now loop attempting to unlink the remaining file entries. The default callback
  implementation
  which if exceeded, an error code comparing equal to `errc::timed_out` is returned.

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

  ## The callback function

  The optional callback function is called with various progress indicators as the algorithm
  executes. Note that it may be called from arbitrary kernel threads, including concurrently.
  Its prototype is `result<void>(remove_all_callback_reason reason, remove_all_callback_arg arg1, remove_all_callback_arg arg2)`.
  Returning a failure cause the whole operation to be cancelled as soon as is practical,
  and that failure is returned from `remove_all()`.

  The default callback function starts a timer when the first `remove_all_callback_reason::unremoveable`
  is seen. If more than ten seconds then elapses, it returns `errc::timed_out`, which in turn
  causes `remove_all()` to return with that failure.

  ## Other notes

  The implementation tries hard to not open too many file descriptors at a time in order to
  not exceed the system limit, which may be as low as 1024 on POSIX. The implementation
  does handle `EMFILE` by giving up what it is doing, and trying again later, however this
  will not help other code running concurrently in other kernel threads which may experience
  spurious `EMFILE` failures.

  Inevitably the open file descriptor count is a function of the thread count. Calling this
  function with `thread = 1` will use far fewer open file descriptors at a time. However, if
  say you are removing a directory containing 1024 other directories, this will unavoidably lead
  to this implementation attempting to open 1024 file descriptors, one for each directory.
  */
  template <class F> inline result<size_t> remove_all(directory_handle &&dirh, F &&callback, size_t threads = 0) noexcept
  {
    return detail::remove_all(std::move(dirh), LLFIO_V2_NAMESPACE::emplace_function_ptr<result<void>(remove_all_callback_reason reason, remove_all_callback_arg arg1, remove_all_callback_arg arg2)>(std::forward<F>(callback)), threads);
  }
  //! \overload With default callback with removal failure timeout of 10 seconds
  inline result<size_t> remove_all(directory_handle &&dirh, size_t threads = 0) noexcept { return detail::remove_all(std::move(dirh), {}, threads); }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../detail/impl/remove_all.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif


#endif
