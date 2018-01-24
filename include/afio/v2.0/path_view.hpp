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

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

AFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace detail
{
#if(!_HAS_CXX17 && __cplusplus < 201700) || (defined(__GLIBCXX__) && __GLIBCXX__ <= 20170818)  // libstdc++'s char_traits is missing constexpr
  template <class T> constexpr size_t constexpr_strlen(const T *s) noexcept
  {
    const T *e = s;
    for(; *e; e++)
    {
      ;
    }
    return e - s;
  }
#endif
}  // namespace detail

/*! \class path_view
\brief A borrowed view of a path. A lightweight trivial-type alternative to
`std::filesystem::path`.

AFIO is sufficiently fast that `std::filesystem::path` as a wrapper of an
underlying `std::basic_string<>` can be problematically expensive for some
filing system operations due to the potential memory allocation. AFIO
therefore works exclusively with borrowed views of other path storage.

Some of the API for `std::filesystem::path` is replicated here, however any
APIs which modify the path other than taking subsets are obviously not
possible with borrowed views.

\todo Lots of member functions remain to be implemented.

# Windows specific notes:

Be aware that on Microsoft Windows, the native path storage
`std::filesystem::path::value_type` is a `wchar_t` referring to UTF-16.
However much of AFIO path usage is a `path_handle` to somewhere on the filing
system plus a relative `const char *` UTF-8 path fragment as the use of
absolute paths is discouraged. Rather than complicate the ABI to handle
templated path character types, on Microsoft Windows only we do the following:

- If view input is `wchar_t`, the original source is passed through unmodified
to the syscall without any memory allocation, copying nor slash conversion.
- If view input is `char`:
  1. The original source **is assumed to be in UTF-8**, not ASCII like most
`char` paths on Microsoft Windows.
  2. Use with any kernel function converts to a temporary UTF-16 internal
buffer. We use the fast NT kernel UTF8 to UTF16 routine, not the slow Win32
routine.
  3. Any forward slashes are converted to backwards slashes.

AFIO calls the NT kernel API directly rather than the Win32 API for:

- For any paths relative to a `path_handle` (the Win32 API does not provide a
race free file system API).
- For any paths beginning with `\!!\`, we pass the path + 3 characters
directly through. This prefix is a pure AFIO extension, and will not be
recognised by other code.
- For any paths beginning with `\??\`, we pass the path + 0 characters
directly through. Note the NT kernel keeps a symlink at `\??\` which refers to
the DosDevices namespace for the current login, so as an incorrect relation
which you should **not** rely on, the Win32 path `C:\foo` probably will appear
at `\??\C:\foo`.

These prefixes are still passed to the Win32 API:

- `\\?\` which is used to tell a Win32 API that the remaining path is longer
than a DOS path.
- `\\.\` which since Windows 7 is treated exactly like `\\?\`.

If the NT kernel API is used directly then:

- Paths are matched case sensitively as raw bytes via `memcmp()`, not case
insensitively (requires slow locale conversion).
- The path limit is 32,767 characters.

If you really care about performance, you are very strongly recommended to use
the NT kernel API wherever possible. Where paths are involved, it is often
three to five times faster due to the multiple memory allocations and string
translations that the Win32 functions perform before calling the NT kernel
routine.

If however you are taking input from some external piece of code, then for
maximum compatibility you should still use the Win32 API.
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
  static constexpr auto _npos = string_view::npos;
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
  template <class U> constexpr auto _invoke(U &&f) noexcept { return !_state._utf16.empty() ? f(_state._utf16) : f(_state._utf8); }
  template <class U> constexpr auto _invoke(U &&f) const noexcept { return !_state._utf16.empty() ? f(_state._utf16) : f(_state._utf8); }
  constexpr auto _find_last_sep() const noexcept
  {
    // wchar paths must use backslashes
    if(!_state._utf16.empty())
      return _state._utf16.rfind('\\');
    // char paths can use either
    return _state._utf8.find_last_of("/\\");
  }
#else
  struct state
  {
    string_view _utf8;

    state() = default;
    constexpr explicit state(string_view v)
        : _utf8(v)
    {
    }
    constexpr void swap(state &o) noexcept { _utf8.swap(o._utf8); }
  } _state;
  template <class U> constexpr auto _invoke(U &&f) noexcept { return f(_state._utf8); }
  template <class U> constexpr auto _invoke(U &&f) const noexcept { return f(_state._utf8); }
  constexpr auto _find_last_sep() const noexcept { return _state._utf8.rfind(filesystem::path::preferred_separator); }
#endif
public:
  //! Constructs an empty path view
  path_view() = default;
  ~path_view() = default;
  //! Implicitly constructs a path view from a path. The input path MUST continue to exist for this view to be valid.
  path_view(const filesystem::path &v) noexcept : _state(v.native()) {}  // NOLINT
  //! Implicitly constructs a UTF-8 path view from a string. The input string MUST continue to exist for this view to be valid.
  path_view(const std::string &v) noexcept : _state(v) {}  // NOLINT
  //! Implicitly constructs a UTF-8 path view from a zero terminated `const char *`. The input string MUST continue to exist for this view to be valid.
  constexpr path_view(const char *v) noexcept :                                                // NOLINT
#if(!_HAS_CXX17 && __cplusplus < 201700) || (defined(__GLIBCXX__) && __GLIBCXX__ <= 20170818)  // libstdc++'s char_traits is missing constexpr
                                                 _state(string_view(v, detail::constexpr_strlen(v)))
#else
                                                 _state(string_view(v))
#endif
  {
  }
  //! Constructs a UTF-8 path view from a lengthed `const char *`. The input string MUST continue to exist for this view to be valid.
  constexpr path_view(const char *v, size_t len) noexcept : _state(string_view(v, len)) {}
  /*! Implicitly constructs a UTF-8 path view from a string view.
  \warning The byte after the end of the view must be legal to read.
  */
  constexpr path_view(string_view v) noexcept : _state(v) {}  // NOLINT
#ifdef _WIN32
  //! Implicitly constructs a UTF-16 path view from a string. The input string MUST continue to exist for this view to be valid.
  path_view(const std::wstring &v) noexcept : _state(v) {}
  //! Implicitly constructs a UTF-16 path view from a zero terminated `const wchar_t *`. The input string MUST continue to exist for this view to be valid.
  constexpr path_view(const wchar_t *v) noexcept :
#if !_HAS_CXX17 && __cplusplus < 201700
  _state(wstring_view(v, detail::constexpr_strlen(v)))
#else
  _state(wstring_view(v))
#endif
  {
  }
  //! Constructs a UTF-16 path view from a lengthed `const wchar_t *`. The input string MUST continue to exist for this view to be valid.
  constexpr path_view(const wchar_t *v, size_t len) noexcept : _state(wstring_view(v, len)) {}
  /*! Implicitly constructs a UTF-16 path view from a wide string view.
  \warning The character after the end of the view must be legal to read.
  */
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
  // True if the path view contains any of the characters `*`, `?`, (POSIX only: `[` or `]`).
  constexpr bool contains_glob() const noexcept
  {
#ifdef _WIN32
    if(!_state._utf16.empty())
      return wstring_view::npos != _state._utf16.find_first_of(L"*?");
    if(!_state._utf8.empty())
      return wstring_view::npos != _state._utf8.find_first_of("*?");
    return false;
#else
    return string_view::npos != _state._utf8.find_first_of("*?[]");
#endif
  }
#ifdef _WIN32
  // True if the path view is a NT kernel path starting with `\!!\` or `\??\`
  constexpr bool is_ntpath() const noexcept
  {
    return _invoke([](const auto &v) {
      if(v.size() < 4)
        return false;
      const auto *d = v.data();
      if(d[0] == '\\' && d[1] == '!' && d[2] == '!' && d[3] == '\\')
        return true;
      if(d[0] == '\\' && d[1] == '?' && d[2] == '?' && d[3] == '\\')
        return true;
      return false;
    });
  }
  // True if the path view matches the format of an AFIO deleted file
  constexpr bool is_afio_deleted() const noexcept
  {
    return filename()._invoke([](const auto &v) {
      if(v.size() == 64 + 8)
      {
        // Could be one of our "deleted" files, is he all hex + ".deleted"?
        for(size_t n = 0; n < 64; n++)
        {
          auto c = v[n];
          if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
        }
        return v[64] == '.' && v[65] == 'd' && v[66] == 'e' && v[67] == 'l' && v[68] == 'e' && v[69] == 't' && v[70] == 'e' && v[71] == 'd';
      }
      return false;
    });
  }
#endif

  //! Adjusts the end of this view to match the final separator.
  constexpr void remove_filename() noexcept
  {
    auto sep_idx = _find_last_sep();
    _invoke([sep_idx](auto &v) {
      if(_npos == sep_idx)
      {
        v = {};
      }
      else
      {
        v.remove_suffix(v.size() - sep_idx);
      }
    });
  }
  //! Returns the size of the view in characters.
  constexpr size_t native_size() const noexcept
  {
    return _invoke([](const auto &v) { return v.size(); });
  }
  constexpr path_view root_name() const noexcept;
  constexpr path_view root_directory() const noexcept;
  constexpr path_view root_path() const noexcept;
  constexpr path_view relative_path() const noexcept;
  constexpr path_view parent_path() const noexcept;
  //! Returns a view of the filename part of this view.
  constexpr path_view filename() const noexcept
  {
    auto sep_idx = _find_last_sep();
    if(_npos == sep_idx)
    {
      return *this;
    }
    return _invoke([sep_idx](const auto &v) { return path_view(v.data() + sep_idx + 1, v.size() - sep_idx - 1); });
  }
  constexpr path_view stem() const noexcept;
  constexpr path_view extension() const noexcept;

  //! Return the path view as a path.
  filesystem::path path() const
  {
#ifdef _WIN32
    if(!_state._utf16.empty())
      return filesystem::path(std::wstring(_state._utf16.data(), _state._utf16.size()));
#endif
    if(!_state._utf8.empty())
    {
      return filesystem::path(std::string(_state._utf8.data(), _state._utf8.size()));
    }
    return {};
  }

  /*! Compares the two string views via the view's `compare()` which in turn calls `traits::compare()`.
  Be aware that on Windows a conversion from UTF-8 to UTF-16 is performed if needed.
  */
  constexpr int compare(const path_view &p) const noexcept
  {
    return _invoke([&p](const auto &v) { return -p.compare(v); });
  }
//! \overload
#ifndef _WIN32
  constexpr
#endif
  int compare(const char *s) const noexcept
  {
    return compare(string_view(s));
  }
//! \overload
#ifndef _WIN32
  constexpr
#endif
  int compare(string_view str) const noexcept
  {
#ifdef _WIN32
    if(!_state._utf16.empty())
    {
      c_str z(path_view(str), false);
      return _state._utf16.compare(wstring_view(z.buffer, z.length));
    }
#endif
    return _state._utf8.compare(str);
  }
#ifdef _WIN32
  int compare(const wchar_t *s) const noexcept { return compare(wstring_view(s)); }
  int compare(wstring_view str) const noexcept
  {
    if(!_state._utf16.empty())
    {
      return _state._utf16.compare(str);
    }
    c_str z(path_view(*this), false);
    return -str.compare(wstring_view(z.buffer, z.length));
  }
#endif

  // iterator begin() const;
  // iterator end() const;

  //! Instantiate from a `path_view` to get a zero terminated path suitable for feeding to the kernel
  struct AFIO_DECL c_str
  {
    //! Number of characters, excluding zero terminating char, at buffer
    uint16_t length{0};
    const filesystem::path::value_type *buffer{nullptr};

#ifdef _WIN32
    c_str(const path_view &view, bool ntkernelapi) noexcept
    {
      if(!view._state._utf16.empty())
      {
        if(view._state._utf16.size() > 32768)
        {
          AFIO_LOG_FATAL(&view, "Attempt to send a path exceeding 64Kb to kernel");
          abort();
        }
        length = static_cast<uint16_t>(view._state._utf16.size());
        // Is this going straight to a NT kernel API? If so, use directly
        if(ntkernelapi)
        {
          buffer = view._state._utf16.data();
          return;
        }
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
    c_str(const path_view &view) noexcept  // NOLINT
    {
      if(!view._state._utf8.empty())
      {
        if(view._state._utf8.size() > 32768)
        {
          AFIO_LOG_FATAL(&view, "Attempt to send a path exceeding 64Kb to kernel");
          abort();
        }
        length = static_cast<uint16_t>(view._state._utf8.size());
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
    ~c_str() = default;
    c_str(const c_str &) = delete;
    c_str(c_str &&) = delete;
    c_str &operator=(const c_str &) = delete;
    c_str &operator=(c_str &&) = delete;

  private:
    filesystem::path::value_type _buffer[32768]{};
#ifdef _WIN32
    AFIO_HEADERS_ONLY_MEMFUNC_SPEC void _from_utf8(const path_view &view) noexcept;
#endif
  };
  friend struct c_str;
};
inline constexpr bool operator==(path_view x, path_view y) noexcept
{
  if(x.native_size() != y.native_size())
  {
    return false;
  }
  return x.compare(y) == 0;
}
inline constexpr bool operator!=(path_view x, path_view y) noexcept
{
  if(x.native_size() != y.native_size())
  {
    return true;
  }
  return x.compare(y) != 0;
}
inline constexpr bool operator<(path_view x, path_view y) noexcept
{
  return x.compare(y) < 0;
}
inline constexpr bool operator>(path_view x, path_view y) noexcept
{
  return x.compare(y) > 0;
}
inline constexpr bool operator<=(path_view x, path_view y) noexcept
{
  return x.compare(y) <= 0;
}
inline constexpr bool operator>=(path_view x, path_view y) noexcept
{
  return x.compare(y) >= 0;
}
inline std::ostream &operator<<(std::ostream &s, const path_view &v)
{
  return s << v.path();
}

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
