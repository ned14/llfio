/* A view of a path to something
(C) 2017-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#ifndef LLFIO_PATH_VIEW_H
#define LLFIO_PATH_VIEW_H

#include "config.hpp"

#include <iterator>
#include <memory>  // for unique_ptr

//! \file path_view.hpp Provides view of a path

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

// Can't use lambdas in constexpr in C++ 14 :(
#if __cplusplus < 201700 && defined(__GNUC__)
#define LLFIO_PATH_VIEW_CONSTEXPR
#else
#define LLFIO_PATH_VIEW_CONSTEXPR constexpr
#endif

/* GCC defines __CHAR8_TYPE__ if there is a char8_t.
clang defines __cpp_char8_t if there is a char8_t.
MSVC seems to only implement char8_t if C++ 20 is enabled.
*/
#ifndef LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED
#if(defined(_MSC_VER) && !defined(__clang__) && (!_HAS_CXX20 || _MSC_VER < 1921)) || (defined(__GNUC__) && !defined(__clang__) && !defined(__CHAR8_TYPE__)) || \
(defined(__clang__) && !defined(__cpp_char8_t))
#define LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED 1
#else
#define LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED 0
#endif
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace detail
{
  template <class T> constexpr inline size_t constexpr_strlen(const T *s) noexcept
  {
    const T *e = s;
    for(; *e; e++)
    {
    }
    return e - s;
  }
  constexpr inline size_t constexpr_strlen(const byte *s) noexcept
  {
    const byte *e = s;
    for(; *e != to_byte(0); e++)
    {
    }
    return e - s;
  }

#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED
#ifdef _MSC_VER  // MSVC's standard library refuses any basic_string_view<T> where T is not an unsigned type
  using char8_t = unsigned char;
#else
  struct char8_t
  {
    char v;
    char8_t() = default;
    constexpr char8_t(char _v) noexcept
        : v(_v)
    {
    }
    constexpr bool operator!() const noexcept { return !v; }
    constexpr explicit operator bool() const noexcept { return !!v; }
    constexpr int operator-(int x) const noexcept { return v - x; }
    constexpr int operator+(int x) const noexcept { return v + x; }
  };
  constexpr inline bool operator<(char8_t a, char8_t b) noexcept { return a.v < b.v; }
  constexpr inline bool operator>(char8_t a, char8_t b) noexcept { return a.v > b.v; }
  constexpr inline bool operator<=(char8_t a, char8_t b) noexcept { return a.v <= b.v; }
  constexpr inline bool operator>=(char8_t a, char8_t b) noexcept { return a.v >= b.v; }
  constexpr inline bool operator==(char8_t a, char8_t b) noexcept { return a.v == b.v; }
  constexpr inline bool operator!=(char8_t a, char8_t b) noexcept { return a.v != b.v; }
#endif
#endif
#if !defined(__CHAR16_TYPE__) && !defined(_MSC_VER)  // VS2015 onwards has built in char16_t
  enum class char16_t : unsigned short
  {
  };
#endif

  template <class T> struct is_source_chartype_acceptable : std::false_type
  {
  };
  template <> struct is_source_chartype_acceptable<char> : std::true_type
  {
  };
  template <> struct is_source_chartype_acceptable<wchar_t> : std::true_type
  {
  };
  template <> struct is_source_chartype_acceptable<char8_t> : std::true_type
  {
  };
  template <> struct is_source_chartype_acceptable<char16_t> : std::true_type
  {
  };

  template <class T> struct is_source_acceptable : is_source_chartype_acceptable<T>
  {
  };
  template <> struct is_source_acceptable<LLFIO_V2_NAMESPACE::byte> : std::true_type
  {
  };

  LLFIO_HEADERS_ONLY_FUNC_SPEC char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length,
                                                      const LLFIO_V2_NAMESPACE::byte *src_buffer, size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const char *src_buffer,
                                                      size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const wchar_t *src_buffer,
                                                      size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const char8_t *src_buffer,
                                                      size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const char16_t *src_buffer,
                                                      size_t src_buffer_length, const std::locale *loc);

  LLFIO_HEADERS_ONLY_FUNC_SPEC wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length,
                                                         const LLFIO_V2_NAMESPACE::byte *src_buffer, size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const char *src_buffer,
                                                         size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const wchar_t *src_buffer,
                                                         size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const char8_t *src_buffer,
                                                         size_t src_buffer_length, const std::locale *loc);
  LLFIO_HEADERS_ONLY_FUNC_SPEC wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const char16_t *src_buffer,
                                                         size_t src_buffer_length, const std::locale *loc);
  class path_view_iterator;

  LLFIO_TEMPLATE(class T, class U)
  LLFIO_TREQUIRES(LLFIO_TEXPR(std::declval<U>()((T *) nullptr))) constexpr inline U is_deleter(U &&v) { return v; }
  template <class T> constexpr inline void is_deleter(...) {}
  LLFIO_TEMPLATE(class U)
  LLFIO_TREQUIRES(LLFIO_TEXPR(std::declval<U>().deallocate((typename U::value_type *) nullptr, (size_t) 0))) constexpr inline U is_allocator(U &&v)
  {
    return v;
  }
  constexpr inline void is_allocator(...) {}
  static_assert(!std::is_void<decltype(is_deleter<int>(std::declval<std::default_delete<int>>()))>::value,
                "std::default_delete<T> is not detected as a deleter!");
  static_assert(!std::is_void<decltype(is_allocator(std::declval<std::allocator<int>>()))>::value, "std::allocator<T> is not detected as an allocator!");
}  // namespace detail

class path_view;
class path_view_component;
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator==(path_view_component x, path_view_component y) noexcept;
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator!=(path_view_component x, path_view_component y) noexcept;
template <class F> inline LLFIO_PATH_VIEW_CONSTEXPR auto visit(path_view_component view, F &&f);
template <class F> inline LLFIO_PATH_VIEW_CONSTEXPR auto visit(F &&f, path_view_component view);
inline std::ostream &operator<<(std::ostream &s, const path_view_component &v);
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator==(path_view x, path_view y) noexcept;
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator!=(path_view x, path_view y) noexcept;

/*! \class path_view_component
\brief An iterated part of a `path_view`.
*/
class LLFIO_DECL path_view_component
{
  friend class path_view;
  friend class detail::path_view_iterator;
  friend inline LLFIO_PATH_VIEW_CONSTEXPR bool LLFIO_V2_NAMESPACE::operator==(path_view_component x, path_view_component y) noexcept;
  friend inline LLFIO_PATH_VIEW_CONSTEXPR bool LLFIO_V2_NAMESPACE::operator!=(path_view_component x, path_view_component y) noexcept;
  template <class F> friend inline LLFIO_PATH_VIEW_CONSTEXPR auto LLFIO_V2_NAMESPACE::visit(path_view_component view, F &&f);
  template <class F> friend inline LLFIO_PATH_VIEW_CONSTEXPR auto LLFIO_V2_NAMESPACE::visit(F &&f, path_view_component view);
  friend inline std::ostream &LLFIO_V2_NAMESPACE::operator<<(std::ostream &s, const path_view_component &v);

public:
  //! The size type
  using size_type = filesystem::path::string_type::size_type;
  //! The preferred separator type
  static constexpr auto preferred_separator = filesystem::path::preferred_separator;

  //! Character type for passthrough input
  using byte = LLFIO_V2_NAMESPACE::byte;
#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED
  using char8_t = detail::char8_t;
#endif
#if !defined(__CHAR16_TYPE__) && !defined(_MSC_VER)  // VS2015 onwards has built in char16_t
  using char16_t = detail::char16_t;
#endif

  //! True if path views can be constructed from this character type.
  //! i.e. is one of `char`, `wchar_t`, `char8_t`, `char16_t`
  template <class Char> static constexpr bool is_source_chartype_acceptable = detail::is_source_chartype_acceptable<Char>::value;

  //! True if path views can be constructed from this source.
  //! i.e. `is_source_chartype_acceptable`, or is `byte`
  template <class Char> static constexpr bool is_source_acceptable = detail::is_source_acceptable<Char>::value;

  //! The default internal buffer size used by `rendered_path`.
  static constexpr size_t default_internal_buffer_size = 1024;  // 2Kb for wchar_t, 1Kb for char

  //! How to interpret separators
  enum format : uint8_t
  {
    unknown,
    native_format,   //!< Separate at the native path separator only.
    generic_format,  //!< Separate at the generic path separator only.
    auto_format,     //!< Separate at both the native and generic path separators.
    binary_format    //!< Do not separate at any path separator.
  };

  //! The zero termination to use
  enum zero_termination
  {
    zero_terminated,      //!< The input is zero terminated, or requested output ought to be zero terminated.
    not_zero_terminated,  //!< The input is not zero terminated, or requested output ought to not be zero terminated.
  };

  //! The default deleter to use
  template <class T> using default_rendered_path_deleter = std::default_delete<T>;

private:
  static constexpr auto _npos = string_view::npos;
  union
  {
    const byte *_bytestr{nullptr};
    const char *_charstr;
    const wchar_t *_wcharstr;
    const char8_t *_char8str;
    const char16_t *_char16str;
  };
  size_t _length{0};  // in characters, excluding any zero terminator
  uint16_t _zero_terminated : 1;
  uint16_t _passthrough : 1;
  uint16_t _char : 1;
  uint16_t _wchar : 1;
  uint16_t _utf8 : 1;
  uint16_t _utf16 : 1;
  uint16_t _reserved1 : 10;
  format _format{format::unknown};
  uint8_t _reserved2{0};

public:
  //! Constructs an empty path view component (DEVIATES from P1030, is not trivial due to C++ 14 compatibility)
  constexpr path_view_component() noexcept
      : _zero_terminated(false)
      , _passthrough(false)
      , _char(false)
      , _wchar(false)
      , _utf8(false)
      , _utf16(false)
      , _reserved1(0)
  {
  }  // NOLINT
  //! Constructs a path view component identical to the input, except with different format interpretation.
  constexpr path_view_component(path_view_component p, format fmt) noexcept
      : _charstr(p._charstr)
      , _length(p._length)
      , _zero_terminated(p._zero_terminated)
      , _passthrough(p._passthrough)
      , _char(p._char)
      , _wchar(p._wchar)
      , _utf8(p._utf8)
      , _utf16(p._utf16)
      , _reserved1(p._reserved1)
      , _format(fmt)
  {
  }
  /*! Implicitly constructs a path view from a path. The input path MUST continue to exist for this view to be valid
  (DEVIATES from P1030 due to filesystem::path not exposing its path formatting).
  */
  path_view_component(const filesystem::path &v, format fmt = auto_format) noexcept  // NOLINT
      : path_view_component(v.native().c_str(), v.native().size(), zero_terminated, fmt)
  {
  }
  /*! Constructs from a basic string if the character type is one of
  `char`, `wchar_t`, `char8_t` or `char16_t`.
  */
  LLFIO_TEMPLATE(class Char)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<Char>))
  constexpr path_view_component(const std::basic_string<Char> &v, format fmt = binary_format) noexcept  // NOLINT
      : path_view_component(v.data(), v.size(), zero_terminated, fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view_component(const char *b, size_t l, enum zero_termination zt, format fmt = binary_format) noexcept
      : _charstr((l == 0) ? nullptr : b)
      , _length((l == 0) ? 0 : l)
      , _zero_terminated((l == 0) ? false : (zt == zero_terminated))
      , _passthrough(false)
      , _char((l == 0) ? false : true)
      , _wchar(false)
      , _utf8(false)
      , _utf16(false)
      , _reserved1(0)
      , _format(fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view_component(const wchar_t *b, size_t l, enum zero_termination zt, format fmt = binary_format) noexcept
      : _wcharstr((l == 0) ? nullptr : b)
      , _length((l == 0) ? 0 : l)
      , _zero_terminated((l == 0) ? false : (zt == zero_terminated))
      , _passthrough(false)
      , _char(false)
      , _wchar((l == 0) ? false : true)
      , _utf8(false)
      , _utf16(false)
      , _reserved1(0)
      , _format(fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view_component(const char8_t *b, size_t l, enum zero_termination zt, format fmt = binary_format) noexcept
      : _char8str((l == 0) ? nullptr : b)
      , _length((l == 0) ? 0 : l)
      , _zero_terminated((l == 0) ? false : (zt == zero_terminated))
      , _passthrough(false)
      , _char(false)
      , _wchar(false)
      , _utf8((l == 0) ? false : true)
      , _utf16(false)
      , _reserved1(0)
      , _format(fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view_component(const char16_t *b, size_t l, enum zero_termination zt, format fmt = binary_format) noexcept
      : _char16str((l == 0) ? nullptr : b)
      , _length((l == 0) ? 0 : l)
      , _zero_terminated((l == 0) ? false : (zt == zero_terminated))
      , _passthrough(false)
      , _char(false)
      , _wchar(false)
      , _utf8(false)
      , _utf16((l == 0) ? false : true)
      , _reserved1(0)
      , _format(fmt)
  {
  }
  /*! Constructs from a lengthed array of `byte`. The input
  array MUST continue to exist for this view to be valid.
  */
  constexpr path_view_component(const byte *b, size_t l, enum zero_termination zt) noexcept
      : _bytestr((l == 0) ? nullptr : b)
      , _length((l == 0) ? 0 : l)
      , _zero_terminated((l == 0) ? false : (zt == zero_terminated))
      , _passthrough((l == 0) ? false : true)
      , _char(false)
      , _wchar(false)
      , _utf8(false)
      , _utf16(false)
      , _reserved1(0)
      , _format(binary_format)
  {
  }

  /*! Implicitly constructs a path view from a zero terminated pointer to a
  character array, which must be one of `char`, `wchar_t`, `char8_t` or `char16_t`.
  The input string MUST continue to exist for this view to be valid.
  */
  LLFIO_TEMPLATE(class Char)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<Char>))
  constexpr path_view_component(const Char *s, format fmt = binary_format) noexcept
      : path_view_component(s, detail::constexpr_strlen(s), zero_terminated, fmt)
  {
  }
  /*! Implicitly constructs a path view from a zero terminated pointer to byte array.
  The input array MUST continue to exist for this view to be valid.
  */
  constexpr path_view_component(const byte *s) noexcept
      : path_view_component(s, detail::constexpr_strlen(s), zero_terminated)
  {
  }

  /*! Constructs from a basic string view if the character type is one of
  `char`, `wchar_t`, `char8_t` or `char16_t`.
  */
  LLFIO_TEMPLATE(class Char)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<Char>))
  constexpr path_view_component(basic_string_view<Char> v, enum zero_termination zt, format fmt = binary_format) noexcept
      : path_view_component(v.data(), v.size(), zt, fmt)
  {
  }
  /*! Constructs from a `span<const byte>`.
   */
  constexpr path_view_component(span<const byte> v, enum zero_termination zt) noexcept
      : path_view_component(v.data(), v.size(), zt)
  {
  }

  /*! Constructs from an iterator sequence if the iterator is contiguous, if its
  value type is one of `char`, `wchar_t`, `char8_t` or `char16_t`.

  (DEVIATES from P1030, cannot detect contiguous iterator in SFINAE in C++ 14)
  */
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<typename It::value_type>), LLFIO_TPRED(is_source_chartype_acceptable<typename End::value_type>))
  constexpr path_view_component(It b, End e, enum zero_termination zt, format fmt = binary_format) noexcept
      : path_view_component(addressof(*b), e - b, zt, fmt)
  {
  }
  //! \overload
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<std::decay_t<It>>), LLFIO_TPRED(is_source_chartype_acceptable<std::decay_t<End>>))
  constexpr path_view_component(It *b, End *e, enum zero_termination zt, format fmt = binary_format) noexcept
      : path_view_component(b, e - b, zt, fmt)
  {
  }
  /*! Constructs from an iterator sequence if the iterator is contiguous, if its
  value type is `byte`.

  (DEVIATES from P1030, cannot detect contiguous iterator in SFINAE in C++ 14)
  */
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(std::is_same<typename It::value_type, byte>::value), LLFIO_TPRED(std::is_same<typename End::value_type, byte>::value))
  constexpr path_view_component(It b, End e, enum zero_termination zt) noexcept
      : path_view_component(addressof(*b), e - b, zt, binary_format)
  {
  }
  //! \overload
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(std::is_same<std::decay_t<It>, byte>::value), LLFIO_TPRED(std::is_same<std::decay_t<End>, byte>::value))
  constexpr path_view_component(It *b, End *e, enum zero_termination zt) noexcept
      : path_view_component(b, e - b, zt, binary_format)
  {
  }

private:
  template <class U> constexpr auto _invoke(U &&f) const noexcept
  {
    using LLFIO_V2_NAMESPACE::basic_string_view;
    return _utf8 ? f(basic_string_view<char8_t>(_char8str, _length))  //
                   :
                   (_utf16 ? f(basic_string_view<char16_t>(_char16str, _length))  //
                             :
                             (_wchar ? f(basic_string_view<wchar_t>(_wcharstr, _length))  //
                                       :
                                       f(basic_string_view<char>((const char *) _bytestr, _length))));
  }
  constexpr auto _find_first_sep(size_t startidx = 0) const noexcept
  {
    using LLFIO_V2_NAMESPACE::basic_string_view;
    switch(_format)
    {
    case format::binary_format:
      return _npos;
    default:
#ifdef _WIN32
    case format::auto_format:
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).find_first_of((const char8_t *) "/\\", startidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).find_first_of((const char16_t *) L"/\\", startidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).find_first_of(L"/\\", startidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).find_first_of((const char *) "/\\", startidx)));
    case format::native_format:
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).find(preferred_separator, startidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).find(preferred_separator, startidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).find(preferred_separator, startidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).find(preferred_separator, startidx)));
    case format::generic_format:
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).find('/', startidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).find('/', startidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).find('/', startidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).find('/', startidx)));
#else
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).find(preferred_separator, startidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).find(preferred_separator, startidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).find(preferred_separator, startidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).find(preferred_separator, startidx)));
#endif
    }
  }
  constexpr auto _find_last_sep(size_t endidx = _npos) const noexcept
  {
    using LLFIO_V2_NAMESPACE::basic_string_view;
    switch(_format)
    {
    case format::binary_format:
      return _npos;
    default:
#ifdef _WIN32
    case format::auto_format:
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).find_last_of((const char8_t *) "/\\", endidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).find_last_of((const char16_t *) L"/\\", endidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).find_last_of(L"/\\", endidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).find_last_of("/\\", endidx)));
    case format::native_format:
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).rfind(preferred_separator, endidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).rfind(preferred_separator, endidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).rfind(preferred_separator, endidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).rfind(preferred_separator, endidx)));
    case format::generic_format:
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).rfind('/', endidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).rfind('/', endidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).rfind('/', endidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).rfind('/', endidx)));
#else
      return _utf8 ? basic_string_view<char8_t>(_char8str, _length).rfind(preferred_separator, endidx)  //
                     :
                     (_utf16 ? basic_string_view<char16_t>(_char16str, _length).rfind(preferred_separator, endidx)  //
                               :
                               (_wchar ? basic_string_view<wchar_t>(_wcharstr, _length).rfind(preferred_separator, endidx)  //
                                         :
                                         basic_string_view<char>((const char *) _bytestr, _length).rfind(preferred_separator, endidx)));
#endif
    }
  }
  LLFIO_PATH_VIEW_CONSTEXPR path_view_component _filename() const noexcept
  {
    auto sep_idx = _find_last_sep();
#if LLFIO_USING_EXPERIMENTAL_FILESYSTEM
#ifdef _WIN32
    if(sep_idx == 2 && _length == 3)
    {
      return this->_invoke([this, sep_idx](const auto &v) mutable {
        if(v[1] == ':')
        {
          return path_view_component(v.data() + 2, 1, zero_termination(), formatting());
        }
        return *this;
      });
    }
#endif
    if(_npos == sep_idx || (_length == 1 && sep_idx == 0))  // Filesystem TS thinks the filename() of "/" is "/"
    {
      return *this;
    }
#else
    if(_npos == sep_idx)
    {
      return *this;
    }
#endif
    return _invoke([sep_idx, this](const auto &v) { return path_view_component(v.data() + sep_idx + 1, v.size() - sep_idx - 1, zero_termination()); });
  }

public:
  path_view_component(const path_view_component &) = default;
  path_view_component(path_view_component &&) = default;
  path_view_component &operator=(const path_view_component &) = default;
  path_view_component &operator=(path_view_component &&) = default;
  ~path_view_component() = default;

  const byte *_raw_data() const noexcept { return _bytestr; }

  //! Swap the view with another
  constexpr void swap(path_view_component &o) noexcept
  {
    path_view_component x = *this;
    *this = o;
    o = x;
  }

  //! True if empty
  LLFIO_NODISCARD constexpr bool empty() const noexcept { return _length == 0; }

  //! Returns the size of the view in characters.
  LLFIO_PATH_VIEW_CONSTEXPR size_t native_size() const noexcept
  {
    return _invoke([](const auto &v) { return v.size(); });
  }

  //! How path separators shall be interpreted
  constexpr format formatting() const noexcept { return _format; }

  //! True if input is declared to be zero terminated
  constexpr bool has_zero_termination() const noexcept { return _zero_terminated; }
  //! The zero termination during construction
  constexpr enum zero_termination zero_termination() const noexcept { return _zero_terminated ? zero_terminated : not_zero_terminated; }

  //! True if `stem()` returns a non-empty path.
  LLFIO_PATH_VIEW_CONSTEXPR bool has_stem() const noexcept { return !stem().empty(); }
  //! True if `extension()` returns a non-empty path.
  LLFIO_PATH_VIEW_CONSTEXPR bool has_extension() const noexcept { return !extension().empty(); }

  //! True if the view contains any of the characters `*`, `?`, (POSIX only: `[` or `]`).
  LLFIO_PATH_VIEW_CONSTEXPR bool contains_glob() const noexcept
  {
    return _invoke([](const auto &v) {
      using value_type = typename std::remove_reference<decltype(*v.data())>::type;
#ifdef _WIN32
      const value_type *tofind = (sizeof(value_type) > 1) ? (const value_type *) L"*?" : (const value_type *) "*?";
#else
      const value_type *tofind = (sizeof(value_type) > 1) ? (const value_type *) L"*?[]" : (const value_type *) "*?[]";
#endif
      return string_view::npos != v.find_first_of(tofind);
    });
  }

  //! Returns a view of the filename without any file extension
  LLFIO_PATH_VIEW_CONSTEXPR path_view_component stem() const noexcept
  {
    auto self = _filename();
    return self._invoke([self](const auto &v) {
      auto dot_idx = v.rfind('.');
      if(_npos == dot_idx || dot_idx == 0 || (dot_idx == 1 && v[dot_idx - 1] == '.'))
      {
        return self;
      }
      return path_view_component(v.data(), dot_idx, not_zero_terminated);
    });
  }
  //! Returns a view of the file extension part of this view
  LLFIO_PATH_VIEW_CONSTEXPR path_view_component extension() const noexcept
  {
    auto self = _filename();
    return self._invoke([this](const auto &v) {
      auto dot_idx = v.rfind('.');
      if(_npos == dot_idx || dot_idx == 0 || (dot_idx == 1 && v[dot_idx - 1] == '.'))
      {
        return path_view_component();
      }
      return path_view_component(v.data() + dot_idx, v.size() - dot_idx, zero_termination());
    });
  }

private:
#ifdef LLFIO_USING_STD_FILESYSTEM
  template <class CharT> static filesystem::path _path_from_char_array(basic_string_view<CharT> v, filesystem::path::format f)
  {
    return {v.data(), v.data() + v.size(), f};
  }
  static filesystem::path _path_from_char_array(basic_string_view<char8_t> v, filesystem::path::format f)
  {
#if(__cplusplus >= 202000 || _HAS_CXX20) && (!defined(_LIBCPP_VERSION) || _LIBCPP_VERSION >= 12000 /* approx end of 2020 */)
    return filesystem::path(v, f);
#else
    if(f != filesystem::path::format::auto_format)
    {
      throw std::runtime_error("UTF-8 path conversion pre-C++20 cannot handle non-auto_format formatting.");
    }
    return filesystem::u8path((const char *) v.data(), (const char *) v.data() + v.size());
#endif
  }
#endif
#ifdef LLFIO_USING_EXPERIMENTAL_FILESYSTEM
  template <class CharT> static filesystem::path _path_from_char_array(basic_string_view<CharT> v) { return {v.data(), v.data() + v.size()}; }
  static filesystem::path _path_from_char_array(basic_string_view<char8_t> v)
  {
#if(__cplusplus >= 202000 || _HAS_CXX20) && (!defined(_LIBCPP_VERSION) || _LIBCPP_VERSION > 10000 /* approx start of 2020 */)
    return filesystem::path(v);
#else
    return filesystem::u8path((const char *) v.data(), (const char *) v.data() + v.size());
#endif
  }
#endif

  template <class CharT> static int _do_compare(const CharT *a, const CharT *b, size_t length) noexcept { return memcmp(a, b, length * sizeof(CharT)); }
  static int _do_compare(const char8_t *_a, const char8_t *_b, size_t length) noexcept
  {
#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED
    basic_string_view<char> a((const char *) _a, length);
    basic_string_view<char> b((const char *) _b, length);
#else
    basic_string_view<char8_t> a(_a, length);
    basic_string_view<char8_t> b(_b, length);
#endif
    return a.compare(b);
  }
  static int _do_compare(const char16_t *_a, const char16_t *_b, size_t length) noexcept
  {
    basic_string_view<char16_t> a(_a, length);
    basic_string_view<char16_t> b(_b, length);
    return a.compare(b);
  }
  // Identical source encodings compare lexiographically
  template <class DestT, class Deleter, size_t _internal_buffer_size, class CharT>
  static int _compare(basic_string_view<CharT> a, enum zero_termination /*unused*/, basic_string_view<CharT> b, enum zero_termination /*unused*/,
                      const std::locale * /*unused*/) noexcept
  {
    return a.compare(b);
  }
  // Disparate source encodings compare via rendered_path
  template <class DestT, class Deleter, size_t _internal_buffer_size, class Char1T, class Char2T>
  static int _compare(basic_string_view<Char1T> a, enum zero_termination a_zt, basic_string_view<Char2T> b, enum zero_termination b_zt,
                      const std::locale *loc) noexcept
  {
    path_view_component _a_(a.data(), a.size(), a_zt);
    path_view_component _b_(b.data(), b.size(), b_zt);
    rendered_path<zero_termination::not_zero_terminated, DestT, Deleter, _internal_buffer_size> _a(_a_, loc);
    rendered_path<zero_termination::not_zero_terminated, DestT, Deleter, _internal_buffer_size> _b(_b_, loc);
    if(_a.size() < _b.size())
    {
      return -1;
    }
    if(_a.size() > _b.size())
    {
      return 1;
    }
    return _do_compare(_a.data(), _b.data(), _a.size());
  }

public:
  //! Return the path view as a path. Allocates and copies memory!
  filesystem::path path() const
  {
    return _invoke([&](const auto &v) {
#ifdef LLFIO_USING_STD_FILESYSTEM
      return _path_from_char_array(v, [](format f) -> filesystem::path::format {
        switch(f)
        {
        case format::generic_format:
          return filesystem::path::format::generic_format;
        case format::native_format:
          return filesystem::path::format::native_format;
        default:
          return filesystem::path::format::auto_format;
        }
      }(formatting()));
#endif
#ifdef LLFIO_USING_EXPERIMENTAL_FILESYSTEM
      if(formatting() == generic_format || formatting() == native_format)
      {
        throw std::runtime_error("Path conversion with <experimental/filesystem> cannot handle generic_format or native_format formatting.");
      }
      return _path_from_char_array(v);
#endif
    });
  }

  /*! Compares the two path views for equivalence or ordering using `T`
  as the destination encoding, if necessary.

  If the source encodings of the two path views are compatible, a
  lexicographical comparison is performed. If they are incompatible,
  either or both views are converted to the destination encoding
  using `rendered_path<T, Delete, _internal_buffer_size>`, and then a
  lexicographical comparison is performed.

  This can, for obvious reasons, be expensive. It can also throw
  exceptions, as `rendered_path` does.

  If the destination encoding is `byte`, `memcmp()` is used,
  and `rendered_path` is never invoked as the two sources are byte
  compared directly.
  */
  LLFIO_TEMPLATE(class T = typename filesystem::path::value_type, class Deleter = default_rendered_path_deleter<T[]>,
                 size_t _internal_buffer_size = default_internal_buffer_size)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_acceptable<T>))
  constexpr int compare(path_view_component p, const std::locale &loc) const
  {
    return _invoke([&](const auto &self) {
      return p._invoke(
      [&](const auto &other) { return _compare<T, Deleter, _internal_buffer_size>(self, zero_termination(), other, p.zero_termination(), &loc); });
    });
  }
  //! \overload
  LLFIO_TEMPLATE(class T = typename filesystem::path::value_type, class Deleter = default_rendered_path_deleter<T[]>,
                 size_t _internal_buffer_size = default_internal_buffer_size)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_acceptable<T>))
  constexpr int compare(path_view_component p) const
  {
    return _invoke([&](const auto &self) {
      return p._invoke(
      [&](const auto &other) { return _compare<T, Deleter, _internal_buffer_size>(self, zero_termination(), other, p.zero_termination(), nullptr); });
    });
  }

private:
  template <class T> struct _rendered_path_base_
  {
  protected:
    using _view_type = span<const T>;
    _view_type _ref;

    constexpr basic_string_view<T> _as_string_view() const { return {_ref.data(), _ref.size()}; }

  public:
    //! Type of the value type
    using value_type = T;
    //! Type of the pointer type
    using pointer = const T *;
    //! Type of the const pointer type
    using const_pointer = const T *;
    //! Type of the reference type
    using reference = const T &;
    //! Type of the const reference type
    using const_reference = const T &;
    //! Type of the iterator type
    using iterator = typename _view_type::iterator;
    //! Type of the const iterator type
    using const_iterator = typename _view_type::iterator;
    //! Type of the reverse iterator type
    using reverse_iterator = typename _view_type::reverse_iterator;
    //! Type of the const reverse iterator type
    using const_reverse_iterator = typename _view_type::reverse_iterator;
    //! Type of the size type
    using size_type = typename _view_type::size_type;
    //! Type of the difference type
    using difference_type = typename _view_type::difference_type;

    _rendered_path_base_() = default;
    _rendered_path_base_(const _rendered_path_base_ &) = default;
    _rendered_path_base_ &operator=(const _rendered_path_base_ &) = default;
    ~_rendered_path_base_() = default;

    //! Begin iteration
    constexpr iterator begin() { return _ref.begin(); }
    //! Begin iteration
    constexpr const_iterator begin() const { return _ref.begin(); }
    //! Begin iteration
    constexpr const_iterator cbegin() const { return _ref.cbegin(); }
    //! End iteration
    constexpr iterator end() { return _ref.end(); }
    //! End iteration
    constexpr const_iterator end() const { return _ref.end(); }
    //! End iteration
    constexpr const_iterator cend() const { return _ref.cend(); }
    //! Begin reverse iteration
    constexpr reverse_iterator rbegin() { return _ref.rbegin(); }
    //! Begin reverse iteration
    constexpr const_reverse_iterator rbegin() const { return _ref.rbegin(); }
    //! Begin reverse iteration
    constexpr const_reverse_iterator crbegin() const { return _ref.crbegin(); }
    //! End reverse iteration
    constexpr reverse_iterator rend() { return _ref.rend(); }
    //! End reverse iteration
    constexpr const_reverse_iterator rend() const { return _ref.rend(); }
    //! End reverse iteration
    constexpr const_reverse_iterator crend() const { return _ref.crend(); }

    //! Access
    constexpr reference operator[](size_type idx) { return _ref[idx]; }
    //! Access
    constexpr const_reference operator[](size_type idx) const { return _ref[idx]; }
    //! Access
    constexpr reference at(size_type idx) { return _ref.at(idx); }
    //! Access
    constexpr const_reference at(size_type idx) const { return _ref.at(idx); }
    //! Access
    constexpr reference front() { return _ref.front(); }
    //! Access
    constexpr const_reference front() const { return _ref.front(); }
    //! Access
    constexpr reference back() { return _ref.back(); }
    //! Access
    constexpr const_reference back() const { return _ref.back(); }
    //! Access
    constexpr pointer data() { return _ref.data(); }
    //! Access
    constexpr const_pointer data() const { return _ref.data(); }
    //! Size
    constexpr size_type size() const { return _ref.size(); }
    //! Size
    constexpr size_type length() const { return _ref.length(); }
    //! Max size
    constexpr size_type max_size() const { return _ref.max_size(); }
    //! Empty
    QUICKCPPLIB_NODISCARD constexpr bool empty() const { return _ref.empty(); }
    //! As span
    constexpr _view_type as_span() const { return _ref; }
  };
  template <enum path_view_component::zero_termination ZeroTermination, class T, bool = false> struct _rendered_path_base : public _rendered_path_base_<T>
  {
    //! As string view
    constexpr basic_string_view<T> as_string_view() const { return {this->_ref.data(), this->_ref.size()}; }
  };
  template <class T> struct _rendered_path_base<zero_terminated, T> : public _rendered_path_base_<T>
  {
    //! Return a zero-terminated character array suitable for use as a C string
    constexpr const T *c_str() const noexcept { return this->_ref.data(); }
  };
  template <bool _> struct _rendered_path_base<zero_terminated, byte, _> : public _rendered_path_base_<byte>
  {
  };
  template <bool _> struct _rendered_path_base<not_zero_terminated, byte, _> : public _rendered_path_base_<byte>
  {
  };

public:
  /*! Instantiate from a `path_view_component` to get a path suitable for feeding to other code.
  \tparam T The destination encoding required.
  \tparam Deleter A custom deleter OR STL allocator for any temporary buffer.
  \tparam _internal_buffer_size Override the size of the internal temporary buffer, thus
  reducing stack space consumption (most compilers optimise away the internal temporary buffer
  if it can be proved it will never be used). The default is 1024 values of `T`.

  This makes the input to the path view component into a destination format suitable for
  consumption by other code. If the source has the same format as the destination, and
  the zero termination requirements are the same, the source is used directly without
  memory copying nor reencoding.

  If the format is compatible, but the destination requires zero termination,
  and the source is not zero terminated, a straight memory copy is performed
  into the temporary buffer.

  `rendered_path` contains a temporary buffer sized according to the template parameter. Output
  below that amount involves no dynamic memory allocation. Output above that amount calls
  `operator new[]`. You can use an externally supplied larger temporary buffer to avoid
  dynamic memory allocation in all situations.
  */
  LLFIO_TEMPLATE(enum path_view_component::zero_termination ZeroTermination, class T = typename filesystem::path::value_type,
                 class AllocatorOrDeleter = default_rendered_path_deleter<T[]>, size_t _internal_buffer_size = default_internal_buffer_size)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_acceptable<T>))
  class rendered_path : public _rendered_path_base<ZeroTermination, T>
  {
    using _base = _rendered_path_base<ZeroTermination, T>;
    static_assert(is_source_acceptable<T>, "path_view_component::rendered_path<T> does not have a T which is one of byte, char, wchar_t, char8_t nor char16_t");
    template <class DestT, class _Deleter, size_t _internal_buffer_size_, class Char1T, class Char2T>
    friend int path_view_component::_compare(basic_string_view<Char1T> a, enum zero_termination a_zt, basic_string_view<Char2T> b, enum zero_termination b_zt,
                                             const std::locale *loc) noexcept;

  public:
    using value_type = typename _base::value_type;
    //! Type of the allocator, or `void` if that was not configured
    using allocator_type = decltype(detail::is_allocator(std::declval<AllocatorOrDeleter>()));
    //! Type of the deleter, or `void` if that was not configured
    using deleter_type = decltype(detail::is_deleter<value_type>(std::declval<AllocatorOrDeleter>()));

  private:
    template <class X = void> static constexpr bool _is_deleter_based = std::is_void<allocator_type>::value;
    template <class X = void> static constexpr bool _is_allocator_based = std::is_void<deleter_type>::value;
    static_assert(_is_allocator_based<> + _is_deleter_based<> == 1, "AllocatorOrDeleter must be either a callable deleter, or a STL allocator, for value_type");

    LLFIO_TEMPLATE(class U, class source_type)
    LLFIO_TREQUIRES(LLFIO_TPRED(!std::is_same<source_type, value_type>::value))
    void _make_passthrough(path_view_component /*unused*/, U & /*unused*/, const source_type * /*unused*/)
    {
      LLFIO_LOG_FATAL(nullptr, "Passthrough to non-identity type ought to never be called!");
      abort();
    }
    template <class U> void _make_passthrough(path_view_component view, U &allocate, const value_type *source)
    {
      using LLFIO_V2_NAMESPACE::basic_string_view;
      // If the consuming API is a NT kernel API, and we have / in the path, we shall need to do slash conversion
      const bool needs_slash_translation = (filesystem::path::preferred_separator != '/') &&
                                           (view.formatting() == format::auto_format || view.formatting() == format::generic_format) &&
                                           view._invoke([](auto sv) { return sv.find('/') != _npos; });
      if(!needs_slash_translation && (this->zero_termination() == not_zero_terminated || view._zero_terminated))
      {
        _base::_ref = typename _base::_view_type(source, view._length);
      }
      else
      {
        // Source must be not zero terminated, and zero terminated is required, or slash conversion is required
        const size_t required_length = view._length + view._zero_terminated;
        const size_t required_bytes = required_length * sizeof(value_type);
        const size_t _buffer_bytes = sizeof(_buffer);
#ifdef _WIN32
        if(required_bytes > 65535)
        {
          LLFIO_LOG_FATAL(nullptr, "Paths exceeding 64Kb are impossible on Microsoft Windows");
          abort();
        }
#endif
        if(required_bytes < _buffer_bytes)
        {
          // Use the internal buffer
          memcpy(_buffer, source, required_bytes);
          _buffer[required_length] = 0;
          _base::_ref = typename _base::_view_type(_buffer, view._length);
        }
        else
        {
          auto *buffer_ = allocate(required_length);
          if(nullptr != buffer_)
          {
            _bytes_to_delete = required_bytes;
            memcpy(buffer_, source, required_bytes);
            buffer_[required_length] = 0;
            _base::_ref = typename _base::_view_type(_buffer, view._length);
          }
        }
        if(needs_slash_translation)
        {
          auto sv = _base::_as_string_view();
          auto idx = sv.find('/');
          while(idx != _npos)
          {
            const_cast<value_type *>(sv.data())[idx] = filesystem::path::preferred_separator;
            idx = sv.find('/', idx);
          }
        }
      }
    }

  public:
    constexpr rendered_path() {}
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)  // conditional expression is constant
#endif
  private:
    template <class U> void _init(path_view_component view, const std::locale *loc, U &&allocate)
    {
      if(0 == view._length)
      {
        _buffer[0] = 0;
        _base::_ref = typename _base::_view_type(_buffer, size_t(0));
        return;
      }
      if(std::is_same<T, byte>::value || view._passthrough)
      {
        _base::_ref = typename _base::_view_type((value_type *) view._bytestr, view._length);
        return;
      }
      if(std::is_same<T, char>::value && view._char)
      {
        _make_passthrough(view, allocate, view._charstr);
        return;
      }
      if(std::is_same<T, wchar_t>::value && view._wchar)
      {
        _make_passthrough(view, allocate, view._wcharstr);
        return;
      }
      if(std::is_same<T, char8_t>::value && view._utf8)
      {
        _make_passthrough(view, allocate, view._char8str);
        return;
      }
      if(std::is_same<T, char16_t>::value && view._utf16)
      {
        _make_passthrough(view, allocate, view._char16str);
        return;
      }
#ifdef _WIN32
      // On Windows, consider char16_t input equivalent to wchar_t
      if(std::is_same<T, wchar_t>::value && view._utf16)
      {
        _make_passthrough(view, allocate, view._wcharstr);
        return;
      }
#else
      // On POSIX, consider char8_t input equivalent to char
      if(std::is_same<T, char>::value && view._utf8)
      {
        _make_passthrough(view, allocate, view._charstr);
        return;
      }
#endif
      // A reencode is required. We keep this out of header because reencoding
      // requires dragging in lots of system header files.
      size_t required_length = 0;
      value_type *end = nullptr;
      if(view._passthrough)
      {
        end = detail::reencode_path_to(required_length, _buffer, _internal_buffer_size, view._bytestr, view._length, loc);
      }
      else if(view._char)
      {
        end = detail::reencode_path_to(required_length, _buffer, _internal_buffer_size, view._charstr, view._length, loc);
      }
      else if(view._wchar)
      {
        end = detail::reencode_path_to(required_length, _buffer, _internal_buffer_size, view._wcharstr, view._length, loc);
      }
      else if(view._utf8)
      {
        end = detail::reencode_path_to(required_length, _buffer, _internal_buffer_size, view._char8str, view._length, loc);
      }
      else if(view._utf16)
      {
        end = detail::reencode_path_to(required_length, _buffer, _internal_buffer_size, view._char16str, view._length, loc);
      }
      else
      {
        LLFIO_LOG_FATAL(nullptr, "path_view_component::cstr somehow sees no state!");
        abort();
      }
      if(0 == required_length)
      {
        // The internal buffer was sufficient.
        _base::_ref = typename _base::_view_type(_buffer, end - _buffer);
        return;
      }
      // The internal buffer is too small. Fall back to dynamic allocation. This may throw.
      auto *allocated_buffer = allocate(required_length);
      if(nullptr == allocated_buffer)
      {
        return;
      }
      _bytes_to_delete = required_length * sizeof(value_type);
      memcpy(allocated_buffer, _buffer, end - _buffer);
      required_length -= (end - _buffer);
      end = allocated_buffer + (end - _buffer);
      if(view._passthrough)
      {
        end = detail::reencode_path_to(view._length, end, required_length, view._bytestr, view._length, loc);
      }
      else if(view._char)
      {
        end = detail::reencode_path_to(view._length, end, required_length, view._charstr, view._length, loc);
      }
      else if(view._wchar)
      {
        end = detail::reencode_path_to(view._length, end, required_length, view._wcharstr, view._length, loc);
      }
      else if(view._utf8)
      {
        end = detail::reencode_path_to(view._length, end, required_length, view._char8str, view._length, loc);
      }
      else if(view._utf16)
      {
        end = detail::reencode_path_to(view._length, end, required_length, view._char16str, view._length, loc);
      }
      else
      {
        LLFIO_LOG_FATAL(nullptr, "path_view_component::cstr somehow sees no state!");
        abort();
      }
      _base::_ref = typename _base::_view_type(allocated_buffer, end - allocated_buffer);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    struct _custom_callable_deleter_tag
    {
    };
    struct _memory_resource_tag
    {
    };
    struct _stl_allocator_tag
    {
    };

    struct _allocate_not_anything_else
    {
      template <class U> explicit _allocate_not_anything_else(U * /*unused*/) {}
      value_type *operator()(size_t length) const { return static_cast<value_type *>(::operator new[](length * sizeof(value_type))); }
    };
    struct _memory_resource_allocate
    {
      pmr::memory_resource *mr{nullptr};
      value_type *operator()(size_t bytes) const { return static_cast<value_type *>(mr->allocate(bytes)); }
    };
    template <class Alloc> struct _stl_allocator_allocate
    {
      Alloc *alloc{nullptr};
      explicit _stl_allocator_allocate(Alloc *a)
          : alloc(a)
      {
      }
      value_type *operator()(size_t count) const { return alloc->allocate(count); }
    };
    template <class Alloc> static auto _default_allocate(Alloc *alloc)
    {
      using type = std::conditional_t<std::is_void<deleter_type>::value, _stl_allocator_allocate<Alloc>, _allocate_not_anything_else>;
      return type(alloc);
    }

    template <class U = deleter_type> static void _invoke_deleter(void *_del, value_type *p, size_t /*unused*/)
    {
      auto *del = static_cast<U *>(_del);
      (*del)(p);
    }
    static void _memory_resouce_deallocate(void *_mr, value_type *p, size_t bytes)
    {
      auto *mr = static_cast<pmr::memory_resource *>(_mr);
      mr->deallocate(p, bytes);
    }
    template <class Alloc = allocator_type> static void _stl_allocator_deallocate(void *_del, value_type *p, size_t bytes)
    {
      auto *del = static_cast<Alloc *>(_del);
      del->deallocate(p, bytes / sizeof(value_type));
    }
    struct _wrap_invoke_deleter
    {
      _wrap_invoke_deleter(void *_del, value_type *p, size_t bytes) { _invoke_deleter<>(_del, p, bytes); }
    };
    struct _wrap_stl_allocator_deallocate
    {
      _wrap_stl_allocator_deallocate(void *_del, value_type *p, size_t bytes) { _stl_allocator_deallocate<>(_del, p, bytes); }
    };
    static void _default_deleter(void *_del, value_type *p, size_t bytes)
    {
      using type = std::conditional_t<std::is_void<deleter_type>::value, _wrap_stl_allocator_deallocate, _wrap_invoke_deleter>;
      type(_del, p, bytes);
    }

    // used by compare()
    rendered_path(path_view_component view, const std::locale *loc)
        : _deleter1(_default_deleter)
        , _deleter1arg(&_deleter2)
    {
      _init(view, loc, _default_allocate<AllocatorOrDeleter>(&_deleter2));
    }

  public:
    /*! Construct, performing any reencoding or memory copying required.
    \param view The path component view to use as source.
    \param output_zero_termination The zero termination in the output desired
    \param loc The locale to use to perform reencoding.
    \param allocate Either a callable with prototype `value_type *(size_t length)` which
    is defaulted to `return static_cast<value_type *>(::operator new[](length * sizeof(value_type)));`,
    or a `pmr::memory_resource *`. You can return `nullptr` if you wish, the consumer of `rendered_path` will
    see a `buffer` set to `nullptr`.

    If `loc` is defaulted, and an error occurs during any conversion from UTF-8 or UTF-16, an exception of
    `system_error(errc::illegal_byte_sequence)` is thrown.
    This is because if you tell `path_view` that its source is UTF-8 or UTF-16, then that
    must be **valid** UTF. If you wish to supply UTF-invalid paths (which are legal
    on most filesystems), use native narrow or wide encoded source, or binary.
    */
    LLFIO_TEMPLATE(class U, class V)
    LLFIO_TREQUIRES(LLFIO_TPRED(_is_deleter_based<U>), LLFIO_TEXPR(std::declval<U>()((size_t) 1)), LLFIO_TEXPR(std::declval<V>()((value_type *) nullptr)))
    rendered_path(path_view_component view, const std::locale &loc, U &&allocate, V &&deleter = AllocatorOrDeleter(), _custom_callable_deleter_tag = {})
        : _deleter1(_invoke_deleter)
        , _deleter1arg(&_deleter2)
        , _deleter2(static_cast<V &&>(deleter))
    {
      _init(view, loc, static_cast<U &&>(allocate));
    }
    //! \overload
    LLFIO_TEMPLATE(class U, class V)
    LLFIO_TREQUIRES(LLFIO_TPRED(_is_deleter_based<U>), LLFIO_TEXPR(std::declval<U>()((size_t) 1)), LLFIO_TEXPR(std::declval<V>()((value_type *) nullptr)))
    rendered_path(path_view_component view, U &&allocate, V &&deleter = AllocatorOrDeleter(), _custom_callable_deleter_tag = {})
        : _deleter1(_invoke_deleter)
        , _deleter1arg(&_deleter2)
        , _deleter2(static_cast<V &&>(deleter))
    {
      _init(view, (const std::locale *) nullptr, static_cast<U &&>(allocate));
    }
    //! \overload memory_resource
    rendered_path(path_view_component view, const std::locale &loc, pmr::memory_resource &mr, _memory_resource_tag = {})
        : _deleter1(_memory_resouce_deallocate)
        , _deleter1arg(&mr)
    {
      _init(view, &loc, _memory_resource_allocate{&mr});
    }
    //! \overload memory_resource
    rendered_path(path_view_component view, pmr::memory_resource &mr, _memory_resource_tag = {})
        : _deleter1(_memory_resouce_deallocate)
        , _deleter1arg(&mr)
    {
      _init(view, (const std::locale *) nullptr, _memory_resource_allocate{&mr});
    }
    //! \overload STL allocator
    LLFIO_TEMPLATE(class U)
    LLFIO_TREQUIRES(LLFIO_TPRED(_is_allocator_based<U>), LLFIO_TEXPR(std::declval<U>().allocate((size_t) 1)))
    rendered_path(path_view_component view, const std::locale &loc, U &&allocate, _stl_allocator_tag = {})
        : _deleter1(_stl_allocator_deallocate<allocator_type>)
        , _deleter1arg(&_deleter2)
        , _deleter2(static_cast<U &&>(allocate))
    {
      _init(view, &loc, _stl_allocator_allocate<std::decay_t<U>>(static_cast<std::decay_t<U> *>(&_deleter2)));
    }
    //! \overload STL allocator
    LLFIO_TEMPLATE(class U)
    LLFIO_TREQUIRES(LLFIO_TPRED(_is_allocator_based<U>), LLFIO_TEXPR(std::declval<U>().allocate((size_t) 1)))
    rendered_path(path_view_component view, U &&allocate, _stl_allocator_tag = {})
        : _deleter1(_stl_allocator_deallocate<allocator_type>)
        , _deleter1arg(&_deleter2)
        , _deleter2(static_cast<U &&>(allocate))
    {
      _init(view, (const std::locale *) nullptr, _stl_allocator_allocate<std::decay_t<U>>(static_cast<std::decay_t<U> *>(&_deleter2)));
    }
    //! \overload default allocation
    rendered_path(path_view_component view, const std::locale &loc)
        : _deleter1(_default_deleter)
        , _deleter1arg(&_deleter2)
    {
      _init(view, &loc, _default_allocate<AllocatorOrDeleter>(&_deleter2));
    }
    //! \overload
    explicit rendered_path(path_view_component view)
        : _deleter1(_default_deleter)
        , _deleter1arg(&_deleter2)
    {
      _init(view, nullptr, _default_allocate<AllocatorOrDeleter>(&_deleter2));
    }
    //! Construct from a compatible `rendered_path`.
    LLFIO_TEMPLATE(class AllocatorOrDeleter2, size_t _internal_buffer_size2)
    LLFIO_TREQUIRES(LLFIO_TPRED(!std::is_same<rendered_path, rendered_path<ZeroTermination, T, AllocatorOrDeleter2, _internal_buffer_size2>>::value),
                    LLFIO_TPRED(std::is_constructible<AllocatorOrDeleter, AllocatorOrDeleter2>::value))
    explicit rendered_path(rendered_path<ZeroTermination, T, AllocatorOrDeleter2, _internal_buffer_size2> &&o) noexcept
        : _base(o)
        , _bytes_to_delete(o._bytes_to_delete)
        , _deleter1(o._deleter1)
        , _deleter1arg(o._deleter1arg)
        , _deleter2(std::move(o._deleter2))
    {
    }
    ~rendered_path() { reset(); }
    rendered_path(const rendered_path &) = delete;
    rendered_path(rendered_path &&o) noexcept
        : _base(o)
        , _bytes_to_delete(o._bytes_to_delete)
        , _deleter1(o._deleter1)
        , _deleter1arg(o._deleter1arg)
        , _deleter2(std::move(o._deleter2))
    {
      if(this != &o)
      {
        if(o.data() == o._buffer)
        {
          memcpy(_buffer, o._buffer, (o.size() + 1) * sizeof(value_type));
          _base::_ref = typename _base::_view_type(_buffer, _base::_ref.size());
        }
        if(o._deleter1arg == &o._deleter2)
        {
          _deleter1arg = &_deleter2;
        }
        o._ref = {};
        o._bytes_to_delete = 0;
        o._deleter1 = nullptr;
        o._deleter1arg = nullptr;
      }
    }
    rendered_path &operator=(const rendered_path &) = delete;
    rendered_path &operator=(rendered_path &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      this->~rendered_path();
      new(this) rendered_path(std::move(o));
      return *this;
    }

    //! Delete any held resources now
    void reset()
    {
      if(_bytes_to_delete > 0)
      {
        _deleter1(_deleter1arg, const_cast<value_type *>(_base::_ref.data()), _bytes_to_delete);
        _base::_ref = typename _base::_view_type{};
        _bytes_to_delete = 0;
      }
    }
    //! Release from ownership any resources
    value_type *release()
    {
      if(_bytes_to_delete > 0)
      {
        auto *ret = _base::_ref.data();
        _base::_ref = typename _base::_view_type{};
        _bytes_to_delete = 0;
        return ret;
      }
      return nullptr;
    }

    //! The zero termination of this rendered path
    static constexpr enum zero_termination zero_termination() noexcept { return ZeroTermination; }
    //! The size of the internal buffer
    static constexpr size_t internal_buffer_size() noexcept { return (_internal_buffer_size > 0) ? _internal_buffer_size : 1; }
    //! The storage capacity, which may be larger than `size()` if the internal buffer is in use
    size_t capacity() noexcept { return (this->data() == _buffer) ? internal_buffer_size() : this->size(); }

    //! Access the custom deleter instance passed to the constructor
    const AllocatorOrDeleter &deleter() const noexcept { return _deleter2; }
    //! Access the custom deleter instance passed to the constructor
    AllocatorOrDeleter &deleter() noexcept { return _deleter2; }

    //! The memory resource passed to the constructor
    pmr::memory_resource *memory_resource() noexcept { return (pmr::memory_resource *) _deleter1arg; }

    //! Access the custom allocator instance passed to the constructor
    const AllocatorOrDeleter &allocator() const noexcept { return _deleter2; }
    //! Access the custom allocator instance passed to the constructor
    AllocatorOrDeleter &allocator() noexcept { return _deleter2; }

  private:
    size_t _bytes_to_delete{0};
    void (*_deleter1)(void *arg, value_type *buffer, size_t bytes_to_delete){nullptr};
    void *_deleter1arg{nullptr};
    AllocatorOrDeleter _deleter2;
    // MAKE SURE this is the final item in storage, the compiler will elide the storage
    // under optimisation if it can prove it is never used.
    value_type _buffer[internal_buffer_size()]{};
  };
  //! Convenience type alias
  template <class T = typename filesystem::path::value_type, class AllocatorOrDeleter = default_rendered_path_deleter<T[]>,
            size_t _internal_buffer_size = default_internal_buffer_size>
  using zero_terminated_rendered_path = rendered_path<zero_termination::zero_terminated, T, AllocatorOrDeleter, _internal_buffer_size>;
  //! Convenience type alias
  template <class T = typename filesystem::path::value_type, class AllocatorOrDeleter = default_rendered_path_deleter<T[]>,
            size_t _internal_buffer_size = default_internal_buffer_size>
  using not_zero_terminated_rendered_path = rendered_path<zero_termination::not_zero_terminated, T, AllocatorOrDeleter, _internal_buffer_size>;
#ifdef __cpp_concepts
  template <enum path_view_component::zero_termination ZeroTermination, class T, class Deleter, size_t _internal_buffer_size>
  requires(is_source_acceptable<T>)
#elif defined(_MSC_VER)
  template <enum path_view_component::zero_termination ZeroTermination, class T, class Deleter, size_t _internal_buffer_size, class>
#else
  template <enum path_view_component::zero_termination ZeroTermination, class T, class Deleter, size_t _internal_buffer_size,
            typename std::enable_if<(is_source_acceptable<T>), bool>::type>
#endif
  friend class rendered_path;
};
static_assert(std::is_trivially_copyable<path_view_component>::value, "path_view_component is not trivially copyable!");
static_assert(sizeof(path_view_component) == 3 * sizeof(void *), "path_view_component is not three pointers in size!");
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator==(path_view_component x, path_view_component y) noexcept
{
  if(x.native_size() != y.native_size())
  {
    return false;
  }
  if(x._passthrough != y._passthrough)
  {
    return false;
  }
  if(x._char != y._char)
  {
    return false;
  }
  if(x._wchar != y._wchar)
  {
    return false;
  }
  if(x._utf8 != y._utf8)
  {
    return false;
  }
  if(x._utf16 != y._utf16)
  {
    return false;
  }
  if(0 == x._length && 0 == y._length)
  {
    return true;
  }
  assert(x._bytestr != nullptr);
  assert(y._bytestr != nullptr);
  const auto bytes = (x._wchar || x._utf16) ? (x._length * 2) : x._length;
  return 0 == memcmp(x._bytestr, y._bytestr, bytes);
}
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator!=(path_view_component x, path_view_component y) noexcept
{
  if(x.native_size() != y.native_size())
  {
    return true;
  }
  if(x._passthrough != y._passthrough)
  {
    return true;
  }
  if(x._char != y._char)
  {
    return true;
  }
  if(x._wchar != y._wchar)
  {
    return true;
  }
  if(x._utf8 != y._utf8)
  {
    return true;
  }
  if(x._utf16 != y._utf16)
  {
    return true;
  }
  if(0 == x._length && 0 == y._length)
  {
    return false;
  }
  assert(x._bytestr != nullptr);
  assert(y._bytestr != nullptr);
  const auto bytes = (x._wchar || x._utf16) ? (x._length * 2) : x._length;
  return 0 != memcmp(x._bytestr, y._bytestr, bytes);
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view_component::is_source_acceptable<CharT>))
inline constexpr bool operator==(path_view_component /*unused*/, const CharT * /*unused*/) noexcept
{
  static_assert(!path_view_component::is_source_acceptable<CharT>, "Do not use operator== with path_view_component and a string literal, use .compare<>()");
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view_component::is_source_acceptable<CharT>))
inline constexpr bool operator==(const CharT * /*unused*/, path_view_component /*unused*/) noexcept
{
  static_assert(!path_view_component::is_source_acceptable<CharT>, "Do not use operator== with path_view_component and a string literal, use .compare<>()");
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view_component::is_source_acceptable<CharT>))
inline constexpr bool operator!=(path_view_component /*unused*/, const CharT * /*unused*/) noexcept
{
  static_assert(!path_view_component::is_source_acceptable<CharT>, "Do not use operator!= with path_view_component and a string literal, use .compare<>()");
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view_component::is_source_acceptable<CharT>))
inline constexpr bool operator!=(const CharT * /*unused*/, path_view_component /*unused*/) noexcept
{
  static_assert(!path_view_component::is_source_acceptable<CharT>, "Do not use operator!= with path_view_component and a string literal, use .compare<>()");
  return false;
}
//! \brief Visit the underlying source for a `path_view_component` (LLFIO backwards compatible overload)
template <class F> inline LLFIO_PATH_VIEW_CONSTEXPR auto visit(path_view_component view, F &&f)
{
  return view._invoke(static_cast<F &&>(f));
}
//! \brief Visit the underlying source for a `path_view_component` (std compatible overload)
template <class F> inline LLFIO_PATH_VIEW_CONSTEXPR auto visit(F &&f, path_view_component view)
{
  return view._invoke(static_cast<F &&>(f));
}

namespace detail
{
  struct string_view_printer
  {
    std::ostream &s;
    template <class CharT> void operator()(basic_string_view<CharT> v) { s << '"' << v << '"'; }
    void operator()(basic_string_view<char16_t> _v)
    {
      // Cheat by going via filesystem::path
      s << filesystem::path(_v.begin(), _v.end());
    }
    void operator()(basic_string_view<wchar_t> _v)
    {
      // Cheat by going via filesystem::path
      s << filesystem::path(_v.begin(), _v.end());
    }
#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED || 1  // std::filesystem::path support for char8_t input is currently lacking :(
    void operator()(basic_string_view<char8_t> _v)
    {
      basic_string_view<char> v((char *) _v.data(), _v.size());
      s << '"' << v << '"';
    }
#else
    void operator()(basic_string_view<char8_t> _v)
    {
      // Cheat by going via filesystem::path
      s << filesystem::path(_v.begin(), _v.end());
    }
#endif
  };
}  // namespace detail
inline std::ostream &operator<<(std::ostream &s, const path_view_component &v)
{
  if(v._passthrough)
  {
    return s << '"' << QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(v._charstr, v._length) << '"';
  }
  v._invoke(detail::string_view_printer{s});
  return s;
}


/*! \class path_view
\brief A borrowed view of a path. A lightweight trivial-type alternative to
`std::filesystem::path`.

LLFIO is sufficiently fast that `std::filesystem::path` as a wrapper of an
underlying `std::basic_string<>` can be problematically expensive for some
filing system operations due to the potential memory allocation. LLFIO
therefore works exclusively with borrowed views of other path storage.

Some of the API for `std::filesystem::path` is replicated here, however any
APIs which modify the path other than taking subsets are obviously not
possible with borrowed views.

Each consumer of `path_view` defines what the "native platform transport" and
"native platform encoding" is. For LLFIO, the native platform transport is
defined to be `std::filesystem::path::value_type`, which is as follows:

- POSIX: The native platform transport is `char`.
- Microsoft Windows: The native platform transport is `wchar_t`.

**If** the input to `path_view` equals the native platform transport, the bits
supplied will be passed through to the operating system without translation (see below).
*If* the consuming API expects null termination, and the input to `path_view` is null
terminated, then you are *guaranteed* that the originally supplied buffer is passed
through. If the input is not null terminated, a bitwise identical copy is made into
temporary storage (which will be the stack for smaller strings), which is then null
terminated before passing to the consuming API.

If the input to `path_view` does NOT equal the native platform transport, then
a translation of the input bits will be performed into temporary storage just before
calling the consuming API. The rules are as follows:

- POSIX: The native platform encoding is assumed to be UTF-8. If the input is `char8_t`
or `char`, it is not translated. If the input is `char16_t`, a UTF-16 to UTF-8 translation
is performed.

- Microsoft Windows: The native platform encoding is assumed to be UTF-16. If the input
is `char16_t` or `wchar_t`, it is not translated. If the input is `char8_t`, a UTF-8 to UTF-16
translation is performed. If the input is `char`, the Microsoft Windows API for ANSI to
UTF-16 translation is invoked in order to match how Windows ANSI APIs are mapped onto the
Windows Unicode APIs (be aware this is very slow).

# Windows specific notes:

On Microsoft Windows, filesystem paths may require to be zero terminated,
or they may not. Which is the case depends on whether LLFIO calls the NT kernel
API directly rather than the Win32 API. As a general rule as to when which
is used, the NT kernel API is called instead of the Win32 API when:

- For any paths relative to a `path_handle` (the Win32 API does not provide a
race free file system API).
- For any paths beginning with `\!!\`, we pass the path + 3 characters
directly through. This prefix is a pure LLFIO extension, and will not be
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

- If the calling thread has the `ThreadExplicitCaseSensitivity` privilege,
or the system registry has enabled case sensitive lookup for NTFS,
paths are matched case sensitively as raw bytes via `memcmp()`, not case
insensitively (requires slow locale conversion).
- If the NTFS directory has its case sensitive lookup bit set (see
`handle::flag`
- The path limit is 32,767 characters.

If you really care about performance, you are very strongly recommended to use
the NT kernel API wherever possible. Where paths are involved, it is often
three to five times faster due to the multiple memory allocations and string
translations that the Win32 functions perform before calling the NT kernel
routine.

If however you are taking input from some external piece of code, then for
maximum compatibility you should still use the Win32 API.
*/
class path_view : public path_view_component
{
  friend class detail::path_view_iterator;
  friend inline LLFIO_PATH_VIEW_CONSTEXPR bool LLFIO_V2_NAMESPACE::operator==(path_view x, path_view y) noexcept;
  friend inline LLFIO_PATH_VIEW_CONSTEXPR bool LLFIO_V2_NAMESPACE::operator!=(path_view x, path_view y) noexcept;

public:
  //! Const iterator type
  using const_iterator = detail::path_view_iterator;
  //! iterator type
  using iterator = const_iterator;
  //! Reverse iterator
  using reverse_iterator = std::reverse_iterator<iterator>;
  //! Const reverse iterator
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  //! Size type
  using size_type = path_view_component::size_type;
  //! Difference type
  using difference_type = std::ptrdiff_t;

public:
  //! Constructs an empty path view
  constexpr path_view() {}  // NOLINT
  ~path_view() = default;

  //! Constructs a path view component identical to the input, except with different format interpretation.
  constexpr path_view(path_view_component p, format fmt = auto_format) noexcept
      : path_view_component(p, fmt)
  {
  }
  /*! Implicitly constructs a path view from a path. The input path MUST continue to exist for this view to be valid
  (DEVIATES from P1030 due to filesystem::path not exposing its path formatting).
  */
  path_view(const filesystem::path &v, format fmt = auto_format) noexcept  // NOLINT
      : path_view_component(v, fmt)
  {
  }

  /*! Constructs from a basic string if the character type is one of
  `char`, `wchar_t`, `char8_t` or `char16_t`.
  */
  LLFIO_TEMPLATE(class Char)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<Char>))
  constexpr path_view(const std::basic_string<Char> &v, format fmt = auto_format) noexcept  // NOLINT
      : path_view_component(v.data(), v.size(), zero_terminated, fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view(const char *b, size_t l, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(b, l, zt, fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view(const wchar_t *b, size_t l, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(b, l, zt, fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view(const char8_t *b, size_t l, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(b, l, zt, fmt)
  {
  }
  /*! Constructs from a lengthed array of one of `char`, `wchar_t`, `char8_t` or `char16_t`. The input
  string MUST continue to exist for this view to be valid.
  */
  constexpr path_view(const char16_t *b, size_t l, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(b, l, zt, fmt)
  {
  }
  /*! Constructs from a lengthed array of `byte`. The input
  array MUST continue to exist for this view to be valid.
  */
  constexpr path_view(const byte *b, size_t l, enum zero_termination zt) noexcept
      : path_view_component(b, l, zt)
  {
  }

  /*! Implicitly constructs a path view from a zero terminated pointer to a
  character array, which must be one of `char`, `wchar_t`, `char8_t` or `char16_t`.
  The input string MUST continue to exist for this view to be valid.
  */
  LLFIO_TEMPLATE(class Char)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<Char>))
  constexpr path_view(const Char *s, format fmt = auto_format) noexcept
      : path_view_component(s, detail::constexpr_strlen(s), zero_terminated, fmt)
  {
  }
  /*! Implicitly constructs a path view from a zero terminated pointer to byte array.
  The input array MUST continue to exist for this view to be valid.
  */
  constexpr path_view(const byte *s) noexcept
      : path_view_component(s)
  {
  }

  /*! Constructs from a basic string view if the character type is one of
  `char`, `wchar_t`, `char8_t` or `char16_t`.
  */
  LLFIO_TEMPLATE(class Char)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<Char>))
  constexpr path_view(basic_string_view<Char> v, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(v.data(), v.size(), zt, fmt)
  {
  }
  /*! Constructs from a `span<const byte>`.
   */
  constexpr path_view(span<const byte> v, enum zero_termination zt) noexcept
      : path_view_component(v, zt)
  {
  }

  /*! Constructs from an iterator sequence if the iterator is contiguous, if its
  value type is one of `char`, `wchar_t`, `char8_t` or `char16_t`.

  (DEVIATES from P1030, cannot detect contiguous iterator in SFINAE in C++ 14)
  */
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<typename It::value_type>), LLFIO_TPRED(is_source_chartype_acceptable<typename End::value_type>))
  constexpr path_view(It b, End e, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(addressof(*b), e - b, zt, fmt)
  {
  }
  //! \overload
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(is_source_chartype_acceptable<std::decay_t<It>>), LLFIO_TPRED(is_source_chartype_acceptable<std::decay_t<End>>))
  constexpr path_view(It *b, End *e, enum zero_termination zt, format fmt = auto_format) noexcept
      : path_view_component(b, e - b, zt, fmt)
  {
  }
  /*! Constructs from an iterator sequence if the iterator is contiguous, if its
  value type is `byte`.

  (DEVIATES from P1030, cannot detect contiguous iterator in SFINAE in C++ 14)
  */
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(std::is_same<typename It::value_type, byte>::value), LLFIO_TPRED(std::is_same<typename End::value_type, byte>::value))
  constexpr path_view(It b, End e, enum zero_termination zt) noexcept
      : path_view_component(addressof(*b), e - b, zt, binary_format)
  {
  }
  //! \overload
  LLFIO_TEMPLATE(class It, class End)
  LLFIO_TREQUIRES(LLFIO_TPRED(std::is_same<std::decay_t<It>, byte>::value), LLFIO_TPRED(std::is_same<std::decay_t<End>, byte>::value))
  constexpr path_view(It *b, End *e, enum zero_termination zt) noexcept
      : path_view_component(b, e - b, zt, binary_format)
  {
  }

  //! Default copy constructor
  path_view(const path_view &) = default;
  //! Default move constructor
  path_view(path_view &&o) noexcept = default;
  //! Default copy assignment
  path_view &operator=(const path_view &p) = default;
  //! Default move assignment
  path_view &operator=(path_view &&p) noexcept = default;

  //! True if has root path
  LLFIO_PATH_VIEW_CONSTEXPR bool has_root_path() const noexcept { return !root_path().empty(); }
  //! True if has root name
  LLFIO_PATH_VIEW_CONSTEXPR bool has_root_name() const noexcept { return !root_name().empty(); }
  //! True if has root directory
  LLFIO_PATH_VIEW_CONSTEXPR bool has_root_directory() const noexcept { return !root_directory().empty(); }
  //! True if has relative path
  LLFIO_PATH_VIEW_CONSTEXPR bool has_relative_path() const noexcept { return !relative_path().empty(); }
  //! True if has parent path
  LLFIO_PATH_VIEW_CONSTEXPR bool has_parent_path() const noexcept { return !parent_path().empty(); }
  //! True if has filename
  LLFIO_PATH_VIEW_CONSTEXPR bool has_filename() const noexcept { return !filename().empty(); }
  //! True if absolute
  constexpr bool is_absolute() const noexcept
  {
    auto sep_idx = this->_find_first_sep();
    if(_npos == sep_idx)
    {
      return false;
    }
#ifdef _WIN32
    if(is_ntpath())
      return true;
    return this->_invoke([sep_idx](const auto &v) {
      if(sep_idx == 0)
      {
        if(v[sep_idx + 1] == preferred_separator)  // double separator at front
          return true;
      }
      auto colon_idx = v.find(':');
      return colon_idx < sep_idx;  // colon before first separator
    });
#else
    return sep_idx == 0;
#endif
  }
  //! True if relative
  constexpr bool is_relative() const noexcept { return !is_absolute(); }
#ifdef _WIN32
  // True if the path view is a NT kernel path starting with `\!!\` or `\??\`
  constexpr bool is_ntpath() const noexcept
  {
    return this->_invoke([](const auto &v) {
      if(v.size() < 4)
      {
        return false;
      }
      const auto *d = v.data();
      if(d[0] == '\\' && d[1] == '!' && d[2] == '!' && d[3] == '\\')
      {
        return true;
      }
      if(d[0] == '\\' && d[1] == '?' && d[2] == '?' && d[3] == '\\')
      {
        return true;
      }
      return false;
    });
  }
  // True if the path view is a UNC path starting with `\\`
  constexpr bool is_uncpath() const noexcept
  {
    return this->_invoke([](const auto &v) {
      if(v.size() < 2)
      {
        return false;
      }
      const auto *d = v.data();
      if(d[0] == '\\' && d[1] == '\\')
      {
        return true;
      }
      return false;
    });
  }
  // True if the path view matches the format of an LLFIO deleted file
  constexpr bool is_llfio_deleted() const noexcept
  {
    return filename()._invoke([](const auto &v) {
      if(v.size() == 64 + 8)
      {
        // Could be one of our "deleted" files, is he all hex + ".deleted"?
        for(size_t n = 0; n < 64; n++)
        {
          auto c = v[n];
          if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
          {
            return false;
          }
        }
        return v[64] == '.' && v[65] == 'd' && v[66] == 'e' && v[67] == 'l' && v[68] == 'e' && v[69] == 't' && v[70] == 'e' && v[71] == 'd';
      }
      return false;
    });
  }
#endif

  //! Returns an iterator to the first path component
  constexpr inline const_iterator cbegin() const noexcept;
  //! Returns an iterator to the first path component
  constexpr inline const_iterator begin() const noexcept;
  //! Returns an iterator to the first path component
  constexpr inline iterator begin() noexcept;
  //! Returns an iterator to after the last path component
  constexpr inline const_iterator cend() const noexcept;
  //! Returns an iterator to after the last path component
  constexpr inline const_iterator end() const noexcept;
  //! Returns an iterator to after the last path component
  constexpr inline iterator end() noexcept;

  //! Returns a copy of this view with the end adjusted to match the final separator.
  LLFIO_PATH_VIEW_CONSTEXPR path_view remove_filename() const noexcept
  {
    auto sep_idx = this->_find_last_sep();
    if(_npos == sep_idx)
    {
      // Sorry, this semantic is so broken that it's unwise to emulate!
#if 0  // LLFIO_USING_EXPERIMENTAL_FILESYSTEM && defined(_MSC_VER)
      return this->_invoke([&](const auto &v) {
        // MSVC's Experimental Filesystem has some really, really weird semantics :(
        return *this;
      });
#else
      return path_view();
#endif
    }
    return this->_invoke([sep_idx, this](auto v) {
      return path_view(v.data(),
#if LLFIO_USING_EXPERIMENTAL_FILESYSTEM
#ifdef _MSC_VER
                       (sep_idx > 3 && (!is_uncpath() || v.data()[2] == '.' || v.data()[2] == '?')) ? sep_idx : (sep_idx + 1)
#else
                       (v.size() - 1 == sep_idx) ? sep_idx : (sep_idx + 1)
#endif
#else
                       (sep_idx + 1)
#endif
                       ,
                       not_zero_terminated, formatting());
    });
  }
  //! Returns a view of the root name part of this view e.g. C:
  LLFIO_PATH_VIEW_CONSTEXPR path_view root_name() const noexcept
  {
    auto sep_idx = this->_find_first_sep();
    if(_npos == sep_idx)
    {
      return path_view();
    }
    return this->_invoke([sep_idx, this](const auto &v) { return path_view(v.data(), sep_idx, not_zero_terminated, formatting()); });
  }
  //! Returns a view of the root directory, if there is one e.g. /
  LLFIO_PATH_VIEW_CONSTEXPR path_view root_directory() const noexcept
  {
    auto sep_idx = this->_find_first_sep();
    if(_npos == sep_idx)
    {
      return path_view();
    }
    return this->_invoke([sep_idx, this](const auto &v) {
#ifdef _WIN32
      auto colon_idx = v.find(':');
      if(colon_idx < sep_idx)
      {
        return path_view(v.data() + sep_idx, 1, not_zero_terminated, formatting());
      }
#endif
      if(sep_idx == 0)
      {
        return path_view(v.data(), 1, not_zero_terminated, formatting());
      }
      return path_view();
    });
  }
  //! Returns, if any, a view of the root path part of this view e.g. C:/
  LLFIO_PATH_VIEW_CONSTEXPR path_view root_path() const noexcept
  {
    auto sep_idx = this->_find_first_sep();
    if(_npos == sep_idx)
    {
      return path_view();
    }
#ifdef _WIN32
    return this->_invoke([this, sep_idx](const auto &v) mutable {
      // Special case \\.\ and \\?\ to match filesystem::path
      if(is_ntpath() || (v.size() >= 4 && sep_idx == 0 && v[1] == '\\' && (v[2] == '.' || v[2] == '?') && v[3] == '\\'))
      {
        return path_view(v.data() + 0, 4, not_zero_terminated, formatting());
      }
      // Is there a drive letter before the first separator?
      auto colon_idx = v.find(':');
      if(colon_idx < sep_idx)
      {
        return path_view(v.data(), sep_idx + 1, not_zero_terminated, formatting());
      }
      // UNC paths return the server name as the root path
      if(is_uncpath())
      {
        sep_idx = this->_find_first_sep(2);
        if(_npos == sep_idx)
        {
          return *this;
        }
        return path_view(v.data(), sep_idx + 1, not_zero_terminated, formatting());
      }
#else
    return this->_invoke([this, sep_idx](const auto &v) {
#endif
      if(sep_idx == 0)
      {
        return path_view(v.data(), 1, not_zero_terminated, formatting());
      }
      return path_view();
    });
  }
  //! Returns a view of everything after the root path
  LLFIO_PATH_VIEW_CONSTEXPR path_view relative_path() const noexcept
  {
    auto sep_idx = this->_find_first_sep();
    if(_npos == sep_idx)
    {
      return *this;
    }
#ifdef _WIN32
    return this->_invoke([this, sep_idx](const auto &v) mutable {
      // Special case \\.\ and \\?\ to match filesystem::path
      if(is_ntpath() || (v.size() >= 4 && sep_idx == 0 && v[1] == '\\' && (v[2] == '.' || v[2] == '?') && v[3] == '\\'))
      {
        return path_view(v.data() + 4, v.size() - 4, this->zero_termination(), formatting());
      }
      // Is there a drive letter before the first separator?
      auto colon_idx = v.find(':');
      if(colon_idx < sep_idx)
      {
        return path_view(v.data() + sep_idx + 1, v.size() - sep_idx - 1, this->zero_termination(), formatting());
      }
      // UNC paths return the server name as the root path
      if(is_uncpath())
      {
        sep_idx = this->_find_first_sep(2);
        if(_npos == sep_idx)
        {
          return path_view();
        }
        return path_view(v.data() + sep_idx + 1, v.size() - sep_idx - 1, this->zero_termination(), formatting());
      }
#else
    return this->_invoke([this, sep_idx](const auto &v) {
#endif
      if(sep_idx == 0)
      {
        return path_view(v.data() + 1, v.size() - 1, this->zero_termination(), formatting());
      }
      return *this;
    });
  }
  //! Returns a view of the everything apart from the filename part of this view
  LLFIO_PATH_VIEW_CONSTEXPR path_view parent_path() const noexcept
  {
    auto sep_idx = this->_find_last_sep();
    if(_npos == sep_idx)
    {
      return path_view();
    }
#ifdef _WIN32
    return this->_invoke([this, sep_idx](const auto &v) {
      // UNC paths return a trailing slash if the parent path is the server name
      if(is_uncpath())
      {
        auto sep_idx2 = this->_find_first_sep(2);
        if(sep_idx2 == sep_idx)
        {
          return path_view(v.data(), sep_idx + 1, not_zero_terminated, formatting());
        }
      }
#if !LLFIO_USING_EXPERIMENTAL_FILESYSTEM
      // If a C:\ or whatever, return exactly that.
      auto rp = root_path();
      if(rp.native_size() == native_size())
      {
        return *this;
      }
      return path_view(v.data(), (sep_idx == 0) ? 1 : sep_idx, not_zero_terminated, formatting());
#else
      return path_view(v.data(), (sep_idx == 0 && this->_length > 1) ? 1 : sep_idx, not_zero_terminated, formatting());
#endif
    });
#elif LLFIO_USING_EXPERIMENTAL_FILESYSTEM  // Filesystem TS returns parent path "" for "/"
    return this->_invoke(
    [this, sep_idx](const auto &v) { return path_view(v.data(), (sep_idx == 0 && this->_length > 1) ? 1 : sep_idx, not_zero_terminated, formatting()); });
#else
  return this->_invoke([this, sep_idx](const auto &v) { return path_view(v.data(), (sep_idx == 0) ? 1 : sep_idx, not_zero_terminated, formatting()); });
#endif
  }
  //! Returns a view of the filename part of this view.
  LLFIO_PATH_VIEW_CONSTEXPR path_view filename() const noexcept { return this->_filename(); }
  //! Returns a view of this view without a trailing separator, if there is one, unless the input is '/'
  LLFIO_PATH_VIEW_CONSTEXPR path_view without_trailing_separator() const noexcept
  {
    auto sep_idx = this->_find_last_sep();
    return this->_invoke([this, sep_idx](const auto &v) {
      if(sep_idx > 0 && v.size() - 1 == sep_idx)
      {
        return path_view(v.data(), sep_idx, not_zero_terminated, formatting());
      }
      else
      {
        return *this;
      }
    });
  }

  /*! Compares the two path views for equivalence or ordering using `T`
  as the destination encoding, if necessary.

  If the source encodings of the two path views are compatible, a
  lexicographical comparison is performed. If they are incompatible,
  either or both views are converted to the destination encoding
  using `rendered_path<T, Delete, _internal_buffer_size>`, and then a
  lexicographical comparison is performed.

  This can, for obvious reasons, be expensive. It can also throw
  exceptions, as `rendered_path` does.

  If the destination encoding is `byte`, `memcmp()` is used,
  and `rendered_path` is never invoked as the two sources are byte
  compared directly.
  */
  LLFIO_TEMPLATE(class T = typename filesystem::path::value_type, class Deleter = default_rendered_path_deleter<T[]>,
                 size_t _internal_buffer_size = default_internal_buffer_size)
  LLFIO_TREQUIRES(LLFIO_TPRED(path_view::is_source_acceptable<T>))
  constexpr inline int compare(path_view p, const std::locale &loc) const;
  //! \overload
  LLFIO_TEMPLATE(class T = typename filesystem::path::value_type, class Deleter = default_rendered_path_deleter<T[]>,
                 size_t _internal_buffer_size = default_internal_buffer_size)
  LLFIO_TREQUIRES(LLFIO_TPRED(path_view::is_source_acceptable<T>))
  constexpr inline int compare(path_view p) const;
};

namespace detail
{
  template <class T> class value_pointer_fascade
  {
    T _v;

  public:
    constexpr value_pointer_fascade(T o)
        : _v(o)
    {
    }
    constexpr const T &operator*() const noexcept { return _v; }
    constexpr T &operator*() noexcept { return _v; }
    constexpr const T *operator->() const noexcept { return &_v; }
    constexpr T *operator->() noexcept { return &_v; }
  };
  class path_view_iterator
  {
    friend class LLFIO_V2_NAMESPACE::path_view;

  public:
    //! Value type
    using value_type = path_view_component;
    //! Reference type
    using reference = value_type;
    //! Const reference type
    using const_reference = const value_type;
    //! Pointer type
    using pointer = value_pointer_fascade<value_type>;
    //! Const pointer type
    using const_pointer = value_pointer_fascade<const value_type>;
    //! Size type
    using size_type = size_t;

  private:
    const path_view *_parent{nullptr};
    size_type _begin{0}, _end{0};
    int _special{0};  // -1 if at preceding /, +1 if at trailing /

    static constexpr auto _npos = string_view::npos;
    constexpr bool _is_end() const noexcept { return (nullptr == _parent) || (!_special && _parent->native_size() == _begin); }
    LLFIO_PATH_VIEW_CONSTEXPR value_type _get() const noexcept
    {
      assert(_parent != nullptr);
      return _parent->_invoke([this](const auto &v) {
        assert(_end <= v.size());
        assert(_begin <= _end);
        return path_view_component(v.data() + _begin, _end - _begin,
                                   (_end == v.size()) ? _parent->zero_termination() : path_view_component::not_zero_terminated);
      });
    }
    LLFIO_PATH_VIEW_CONSTEXPR void _inc() noexcept
    {
      _begin = (_end > 0 && !_special) ? (_end + 1) : _end;
      _end = _parent->_find_first_sep(_begin);
      if(0 == _end)
      {
        // Path has a beginning /
        _special = -1;
        _end = 1;
        return;
      }
      if(_npos == _end)
      {
        _end = _parent->native_size();
        if(_begin == _end && !_special)
        {
          // Path has a trailing /
          _special = 1;
          return;
        }
        if(_begin > _end)
        {
          _begin = _end;
        }
      }
      _special = 0;
    }
    constexpr void _dec() noexcept
    {
      auto is_end = _is_end();
      _end = is_end ? _begin : (_begin - 1);
      if(0 == _end)
      {
        // Path has a beginning /
        _special = -1;
        _begin = 0;
        _end = 1;
        return;
      }
      _begin = _parent->_find_last_sep(_end - 1);
      if(is_end && _begin == _end - 1)
      {
        // Path has a trailing /
        _special = 1;
        _begin = _end;
        return;
      }
      _begin = (_npos == _begin) ? 0 : (_begin + 1);
      _special = 0;
    }

    constexpr path_view_iterator(const path_view *p, bool end)
        : _parent(p)
        , _begin(end ? p->native_size() : 0)
        , _end(end ? p->native_size() : 0)
    {
      if(!end)
      {
        _inc();
      }
    }

  public:
    path_view_iterator() = default;
    path_view_iterator(const path_view_iterator &) = default;
    path_view_iterator(path_view_iterator &&) = default;
    path_view_iterator &operator=(const path_view_iterator &) = default;
    path_view_iterator &operator=(path_view_iterator &&) = default;
    ~path_view_iterator() = default;

    LLFIO_PATH_VIEW_CONSTEXPR const_reference operator*() const noexcept { return _get(); }
    LLFIO_PATH_VIEW_CONSTEXPR reference operator*() noexcept { return _get(); }
    LLFIO_PATH_VIEW_CONSTEXPR const_pointer operator->() const noexcept { return _get(); }
    LLFIO_PATH_VIEW_CONSTEXPR pointer operator->() noexcept { return _get(); }

    constexpr bool operator!=(path_view_iterator o) const noexcept
    {
      if(_is_end() && o._is_end())
      {
        return false;
      }
      return _parent != o._parent || _begin != o._begin || _end != o._end || _special != o._special;
    }
    constexpr bool operator==(path_view_iterator o) const noexcept
    {
      if(_is_end() && o._is_end())
      {
        return true;
      }
      return _parent == o._parent && _begin == o._begin && _end == o._end && _special == o._special;
    }

    constexpr path_view_iterator &operator--() noexcept
    {
      _dec();
      return *this;
    }
    constexpr path_view_iterator operator--(int) noexcept
    {
      auto self(*this);
      _dec();
      return self;
    }
    LLFIO_PATH_VIEW_CONSTEXPR path_view_iterator &operator++() noexcept
    {
      _inc();
      return *this;
    }
    LLFIO_PATH_VIEW_CONSTEXPR path_view_iterator operator++(int) noexcept
    {
      auto self(*this);
      _inc();
      return self;
    }
  };
}  // namespace detail

constexpr inline path_view::const_iterator path_view::cbegin() const noexcept
{
  return const_iterator(this, false);
}
constexpr inline path_view::const_iterator path_view::cend() const noexcept
{
  return const_iterator(this, true);
}
constexpr inline path_view::const_iterator path_view::begin() const noexcept
{
  return cbegin();
}
constexpr inline path_view::iterator path_view::begin() noexcept
{
  return cbegin();
}
constexpr inline path_view::const_iterator path_view::end() const noexcept
{
  return cend();
}
constexpr inline path_view::iterator path_view::end() noexcept
{
  return cend();
}

inline LLFIO_PATH_VIEW_CONSTEXPR bool operator==(path_view x, path_view y) noexcept
{
  auto it1 = x.begin(), it2 = y.begin();
  for(; it1 != x.end() && it2 != y.end(); ++it1, ++it2)
  {
    if(*it1 != *it2)
    {
      return false;
    }
  }
  if(it1 == x.end() && it2 != y.end())
  {
    return false;
  }
  if(it1 != x.end() && it2 == y.end())
  {
    return false;
  }
  return true;
}
inline LLFIO_PATH_VIEW_CONSTEXPR bool operator!=(path_view x, path_view y) noexcept
{
  auto it1 = x.begin(), it2 = y.begin();
  for(; it1 != x.end() && it2 != y.end(); ++it1, ++it2)
  {
    if(*it1 != *it2)
    {
      return true;
    }
  }
  if(it1 == x.end() && it2 != y.end())
  {
    return true;
  }
  if(it1 != x.end() && it2 == y.end())
  {
    return true;
  }
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view::is_source_acceptable<CharT>))
inline constexpr bool operator==(path_view /*unused*/, const CharT * /*unused*/) noexcept
{
  static_assert(!path_view::is_source_acceptable<CharT>, "Do not use operator== with path_view and a string literal, use .compare<>()");
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view::is_source_acceptable<CharT>))
inline constexpr bool operator==(const CharT * /*unused*/, path_view /*unused*/) noexcept
{
  static_assert(!path_view::is_source_acceptable<CharT>, "Do not use operator== with path_view and a string literal, use .compare<>()");
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view::is_source_acceptable<CharT>))
inline constexpr bool operator!=(path_view /*unused*/, const CharT * /*unused*/) noexcept
{
  static_assert(!path_view::is_source_acceptable<CharT>, "Do not use operator!= with path_view and a string literal, use .compare<>()");
  return false;
}
LLFIO_TEMPLATE(class CharT)
LLFIO_TREQUIRES(LLFIO_TPRED(path_view::is_source_acceptable<CharT>))
inline constexpr bool operator!=(const CharT * /*unused*/, path_view /*unused*/) noexcept
{
  static_assert(!path_view::is_source_acceptable<CharT>, "Do not use operator!= with path_view and a string literal, use .compare<>()");
  return false;
}
#ifdef __cpp_concepts
template <class T, class Deleter, size_t _internal_buffer_size>
requires(path_view::is_source_acceptable<T>)
#elif defined(_MSC_VER)
template <class T, class Deleter, size_t _internal_buffer_size, class>
#else
template <class T, class Deleter, size_t _internal_buffer_size, typename std::enable_if<(path_view::is_source_acceptable<T>), bool>::type>
#endif
constexpr inline int path_view::compare(path_view o, const std::locale &loc) const
{
  auto it1 = begin(), it2 = o.begin();
  for(; it1 != end() && it2 != o.end(); ++it1, ++it2)
  {
    int res = it1->compare<T, Deleter, _internal_buffer_size>(*it2, loc);
    if(res != 0)
    {
      return res;
    }
  }
  if(it1 == end() && it2 != o.end())
  {
    return -1;
  }
  if(it1 != end() && it2 == o.end())
  {
    return 1;
  }
  return 0;  // identical
}
#ifdef __cpp_concepts
template <class T, class Deleter, size_t _internal_buffer_size>
requires(path_view::is_source_acceptable<T>)
#elif defined(_MSC_VER)
template <class T, class Deleter, size_t _internal_buffer_size, class>
#else
template <class T, class Deleter, size_t _internal_buffer_size, typename std::enable_if<(path_view::is_source_acceptable<T>), bool>::type>
#endif
constexpr inline int path_view::compare(path_view o) const
{
  auto it1 = begin(), it2 = o.begin();
  for(; it1 != end() && it2 != o.end(); ++it1, ++it2)
  {
    int res = it1->compare<T, Deleter, _internal_buffer_size>(*it2);
    if(res != 0)
    {
      return res;
    }
  }
  if(it1 == end() && it2 != o.end())
  {
    return -1;
  }
  if(it1 != end() && it2 == o.end())
  {
    return 1;
  }
  return 0;  // identical
}

//! Append a path view component to a path
inline filesystem::path &operator+=(filesystem::path &a, path_view_component b)
{
  path_view_component::not_zero_terminated_rendered_path<> zpath(b);
  basic_string_view<filesystem::path::value_type> _ = zpath.as_string_view();
  a.concat(_.begin(), _.end());
  return a;
}
//! Append a path view component to a path
inline filesystem::path &operator/=(filesystem::path &a, path_view_component b)
{
  path_view_component::not_zero_terminated_rendered_path<> zpath(b);
  basic_string_view<filesystem::path::value_type> _ = zpath.as_string_view();
  a.append(_.begin(), _.end());
  return a;
}
//! Append a path view component to a path
inline filesystem::path operator/(const filesystem::path &a, path_view_component b)
{
  filesystem::path ret(a);
  return ret /= b;
}
//! Append a path view component to a path
inline filesystem::path operator/(filesystem::path &&a, path_view_component b)
{
  filesystem::path ret(std::move(a));
  return ret /= b;
}
namespace detail
{
  struct path_view_component_operator_slash_visitor
  {
    filesystem::path ret;
    template <class CharT, class Traits> void operator()(basic_string_view<CharT, Traits> sv) { ret.assign(sv.begin(), sv.end()); }
#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED || (defined(_LIBCPP_VERSION) && _LIBCPP_VERSION < 12000 /* approx end of 2020 */)
    void operator()(basic_string_view<char8_t> sv)
    {
      basic_string_view<char> _((const char *) sv.data(), sv.size());
      ret.assign(_.begin(), _.end());
    }
#endif
    template <class T> void operator()(span<T> /*unused*/) { throw std::logic_error("filesystem::path cannot be constructed from a byte input."); }
  };
}  // namespace detail
//! Append a path view component to a path view component
LLFIO_TEMPLATE(class T)
LLFIO_TREQUIRES(LLFIO_TPRED(std::is_same<T, path_view_component>::value || std::is_same<T, path_view>::value))
inline filesystem::path operator/(T a, path_view_component b)
{
  detail::path_view_component_operator_slash_visitor x;
  visit(x, a);
  return x.ret /= b;
}


#ifndef NDEBUG
static_assert(std::is_trivially_copyable<path_view>::value, "path_view is not a trivially copyable!");
#endif

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/path_view.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
