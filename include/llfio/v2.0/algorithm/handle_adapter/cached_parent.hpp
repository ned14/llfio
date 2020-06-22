/* A parent handle caching adapter
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: Sept 2017


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

#ifndef LLFIO_CACHED_PARENT_HANDLE_ADAPTER_HPP
#define LLFIO_CACHED_PARENT_HANDLE_ADAPTER_HPP

#include "../../directory_handle.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

//! \file handle_adapter/cached_parent.hpp Adapts any `fs_handle` to cache its parent directory handle
LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace detail
  {
    struct LLFIO_DECL cached_path_handle : public std::enable_shared_from_this<cached_path_handle>
    {
      directory_handle h;
      filesystem::path _lastpath;
      explicit cached_path_handle(directory_handle &&_h)
          : h(std::move(_h))
      {
      }
      LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<filesystem::path> current_path(const filesystem::path &append) noexcept;
    };
    using cached_path_handle_ptr = std::shared_ptr<cached_path_handle>;
    // Passed the base and path of the adapted handle being created, returns a handle to the containing directory and the leafname
    LLFIO_HEADERS_ONLY_FUNC_SPEC std::pair<cached_path_handle_ptr, filesystem::path> get_cached_path_handle(const path_handle &base, path_view path);
  }  // namespace detail

  /*! \brief Adapts any `construct()`-able implementation to cache its parent directory handle in a process wide cache.

  For some use cases where one is calling `parent_path_handle()` or code which calls that function very frequently
  e.g. calling `relink()` or `unlink()` a lot on many files with the same parent directory, having to constantly
  fetch the current path, open the parent directory and verify inodes becomes unhelpfully inefficient. This
  adapter keeps a process-wide hash table of directory handles shared between all instances of this adapter,
  thus making calling `parent_path_handle()` almost zero cost.

  This adapter is of especial use on platforms which do not reliably implement per-fd path tracking for regular
  files (Apple MacOS, FreeBSD) as `current_path()` is reimplemented to use the current path of the shared parent
  directory instead. One loses race freedom within the contained directory, but that is the case on POSIX anyway.

  This adapter is also of use on platforms which do not implement path tracking for open handles at all (e.g.
  Linux without `/proc` mounted) as the process-wide cache of directory handles retains the path of the directory
  handle at the time of creation. Third party changes to the part of the filesystem you are working upon will
  result in the inability to do race free unlinking etc, but if no third party changes are encountered it ought
  to work well.

  \todo I have been lazy and used public inheritance from that base i/o handle.
  I should use protected inheritance to prevent slicing, and expose all the public functions by hand.
  */
  template <class T> LLFIO_REQUIRES(sizeof(construct<T>) > 0) class LLFIO_DECL cached_parent_handle_adapter : public T
  {
    static_assert(sizeof(construct<T>) > 0, "Type T must be registered with the construct<T> framework so cached_parent_handle_adapter<T> knows how to construct it");  // NOLINT

  public:
    //! The handle type being adapted
    using adapted_handle_type = T;
    using path_type = typename T::path_type;
    using path_view_type = typename T::path_view_type;

  protected:
    detail::cached_path_handle_ptr _sph;
    filesystem::path _leafname;

  public:
    cached_parent_handle_adapter() = default;
    cached_parent_handle_adapter(const cached_parent_handle_adapter &) = default;
    cached_parent_handle_adapter(cached_parent_handle_adapter &&) = default;  // NOLINT
    cached_parent_handle_adapter &operator=(const cached_parent_handle_adapter &) = default;
    cached_parent_handle_adapter &operator=(cached_parent_handle_adapter &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      this->~cached_parent_handle_adapter();
      new(this) cached_parent_handle_adapter(std::move(o));
      return *this;
    }
    cached_parent_handle_adapter(adapted_handle_type &&o, const path_handle &base, path_view path)
        : adapted_handle_type(std::move(o))
    {
      auto r = detail::get_cached_path_handle(base, path);
      _sph = std::move(r.first);
      _leafname = std::move(r.second);
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~cached_parent_handle_adapter() override
    {
      if(this->_v)
      {
        (void) cached_parent_handle_adapter::close();
      }
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_type> current_path() const noexcept override
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      return (_sph != nullptr) ? _sph->current_path(_leafname) : path_type();
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      OUTCOME_TRYV(adapted_handle_type::close());
      _sph.reset();
      _leafname.clear();
      return success();
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC native_handle_type release() noexcept override
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      _sph.reset();
      _leafname.clear();
      return adapted_handle_type::release();
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_handle> parent_path_handle(deadline /* unused */ = std::chrono::seconds(30)) const noexcept override
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      OUTCOME_TRY(auto &&ret, _sph->h.clone_to_path_handle());
      return {std::move(ret)};
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
    result<void> relink(const path_handle &base, path_view_type newpath, bool atomic_replace = true, deadline d = std::chrono::seconds(30)) noexcept override
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      OUTCOME_TRYV(adapted_handle_type::relink(base, newpath, atomic_replace, d));
      _sph.reset();
      _leafname.clear();
      try
      {
        auto r = detail::get_cached_path_handle(base, newpath);
        _sph = std::move(r.first);
        _leafname = std::move(r.second);
        return success();
      }
      catch(...)
      {
        return error_from_exception();
      }
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC
    result<void> unlink(deadline d = std::chrono::seconds(30)) noexcept override
    {
      LLFIO_LOG_FUNCTION_CALL(this);
      OUTCOME_TRYV(adapted_handle_type::unlink(d));
      _sph.reset();
      _leafname.clear();
      return success();
    }
  };
  /*! \brief Constructs a `T` adapted into a parent handle caching implementation.

  This function works via the `construct<T>()` free function framework for which your `handle`
  implementation must have registered its construction details.
  */
  template <class T, class... Args> inline result<cached_parent_handle_adapter<T>> cache_parent(Args &&... args) noexcept
  {
    construct<T> constructor{std::forward<Args>(args)...};
    OUTCOME_TRY(auto &&h, constructor());
    try
    {
      return cached_parent_handle_adapter<T>(std::move(h), constructor.base, constructor._path);
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

}  // namespace algorithm

//! \brief Constructor for `algorithm::::cached_parent_handle_adapter<T>`
template <class T> struct construct<algorithm::cached_parent_handle_adapter<T>>
{
  construct<T> args;
  result<algorithm::cached_parent_handle_adapter<T>> operator()() const noexcept
  {
    OUTCOME_TRY(auto &&h, args());
    try
    {
      return algorithm::cached_parent_handle_adapter<T>(std::move(h), args.base, args._path);
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
};

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../../detail/impl/cached_parent_handle_adapter.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
