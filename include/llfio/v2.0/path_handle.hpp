/* A handle to a filesystem location
(C) 2017-2020 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Jul 2017


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

#ifndef LLFIO_PATH_HANDLE_H
#define LLFIO_PATH_HANDLE_H

#include "handle.hpp"
#include "path_view.hpp"

//! \file path_handle.hpp Provides a handle to a filesystem location.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class directory_handle;

/*! \class path_handle
\brief A handle to somewhere originally identified by a path on the filing system. Typically used
as the lightest weight handle to some location on the filing system which may
unpredictably relocate over time. This handle is thus an *anchor* to a subset island of the
filing system, free of any race conditions introduced by third party changes to any part
of the path leading to that island.
*/
class LLFIO_DECL path_handle : public handle
{
  friend class directory_handle;

public:
  using path_type = handle::path_type;
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;
  using mode = handle::mode;
  using creation = handle::creation;
  using caching = handle::caching;
  using flag = handle::flag;

  //! The path view type used by this handle
  using path_view_type = path_view;

  //! Default constructor
  constexpr path_handle() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  explicit constexpr path_handle(native_handle_type h, caching caching, flag flags)
      : handle(h, caching, flags)
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr path_handle(handle &&o) noexcept
      : handle(std::move(o))
  {
  }
  //! Move construction permitted
  path_handle(path_handle &&) = default;
  //! No copy construction (use `clone()`)
  path_handle(const path_handle &) = delete;
  //! Move assignment permitted
  path_handle &operator=(path_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~path_handle();
    new(this) path_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  path_handle &operator=(const path_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(path_handle &o) noexcept
  {
    path_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  //! A `path_handle` returning edition of `handle::clone()`
  result<path_handle> clone_to_path_handle() const noexcept
  {
    OUTCOME_TRY(auto &&newh, handle::clone());
    return path_handle(std::move(newh));
  }

  /*! Returns whether a file entry exists more efficiently that opening and
  closing a `file_handle`. Note that this can be a rich source of TOCTOU
  security attacks! Be aware that symbolic links are NOT dereferenced, so
  a subsequent file handle open may fail.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<bool> exists(path_view_type path) const noexcept;
  //! \overload
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<bool> exists(const path_handle &base, path_view_type path) noexcept { return base.exists(path); }

  /*! Create a path handle opening access to some location on the filing system.
  Some operating systems provide a particularly lightweight method of doing this
  (Linux: `O_PATH`, Windows: no access perms) which is much faster than opening
  a directory. For other systems, we open a directory with read only permissions.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<path_handle> path(const path_handle &base, path_view_type path) noexcept;
  //! \overload
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<path_handle> path(path_view_type _path) noexcept { return path(path_handle(), _path); }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~path_handle() override
  {
    if(_v)
    {
      (void) path_handle::close();
    }
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
#ifndef NDEBUG
    if(_v)
    {
      // Tell handle::close() that we have correctly executed
      _v.behaviour |= native_handle_type::disposition::_child_close_executed;
    }
#endif
    return handle::close();
  }
};

//! \brief Constructor for `path_handle`
template <> struct construct<path_handle>
{
  const path_handle &base;
  path_handle::path_view_type _path;
  result<path_handle> operator()() const noexcept { return path_handle::path(base, _path); }
};

// BEGIN make_free_functions.py
/*! Create a path handle opening access to some location on the filing system.
Some operating systems provide a particularly lightweight method of doing this
(Linux: `O_PATH`, Windows: no access perms) which is much faster than opening
a directory. For other systems, we open a directory with read only permissions.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<path_handle> path(const path_handle &base, path_handle::path_view_type path) noexcept
{
  return path_handle::path(std::forward<decltype(base)>(base), std::forward<decltype(path)>(path));
}
//! \overload
inline result<path_handle> path(path_handle::path_view_type _path) noexcept
{
  return path_handle::path(std::forward<decltype(_path)>(_path));
}
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/path_handle.ipp"
#else
#include "detail/impl/posix/path_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
