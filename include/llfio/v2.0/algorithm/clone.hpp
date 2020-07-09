/* A filesystem algorithm which clones a directory tree
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

#ifndef LLFIO_ALGORITHM_CLONE_HPP
#define LLFIO_ALGORITHM_CLONE_HPP

#include "../file_handle.hpp"
#include "traverse.hpp"

//! \file clone.hpp Provides a directory tree clone algorithm.

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  /*! \brief Clone or copy the extents of the filesystem entity identified by `src`
  to `destdir` optionally renamed to `destleaf`.

  \return The number of bytes cloned or copied.
  \param src The file to clone or copy.
  \param destdir The base to lookup `destleaf` within.
  \param destleaf The leafname to use. If empty, use the same leafname as `src` currently has.
  \param preserve_timestamps Use `stat_t::stamp()` to preserve as much metadata from
  the original to the clone/copy as possible.
  \param force_copy_now Parameter to pass to `file_handle::clone_extents()` to force
  extents to be copied now, not copy-on-write lazily later.
  \param creation How to create the destination file handle. NOTE that if this
  is NOT `always_new`, if the destination has identical maximum extent and last
  modified timestamp (and permissions on POSIX) to the source, it is NOT copied, and
  zero is returned.
  \param d Deadline by which to complete the operation.

  Firstly, a `file_handle` is constructed at the destination using `creation`,
  which defaults to always creating a new inode. The caching used for the
  destination handle is replicated from the source handle -- be aware that
  not caching metadata is expensive.

  Next `file_handle::clone_extents()` with `emulate_if_unsupported = false` is
  called on the whole file content. If extent cloning is supported, this will
  be very fast and not consume new disk space (note: except on networked filesystems).
  If the source file is sparsely allocated, the destination will have identical
  sparse allocation.

  If the previous operation did not succeed, the disk free space is checked
  using `statfs_t`, and if the copy would exceed current disk free space, the
  destination file is unlinked and an error code comparing equal to
  `errc::no_space_on_device` is returned.

  Next, `file_handle::clone_extents()` with `emulate_if_unsupported = true` is
  called on the whole file content. This copies only the allocated extents in
  blocks sized whatever is the large page size on this platform (2Mb on x64).

  Finally, if `preserve_timestamps` is true, the destination file handle is
  restamped with the metadata from the source file handle just before the
  destination file handle is closed.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<file_handle::extent_type> clone_or_copy(file_handle &src, const path_handle &destdir, path_view destleaf = {},
                                                                              bool preserve_timestamps = true, bool force_copy_now = false,
                                                                              file_handle::creation creation = file_handle::creation::always_new,
                                                                              deadline d = {}) noexcept;

#if 0
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4275)  // dll interface
#endif
  /*! \brief A visitor for the filesystem traversal and cloning algorithm.

  Note that at any time, returning a failure causes `copy_directory_hierarchy()`
  or `clone_or_copy()` to exit as soon as possible with the same failure.

  You can override the members here inherited from `traverse_visitor`, however note
  that `copy_directory_hierarchy()` or `clone_or_copy()` is entirely implemented
  using `traverse()`, so not calling the implementations here will affect operation.
  */
  struct LLFIO_DECL clone_copy_link_symlink_visitor : public traverse_visitor
  {
    enum op_t
    {
      none = 0,//!< File content is to be ignored, directories only
      clone,   //!< File content is to be extents cloned when possible
      copy,    //!< File content is to copied now into its destination
      link,    //!< File content is to be hard linked into its destination
      symlink  //!< File content is to be symlinked into its destination
    } op{op_t::clone};
    bool always_create_new_files{false};
    bool follow_symlinks{false};
    std::chrono::steady_clock::duration timeout{std::chrono::seconds(10)};
    std::chrono::steady_clock::time_point begin;

    //! Default constructor
    clone_copy_link_symlink_visitor() = default;
    //! Constructs an instance with the default timeout of ten seconds.
    constexpr clone_copy_link_symlink_visitor(op_t _op, bool _always_create_new_files = false, bool _follow_symlinks = false) : op(_op), always_create_new_files(_always_create_new_files), follow_symlinks(_follow_symlinks) {}
    //! Constructs an instance with the specified timeout.
    constexpr explicit clone_copy_link_symlink_visitor(op_t _op, std::chrono::steady_clock::duration _timeout, bool _always_create_new_files = false,
                                                       bool _follow_symlinks = false)
        : op(_op)
        , always_create_new_files(_always_create_new_files)
        , follow_symlinks(_follow_symlinks)
        , timeout(_timeout)
    {
    }

    /*! \brief This override creates directories in the destination for
    every directory in the source, and optionally clones/copies/links/symlinks
    any file content, optionally dereferencing or not dereferencing symlinks.
    */
    virtual result<void> post_enumeration(void *data, const directory_handle &dirh, directory_handle::buffers_type &contents, size_t depth) noexcept override;
  };
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  /*! \brief Clone or copy the extents of the filesystem entity identified by `srcdir`
  and `srcleaf`, and everything therein, to `destdir` named `srcleaf` or `destleaf`.

  \return The number of items cloned or copied.
  \param srcdir The base to lookup `srcleaf` within.
  \param srcleaf The leafname to lookup. If empty, treat `srcdir` as the directory to clone.
  \param destdir The base to lookup `destleaf` within.
  \param destleaf The leafname to lookup. If empty, treat `destdir` as the directory to clone.
  \param visitor The visitor to use.
  \param threads The number of kernel threads for `traverse()` to use.
  \param force_slow_path The parameter to pass to `traverse()`.

  - `srcleaf` empty and `destleaf` empty: Clone contents of `srcdir` into `destdir`.
  - `srcleaf` non-empty and `destleaf` empty: Clone `srcdir`/`srcleaf` into `destdir`/`srcleaf`.
  - `srcleaf` non-empty and `destleaf` non-empty: Clone `srcdir`/`srcleaf` into `destdir`/`destleaf`.

  This algorithm firstly traverses the source directory tree to calculate the filesystem
  blocks which could be used by a copy of the entity. If insufficient disc space
  remains on the destination volume for a copy of just the directories, the operation
  exits with an error code comparing equal to `errc::no_space_on_device`.

  A single large file is then chosen from the source, and an attempt is made to `file_handle::clone_extents()`
  with `emulate_if_unsupported = false` it into the destination. If extents cloning is
  not successful, and if the allocated blocks for both the files and the directories
  would exceed the free disc space on the destination volume, the destination is removed
  and an error code is returned comparing equal to `errc::no_space_on_device`.

  Otherwise, for every file in the source, its contents are cloned with `clone_or_copy()`
  into an equivalent file in the destination. This means that the contents are either
  cloned or copied to the best extent of your filesystems and kernel. If failure occurs
  during this stage, the destination is left as-is in a partially copied state.

  Note the default visitor parameters: Extent cloning is preferred, we do nothing
  if destination file maximum extent and timestamps are identical to source, we don't
  follow symlinks, instead cloning the symlink itself.

  Note also that files or directories in the destination which are not in the source
  are left untouched.

  You should review the documentation for `algorithm::traverse()`, as this algorithm is
  entirely implemented using that algorithm.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> clone_or_copy(const path_handle &srcdir, path_view srcleaf, const path_handle &destdir, path_view destleaf = {}, clone_copy_link_visitor *visitor = nullptr, size_t threads = 0, bool force_slow_path = false) noexcept;
#endif
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../detail/impl/clone.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif


#endif
