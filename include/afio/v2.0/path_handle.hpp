/* A handle to a filesystem location
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#ifndef AFIO_PATH_HANDLE_H
#define AFIO_PATH_HANDLE_H

#include "handle.hpp"
#include "path_view.hpp"

//! \file path_handle.hpp Provides a handle to a filesystem location.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

AFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class path_handle
\brief A handle to somewhere originally identified by a path on the filing system. Typically used
as the lightest weight handle to some location on the filing system which may
unpredictably relocate over time. This handle is thus an *anchor* to a subset island of the
filing system, free of any race conditions introduced by third party changes to any part
of the path leading to that island.
*/
class AFIO_DECL path_handle : public handle
{
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
  path_handle() = default;
  //! Construct a handle from a supplied native handle
  explicit constexpr path_handle(native_handle_type h)
      : handle(h, caching::all, flag::none)
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr path_handle(handle &&o) noexcept : handle(std::move(o)) {}
  //! Move construction permitted
  path_handle(path_handle &&) = default;
  //! Move assignment permitted
  path_handle &operator=(path_handle &&) = default;

  /*! Create a path handle opening access to some location on the filing system.
  Some operating systems provide a particularly lightweight method of doing this
  (Linux: `O_PATH`, Windows: no access perms) which is much faster than opening
  a directory. For other systems, we open a directory with read only permissions.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<path_handle> path(const path_handle &base, path_view_type _path) noexcept;
  //! \overload
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<path_handle> path(path_view_type _path) noexcept { return path(path_handle(), _path); }
};

// BEGIN make_free_functions.py
/*! Create a path handle opening access to some location on the filing system.
Some operating systems provide a particularly lightweight method of doing this
(Linux: `O_PATH`, Windows: no access perms) which is much faster than opening
a directory. For other systems, we open a directory with read only permissions.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<path_handle> path(const path_handle &base, path_handle::path_view_type _path) noexcept
{
  return path_handle::path(std::forward<decltype(base)>(base), std::forward<decltype(_path)>(_path));
}
//! \overload
inline result<path_handle> path(path_handle::path_view_type _path) noexcept
{
  return path_handle::path(std::forward<decltype(_path)>(_path));
}
// END make_free_functions.py

AFIO_V2_NAMESPACE_END

#if AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/path_handle.ipp"
#else
#include "detail/impl/posix/path_handle.ipp"
#endif
#undef AFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
