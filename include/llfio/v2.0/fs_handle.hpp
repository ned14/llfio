/* A filing system handle
(C) 2017-2020 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Aug 2017


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

#ifndef LLFIO_FS_HANDLE_H
#define LLFIO_FS_HANDLE_H

#include "path_handle.hpp"
#include "path_view.hpp"

#include "quickcpplib/uint128.hpp"

//! \file fs_handle.hpp Provides fs_handle

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class fs_handle;

//! \brief The kinds of win32 path namespace possible.
enum class win32_path_namespace
{
  /*! Map the input path to a valid win32 path as fast as possible for the input.
  This is currently `guid_volume` followed by `dos`, but may change in the future.
  */
  any,
  /*! Map `\!!\Device\...` form input paths to `\\.\...` for which it is _usually_
  the case there is a mapping, which results in a valid Win32 path, but which
  legacy code bases may not accept. This efficiently covers the vast majority of
  what can be returned by `handle::current_path()` on Windows, but if the input
  path cannot be mapped, a failure is returned.
  */
  device,
  /*! Map the input path to a DOS drive letter prefix, possibly with `\\?\` prefix
  to opt out of strict DOS path parsing if the mapped DOS path is incompatible with
  traditional DOS (e.g. it contains one of the forbidden character sequences such
  as `CON`, or it exceeds 260 codepoints, and so on). Well written software will
  correctly handle `\\?\` prefixes, but if the code you are handing the path to is
  particularly legacy, you ought to ensure that the prefix is not present.

  \warning There is not a one-one mapping between NT kernel paths (which is what
  LLFIO returns from `handle::current_path()`) and DOS style paths, so what you get
  may be surprising. It is also possible that there is no mapping at all, in which
  case a failure is returned.
  */
  dos,
  /*! Map the input path replacing the volume as a GUID, such that say an input path
  of `C:\foo\bar` might be mapped to `\\?\Volume{9f9bd10e-9003-4da5-b146-70584e30854a}\foo\bar`.
  This is a valid Win32 path, but legacy code bases may not accept it. This eliminates
  problems with drive letters vanishing or being ambiguous, and unlike with `dos`,
  there is a guaranteed one-one mapping between NT kernel paths and `guid_volume` paths.
  The mapped path is NOT checked for equivalence to the input file.
  */
  guid_volume
#if 0
  /*! Map the input path replacing the the whole path as a GUID, such that say an input
  path of `C:\foo\bar` might be mapped to `\\?\Volume{9f9bd10e-9003-4da5-b146-70584e30854a}\{5a13b46c-44b9-40f3-9303-23cf7d918708}`.
  This is a valid Win32 path, but legacy code bases may not accept it. This eliminates
  problems with long paths or if the file could be renamed concurrently. Note this may
  cause the creation of a GUID for the file on some filesystems (NTFS). The mapped path
  is NOT checked for equivalence to the input file.
  */
  guid_all

/*
- `win32_path_namespace::guid_all` does the same as `guid_volume`, but additionally
asks Windows for the GUID for the file upon the volume, creating one if one
doesn't exist if necessary. The path returned consists of two GUIDs, and is a
perfectly valid Win32 path which most Win32 APIs will accept.
*/
#endif
};

/*! \brief Maps the current path of `h` into a form suitable for Win32 APIs.
Passes through unmodified on POSIX, so you can use this in portable code.
\return The mapped current path of `h`, which may have been validated to refer to
the exact same inode via `.unique_id()` (see below).
\param h The handle whose `.current_path()` is to be mapped into a form suitable
for Win32 APIs.
\param mapping Which Win32 path namespace to map onto.

This implementation may need to validate that the mapping of the current path of `h`
onto the desired Win32 path namespace does indeed refer to the same file:

- `win32_path_namespace::device` transforms `\!!\Device\...` => `\\.\...` and
ensures that the mapped file's unique id matches the original, otherwise
returning failure.
- `win32_path_namespace::dos` enumerates all the DOS devices on the system and
what those map onto within the NT kernel namespace. This mapping is for
obvious reasons quite slow.
- `win32_path_namespace::guid_volume` simply fetches the GUID of the volume of
the handle, and constructs a valid Win32 path from that.
- `win32_path_namespace::any` means attempt `guid_volume` first, and if it fails
(e.g. your file is on a network share) then it attempts `dos`. This semantic may
change in the future, however any path emitted will always be a valid Win32 path.
*/
#ifdef _WIN32
LLFIO_HEADERS_ONLY_FUNC_SPEC result<filesystem::path> to_win32_path(const fs_handle &h, win32_path_namespace mapping = win32_path_namespace::any) noexcept;
LLFIO_TEMPLATE(class T)
LLFIO_TREQUIRES(LLFIO_TPRED(!std::is_base_of<fs_handle, T>::value && std::is_base_of<handle, T>::value))
inline result<filesystem::path> to_win32_path(const T &h, win32_path_namespace mapping = win32_path_namespace::any) noexcept
{
  struct wrapper final : public fs_handle
  {
    const handle &_h;
    explicit wrapper(const handle &h)
        : _h(h)
    {
    }
    virtual const handle &_get_handle() const noexcept override { return _h; }
  } _(h);
  return to_win32_path(_, mapping);
}
#else
inline result<filesystem::path> to_win32_path(const fs_handle &h, win32_path_namespace mapping = win32_path_namespace::any) noexcept;
LLFIO_TEMPLATE(class T)
LLFIO_TREQUIRES(LLFIO_TPRED(!std::is_base_of<fs_handle, T>::value && std::is_base_of<handle, T>::value))
inline result<filesystem::path> to_win32_path(const T &h, win32_path_namespace /*unused*/ = win32_path_namespace::any) noexcept
{
  return h.current_path();
}
#endif

/*! \class fs_handle
\brief A handle to something with a device and inode number.

\sa `algorithm::cached_parent_handle_adapter<T>`
*/
class LLFIO_DECL fs_handle
{
#ifdef _WIN32
  friend LLFIO_HEADERS_ONLY_FUNC_SPEC result<filesystem::path> to_win32_path(const fs_handle &h, win32_path_namespace mapping) noexcept;
#else
  friend inline result<filesystem::path> to_win32_path(const fs_handle &h, win32_path_namespace /*unused*/) noexcept { return h._get_handle().current_path(); }
#endif

public:
  using dev_t = uint64_t;
  using ino_t = uint64_t;
  //! The path view type used by this handle
  using path_view_type = path_view;
  //! The unique identifier type used by this handle
  using unique_id_type = QUICKCPPLIB_NAMESPACE::integers128::uint128;
  //! A hasher for the unique identifier type used by this handle
  using unique_id_type_hasher = QUICKCPPLIB_NAMESPACE::integers128::uint128_hasher;

protected:
  mutable dev_t _devid{0};
  mutable ino_t _inode{0};

  //! Fill in _devid and _inode from the handle via fstat()
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> _fetch_inode() const noexcept;

  virtual const handle &_get_handle() const noexcept = 0;

protected:
  //! Default constructor
  constexpr fs_handle() {}  // NOLINT
  ~fs_handle() = default;
  //! Construct a handle
  constexpr fs_handle(dev_t devid, ino_t inode)
      : _devid(devid)
      , _inode(inode)
  {
  }
  //! Implicit move construction of fs_handle permitted
  constexpr fs_handle(fs_handle &&o) noexcept
      : _devid(o._devid)
      , _inode(o._inode)
  {
    o._devid = 0;
    o._inode = 0;
  }
  //! Move assignment of fs_handle permitted
  fs_handle &operator=(fs_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    _devid = o._devid;
    _inode = o._inode;
    o._devid = 0;
    o._inode = 0;
    return *this;
  }

public:
  //! No copy construction (use `clone()`)
  fs_handle(const fs_handle &) = delete;
  //! No copy assignment
  fs_handle &operator=(const fs_handle &o) = delete;

  //! Unless `flag::disable_safety_unlinks` is set, the device id of the file when opened
  dev_t st_dev() const noexcept
  {
    if(_devid == 0 && _inode == 0)
    {
      (void) _fetch_inode();
    }
    return _devid;
  }
  //! Unless `flag::disable_safety_unlinks` is set, the inode of the file when opened. When combined with st_dev(), forms a unique identifer on this system
  ino_t st_ino() const noexcept
  {
    if(_devid == 0 && _inode == 0)
    {
      (void) _fetch_inode();
    }
    return _inode;
  }
  //! A unique identifier for this handle across the entire system. Can be used in hash tables etc.
  unique_id_type unique_id() const noexcept
  {
    if(_devid == 0 && _inode == 0)
    {
      (void) _fetch_inode();
    }
    unique_id_type ret;
    ret.as_longlongs[0] = _devid;
    ret.as_longlongs[1] = _inode;
    return ret;
  }

  /*! Obtain a handle to the path **currently** containing this handle's file entry.

  \warning This call is \b racy and can result in the wrong path handle being returned. Note that
  unless `flag::disable_safety_unlinks` is set, this implementation opens a
  `path_handle` to the source containing directory, then checks if the file entry within has the
  same inode as the open file handle. It will retry this matching until
  success until the deadline given.

  \mallocs Calls `current_path()` and thus is both expensive and calls malloc many times.

  \sa `algorithm::cached_parent_handle_adapter<T>` which overrides this with a zero cost
  implementation, thus making unlinking and relinking very considerably quicker.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_handle> parent_path_handle(deadline d = std::chrono::seconds(30)) const noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(parent_path_handle)

  /*! Relinks the current path of this open handle to the new path specified. If `atomic_replace` is
  true, the relink \b atomically and silently replaces any item at the new path specified. This
  operation is both atomic and matching POSIX behaviour even on Microsoft Windows where
  no Win32 API can match POSIX semantics.

  Note that if `atomic_replace` is false, the operation *may* be implemented as creating a hard
  link to the destination (which fails if the destination exists), opening a new file descriptor
  to the destination, closing the existing file descriptor, replacing the existing file descriptor
  with the new one (this is to ensure path tracking continues to work), then unlinking the previous
  link. Thus `native_handle()`'s value *may* change. This is not the case on Microsoft Windows nor
  Linux, both of which provide syscalls capable of refusing to rename if the destination exists.

  If the handle refers to a pipe, on Microsoft Windows the base path handle is ignored as there is
  a single global named pipe namespace. Unless the path fragment begins with `\`, the string `\??\`
  is prefixed to the name before passing it to the NT kernel API which performs the rename. This
  is because `\\.\` in Win32 maps onto `\??\` in the NT kernel.

  \warning Some operating systems provide a race free syscall for renaming an open handle (Windows).
  On all other operating systems this call is \b racy and can result in the wrong file entry being
  relinked. Note that unless `flag::disable_safety_unlinks` is set, this implementation opens a
  `path_handle` to the source containing directory first, then checks before relinking that the item
  about to be relinked has the same inode as the open file handle. It will retry this matching until
  success until the deadline given. This should prevent most unmalicious accidental loss of data.

  \param base Base for any relative path.
  \param path The relative or absolute new path to relink to.
  \param atomic_replace Atomically replace the destination if a file entry already is present there.
  Choosing false for this will fail if a file entry is already present at the destination, and may
  not be an atomic operation on some platforms (i.e. both the old and new names may be linked to the
  same inode for a very short period of time). Windows and recent Linuxes are always atomic.
  \param d The deadline by which the matching of the containing directory to the open handle's inode
  must succeed, else `errc::timed_out` will be returned.
  \mallocs Except on platforms with race free syscalls for renaming open handles (Windows), calls
  `current_path()` via `parent_path_handle()` and thus is both expensive and calls malloc many times.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> relink(const path_handle &base, path_view_type path, bool atomic_replace = true, deadline d = std::chrono::seconds(30)) noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(relink)

  /*! Links the inode referred to by this open handle to the path specified. The current path
  of this open handle is not changed, unless it has no current path due to being unlinked.

  \warning Some operating systems provide a race free syscall for linking an open handle to a new
  location (Linux, Windows).
  On all other operating systems this call is \b racy and can result in the wrong inode being
  linked. Note that unless `flag::disable_safety_unlinks` is set, this implementation opens a
  `path_handle` to the source containing directory first, then checks before linking that the item
  about to be hard linked has the same inode as the open file handle. It will retry this matching
  until success until the deadline given. This should prevent most unmalicious accidental loss of
  data.

  \param base Base for any relative path.
  \param path The relative or absolute new path to hard link to.
  \param d The deadline by which the matching of the containing directory to the open handle's inode
  must succeed, else `errc::timed_out` will be returned.
  \mallocs Except on platforms with race free syscalls for renaming open handles (Windows), calls
  `current_path()` via `parent_path_handle()` and thus is both expensive and calls malloc many times.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> link(const path_handle &base, path_view_type path, deadline d = std::chrono::seconds(30)) noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(link)

  /*! Unlinks the current path of this open handle, causing its entry to immediately disappear
  from the filing system. On Windows before Windows 10 1709 unless
  `flag::win_disable_unlink_emulation` is set, this behaviour is simulated by renaming the file
  to something random and setting its delete-on-last-close flag. Note that Windows may prevent
  the renaming of a file in use by another process, if so it will NOT be renamed.
  After the next handle to that file closes, it will become permanently unopenable by anyone
  else until the last handle is closed, whereupon the entry will be eventually removed by the
  operating system.

  \warning Some operating systems provide a race free syscall for unlinking an open handle (Windows).
  On all other operating systems this call is \b racy and can result in the wrong file entry being
  unlinked. Note that unless `flag::disable_safety_unlinks` is set, this implementation opens a
  `path_handle` to the containing directory first, then checks that the item about to be unlinked
  has the same inode as the open file handle. It will retry this matching until success until the
  deadline given. This should prevent most unmalicious accidental loss of data.

  \param d The deadline by which the matching of the containing directory to the open handle's inode
  must succeed, else `errc::timed_out` will be returned.
  \mallocs Except on platforms with race free syscalls for unlinking open handles (Windows), calls
  `current_path()` and thus is both expensive and calls malloc many times. On Windows, also calls
  `current_path()` if `flag::disable_safety_unlinks` is not set.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
  result<void> unlink(deadline d = std::chrono::seconds(30)) noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(unlink)
};

namespace detail
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<path_handle> containing_directory(optional<std::reference_wrapper<filesystem::path>> out_filename, const handle &h,
                                                                        const fs_handle &fsh, deadline d) noexcept;
}

// BEGIN make_free_functions.py
/*! Relinks the current path of this open handle to the new path specified. If `atomic_replace` is
true, the relink \b atomically and silently replaces any item at the new path specified. This operation
is both atomic and silent matching POSIX behaviour even on Microsoft Windows where
no Win32 API can match POSIX semantics.

\warning Some operating systems provide a race free syscall for renaming an open handle (Windows).
On all other operating systems this call is \b racy and can result in the wrong file entry being
relinked. Note that unless `flag::disable_safety_unlinks` is set, this implementation opens a
`path_handle` to the source containing directory first, then checks before relinking that the item
about to be relinked has the same inode as the open file handle. It will retry this matching until
success until the deadline given. This should prevent most unmalicious accidental loss of data.

\param self The object whose member function to call.
\param base Base for any relative path.
\param path The relative or absolute new path to relink to.
\param atomic_replace Atomically replace the destination if a file entry already is present there.
Choosing false for this will fail if a file entry is already present at the destination, and may
not be an atomic operation on some platforms (i.e. both the old and new names may be linked to the
same inode for a very short period of time). Windows and recent Linuxes are always atomic.
\param d The deadline by which the matching of the containing directory to the open handle's inode
must succeed, else `errc::timed_out` will be returned.
\mallocs Except on platforms with race free syscalls for renaming open handles (Windows), calls
`current_path()` via `parent_path_handle()` and thus is both expensive and calls malloc many times.
*/
inline result<void> relink(fs_handle &self, const path_handle &base, fs_handle::path_view_type path, bool atomic_replace = true,
                           deadline d = std::chrono::seconds(30)) noexcept
{
  return self.relink(std::forward<decltype(base)>(base), std::forward<decltype(path)>(path), std::forward<decltype(atomic_replace)>(atomic_replace),
                     std::forward<decltype(d)>(d));
}
/*! Unlinks the current path of this open handle, causing its entry to immediately disappear from the filing system.
On Windows unless `flag::win_disable_unlink_emulation` is set, this behaviour is
simulated by renaming the file to something random and setting its delete-on-last-close flag.
Note that Windows may prevent the renaming of a file in use by another process, if so it will
NOT be renamed.
After the next handle to that file closes, it will become permanently unopenable by anyone
else until the last handle is closed, whereupon the entry will be eventually removed by the
operating system.

\warning Some operating systems provide a race free syscall for unlinking an open handle (Windows).
On all other operating systems this call is \b racy and can result in the wrong file entry being
unlinked. Note that unless `flag::disable_safety_unlinks` is set, this implementation opens a
`path_handle` to the containing directory first, then checks that the item about to be unlinked
has the same inode as the open file handle. It will retry this matching until success until the
deadline given. This should prevent most unmalicious accidental loss of data.

\param self The object whose member function to call.
\param d The deadline by which the matching of the containing directory to the open handle's inode
must succeed, else `errc::timed_out` will be returned.
\mallocs Except on platforms with race free syscalls for unlinking open handles (Windows), calls
`current_path()` and thus is both expensive and calls malloc many times. On Windows, also calls
`current_path()` if `flag::disable_safety_unlinks` is not set.
*/
inline result<void> unlink(fs_handle &self, deadline d = std::chrono::seconds(30)) noexcept
{
  return self.unlink(std::forward<decltype(d)>(d));
}
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/fs_handle.ipp"
#else
#include "detail/impl/posix/fs_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
