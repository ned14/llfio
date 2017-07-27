/* A view of a path to something
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

#ifndef AFIO_PATH_VIEW_H
#define AFIO_PATH_VIEW_H

#include "config.hpp"

//! \file path_view.hpp Provides view of a path

AFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace detail
{
#if !_HAS_CXX17 && __cplusplus < 201700
  template <class T> constexpr size_t constexpr_strlen(const T *s) noexcept
  {
    const T *e = s;
    for(; *e; e++)
      ;
    return e - s;
  }
#endif
}

/*! \class path_view
\brief A borrowed view of a path. A lightweight trivial-type alternative to `std::filesystem::path`.

AFIO is sufficiently fast that `std::filesystem::path` as a wrapper of an underlying `std::basic_string<>`
can be problematically expensive for some filing system operations due to the potential memory allocation.
AFIO therefore works exclusively with borrowed views of other path storage.

Some of the API for `std::filesystem::path` is replicated here, however any APIs which modify the path other
than taking subsets are obviously not possible with borrowed views.

\todo Lots of member functions remain to be implemented.

# Windows specific notes:

Be aware that on Microsoft Windows, the native path storage `std::filesystem::path::value_type` is a `wchar_t`
referring to UTF-16. However much of AFIO path usage is a `path_handle` to somewhere on the filing system
plus a relative `const char *` UTF-8 path fragment as the use of absolute paths is discouraged. Rather than
complicate the ABI to handle templated path character types, on Microsoft Windows only we do the following:

- If view input is `wchar_t`, the original source is passed through unmodified to the syscall without any
memory allocation nor copying.
- If view input is `char`:
  1. The original source **is assumed to be in UTF-8**, not ASCII like most `char` paths on Microsoft Windows.
  2. Use with any kernel function converts to a temporary UTF-16 internal buffer. We use the fast NT kernel
UTF8 to UTF16 routine, not the slow Win32 routine.

Be also aware that AFIO calls the NT kernel API directly rather than the Win32 API for:

- For any paths relative to a `path_handle` (the Win32 API does not provide a race free file system API).
- For any paths beginning with `\\.\` (NT kernel namespace) or `\??\` (NT kernel symlink namespace). Note
this is NOT `\\?\` which is used to tell a Win32 API to not perform DOS to NT path conversion.

If the NT kernel API is used directly then:

- Paths are matched case sensitively as raw bytes via `memcmp()`, not case insensitively (requires slow
locale conversion).
- The path limit is 32,767 characters.

For performance, you are very strongly recommended to use the NT kernel API wherever possible. Where paths
are involved, it is often three to five times faster due to the multiple memory allocations and string
translations that the Win32 functions perform before calling the NT kernel routine.

If however you are taking input from some external piece of code, then for maximum compatibility we still use
the Win32 API.
*/
class AFIO_DECL path_view
{
public:
  //! Character type
  using value_type = char;
  //! Pointer type
  using pointer = char *;
  //! Const pointer type
  using const_pointer = const char *;
  //! Reference type
  using reference = char &;
  //! Const reference type
  using const_reference = const char &;
  // const_iterator
  // iterator
  // reverse_iterator
  // const_reverse_iterator
  //! Size type
  using size_type = std::size_t;
  //! Difference type
  using difference_type = std::ptrdiff_t;

private:
#ifdef _WIN32
  struct state
  {
    string_view _utf8;
    wstring_view _utf16;

    constexpr state()
        : _utf8()
        , _utf16()
    {
    }
    constexpr state(string_view v)
        : _utf8(v)
        , _utf16()
    {
    }
    constexpr state(wstring_view v)
        : _utf8()
        , _utf16(v)
    {
    }
    constexpr void swap(state &o) noexcept
    {
      _utf8.swap(o._utf8);
      _utf16.swap(o._utf16);
    }
  } _state;
  template <class U> constexpr auto _invoke(U &&f) const noexcept { return !_state._utf16.empty() ? f(_state._utf16) : f(_state._utf8); }
#else
  struct state
  {
    string_view _utf8;

    constexpr state()
        : _utf8()
    {
    }
    constexpr state(string_view v)
        : _utf8(v)
    {
    }
    constexpr void swap(state &o) noexcept { _utf8.swap(o._utf8); }
  } _state;
  template <class U> constexpr auto _invoke(U &&f) const noexcept { return f(_state._utf8); }
#endif
public:
  //! Constructs an empty path view
  constexpr path_view() noexcept : _state{} {}
  //! Implicitly constructs a path view from a path. The input path MUST continue to exist for this view to be valid.
  path_view(const filesystem::path &v) noexcept : _state(v.native()) {}
  //! Implicitly constructs a UTF-8 path view from a string. The input string MUST continue to exist for this view to be valid.
  path_view(const std::string &v) noexcept : _state(v) {}
  //! Implicitly constructs a UTF-8 path view from a `const char *`. The input string MUST continue to exist for this view to be valid.
  constexpr path_view(const char *v) noexcept :
#if !_HAS_CXX17 && __cplusplus < 201700
  _state(string_view(v, detail::constexpr_strlen(v)))
#else
  _state(string_view(v))
#endif
  {
  }
  //! Implicitly constructs a UTF-8 path view from a string view.
  constexpr path_view(string_view v) noexcept : _state(v) {}
#ifdef _WIN32
  //! Implicitly constructs a UTF-16 path view from a string. The input string MUST continue to exist for this view to be valid.
  path_view(const std::wstring &v) noexcept : _state(v) {}
  //! Implicitly constructs a UTF-16 path view from a `const wchar_t *`. The input string MUST continue to exist for this view to be valid.
  constexpr path_view(const wchar_t *v) noexcept :
#if !_HAS_CXX17 && __cplusplus < 201700
  _state(wstring_view(v, detail::constexpr_strlen(v)))
#else
  _state(wstring_view(v))
#endif
  {
  }
  //! Implicitly constructs a UTF-16 path view from a wide string view.
  constexpr path_view(wstring_view v) noexcept : _state(v) {}
#endif
  //! Default copy constructor
  path_view(const path_view &) = default;
  //! Default move constructor
  path_view(path_view &&o) noexcept = default;
  //! Default copy assignment
  path_view &operator=(const path_view &p) = default;
  //! Default move assignment
  path_view &operator=(path_view &&p) noexcept = default;

  //! Swap the view with another
  constexpr void swap(path_view &o) noexcept { _state.swap(o._state); }

  //! True if empty
  constexpr bool empty() const noexcept
  {
    return _invoke([](const auto &v) { return v.empty(); });
  }
  constexpr bool has_root_path() const noexcept;
  constexpr bool has_root_name() const noexcept;
  constexpr bool has_root_directory() const noexcept;
  constexpr bool has_relative_path() const noexcept;
  constexpr bool has_parent_path() const noexcept;
  constexpr bool has_filename() const noexcept;
  constexpr bool has_stem() const noexcept;
  constexpr bool has_extension() const noexcept;
  constexpr bool is_absolute() const noexcept;
  constexpr bool is_relative() const noexcept;
#ifdef _WIN32
  // True if the path view is a NT kernel path starting with `\\.\`
  constexpr bool is_ntpath() const noexcept
  {
    return _invoke([](const auto &v) {
      if(v.size() < 4)
        return false;
      const auto *d = v.data();
      if(d[0] == '\\' && d[1] == '\\' && d[2] == '.' && d[3] == '\\')
        return true;
      if(d[0] == '\\' && d[1] == '?' && d[2] == '?' && d[3] == '\\')
        return true;
      return false;
    });
  }
#endif

  constexpr path_view remove_filename() const noexcept;
  constexpr path_view root_name() const noexcept;
  constexpr path_view root_directory() const noexcept;
  constexpr path_view root_path() const noexcept;
  constexpr path_view relative_path() const noexcept;
  constexpr path_view parent_path() const noexcept;
  constexpr path_view filename() const noexcept;
  constexpr path_view stem() const noexcept;
  constexpr path_view extension() const noexcept;

  template <class CharT, class Traits = std::char_traits<CharT>, class Alloc = std::allocator<CharT>> std::basic_string<CharT, Traits, Alloc> string(const Alloc &a = Alloc()) const;
  std::string string() const;
  std::wstring wstring() const;
  std::string u8string() const;
  std::u16string u16string() const;
  std::u32string u32string() const;

  constexpr int compare(const path_view &p) const noexcept;
  constexpr int compare(string_view str) const noexcept;
  constexpr int compare(const value_type *s) const noexcept;

  // iterator begin() const;
  // iterator end() const;

  //! Instantiate from a `path_view` to get a zero terminated path suitable for feeding to the kernel
  struct AFIO_DECL c_str
  {
    //! Number of characters, excluding zero terminating char, at buffer
    uint16_t length{0};
    const filesystem::path::value_type *buffer{nullptr};

    c_str(const path_view &view) noexcept
    {
#ifdef _WIN32
      if(!view._state._utf16.empty())
      {
        length = view._state._utf16.size();
        // Is the byte just after the view a zero? If so, use directly
        if(0 == view._state._utf16.data()[length])
        {
          buffer = view._state._utf16.data();
          return;
        }
        // Otherwise use _buffer and zero terminate.
        if(length > sizeof(_buffer) - 1)
        {
          AFIO_LOG_FATAL(&view, "Attempt to send a path exceeding 64Kb to kernel");
          abort();
        }
        memcpy(_buffer, view._state._utf16.data(), length);
        _buffer[length] = 0;
        buffer = _buffer;
        return;
      }
      if(!view._state._utf8.empty())
      {
        _from_utf8(view);
        return;
      }
#else
      if(!view._state._utf8.empty())
      {
        length = view._state._utf8.size();
        // Is the byte just after the view a zero? If so, use directly
        if(0 == view._state._utf8.data()[length])
        {
          buffer = view._state._utf8.data();
          return;
        }
        // Otherwise use _buffer and zero terminate.
        if(length > sizeof(_buffer) - 1)
        {
          AFIO_LOG_FATAL(&view, "Attempt to send a path exceeding 32Kb to kernel");
          abort();
        }
        memcpy(_buffer, view._state._utf8.data(), length);
        _buffer[length] = 0;
        buffer = _buffer;
        return;
      }
#endif
      length = 0;
      _buffer[0] = 0;
      buffer = _buffer;
    }
    c_str(const c_str &) = delete;
    c_str(c_str &&) = delete;
    c_str &operator=(const c_str &) = delete;
    c_str &operator=(c_str &&) = delete;

  private:
    filesystem::path::value_type _buffer[32768];
#ifdef _WIN32
    AFIO_HEADERS_ONLY_MEMFUNC_SPEC void _from_utf8(const path_view &view) noexcept;
#endif
  };
  friend struct c_str;
};

AFIO_V2_NAMESPACE_END

#if AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/path_view.ipp"
#endif
#undef AFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
