/* Configures AFIO
(C) 2015-2018 Niall Douglas <http://www.nedproductions.biz/> (24 commits)
File Created: Dec 2015


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

#ifndef AFIO_CONFIG_HPP
#define AFIO_CONFIG_HPP

//#include <iostream>
//#define AFIO_LOG_TO_OSTREAM std::cerr
//#define AFIO_LOGGING_LEVEL 6
//#define AFIO_DISABLE_PATHS_IN_FAILURE_INFO

//! \file config.hpp Configures a compiler environment for AFIO header and source code

//! \defgroup config Configuration macros

#if !defined(AFIO_HEADERS_ONLY) && !defined(BOOST_ALL_DYN_LINK)
//! \brief Whether AFIO is a headers only library. Defaults to 1 unless BOOST_ALL_DYN_LINK is defined. \ingroup config
#define AFIO_HEADERS_ONLY 1
#endif

//! \def AFIO_DISABLE_PATHS_IN_FAILURE_INFO
//! \brief Define to not record the current handle's path in any failure info.
#if DOXYGEN_IS_IN_THE_HOUSE
#define AFIO_DISABLE_PATHS_IN_FAILURE_INFO not defined
#endif

#if !defined(AFIO_LOGGING_LEVEL)
//! \brief How much detail to log. 0=disabled, 1=fatal, 2=error, 3=warn, 4=info, 5=debug, 6=all.
//! Defaults to error level. \ingroup config
#ifdef NDEBUG
#define AFIO_LOGGING_LEVEL 1  // fatal
#else
#define AFIO_LOGGING_LEVEL 3  // warn
#endif
#endif

#ifndef AFIO_LOG_TO_OSTREAM
#if !defined(NDEBUG) && !defined(AFIO_DISABLE_LOG_TO_OSTREAM)
#include <iostream>  // for std::cerr
//! \brief Any `ostream` to also log to. If `NDEBUG` is not defined, `std::cerr` is the default.
#define AFIO_LOG_TO_OSTREAM std::cerr
#endif
#endif

#if !defined(AFIO_LOG_BACKTRACE_LEVELS)
//! \brief Bit mask of which log levels should be stack backtraced
//! which will slow those logs thirty fold or so. Defaults to (1U<<1U)|(1U<<2U)|(1U<<3U) i.e. stack backtrace
//! on fatal, error and warn logs. \ingroup config
#define AFIO_LOG_BACKTRACE_LEVELS ((1U << 1U) | (1U << 2U) | (1U << 3U))
#endif

#if !defined(AFIO_LOGGING_MEMORY)
#ifdef NDEBUG
#define AFIO_LOGGING_MEMORY 4096
#else
//! \brief How much memory to use for the log.
//! Defaults to 4Kb if NDEBUG defined, else 1Mb. \ingroup config
#define AFIO_LOGGING_MEMORY (1024 * 1024)
#endif
#endif

#if !defined(AFIO_EXPERIMENTAL_STATUS_CODE)
//! \brief Whether to use SG14 experimental `status_code` instead of `std::error_code`
#define AFIO_EXPERIMENTAL_STATUS_CODE 0
#endif


#if defined(_WIN32)
#if !defined(_UNICODE)
#error AFIO cannot target the ANSI Windows API. Please define _UNICODE to target the Unicode Windows API.
#endif
#if !defined(_WIN32_WINNT)
#define _WIN32_WINNT 0x0600
#elif _WIN32_WINNT < 0x0600
#error _WIN32_WINNT must at least be set to Windows Vista for AFIO to work
#endif
#if defined(NTDDI_VERSION) && NTDDI_VERSION < 0x06000000
#error NTDDI_VERSION must at least be set to Windows Vista for AFIO to work
#endif
#endif

#ifdef __APPLE__
#define AFIO_MISSING_PIOV 1
#endif

// Pull in detection of __MINGW64_VERSION_MAJOR
#ifdef __MINGW32__
#include <_mingw.h>
#endif

#include "quickcpplib/include/cpp_feature.h"

#ifndef __cpp_exceptions
#error AFIO needs C++ exceptions to be turned on
#endif
#ifndef __cpp_alias_templates
#error AFIO needs template alias support in the compiler
#endif
#ifndef __cpp_variadic_templates
#error AFIO needs variadic template support in the compiler
#endif
#if __cpp_constexpr < 201304L && !defined(_MSC_VER)
#error AFIO needs relaxed constexpr (C++ 14) support in the compiler
#endif
#ifndef __cpp_init_captures
#error AFIO needs lambda init captures support in the compiler (C++ 14)
#endif
#ifndef __cpp_attributes
#error AFIO needs attributes support in the compiler
#endif
#ifndef __cpp_variable_templates
#error AFIO needs variable template support in the compiler
#endif
#ifndef __cpp_generic_lambdas
#error AFIO needs generic lambda support in the compiler
#endif
#ifdef __has_include
// clang-format off
#if !__has_include(<filesystem>) && !__has_include(<experimental/filesystem>)
// clang-format on
#error AFIO needs an implementation of the Filesystem TS in the standard library
#endif
#endif


#include "quickcpplib/include/import.h"

#ifdef AFIO_UNSTABLE_VERSION
#include "../revision.hpp"
#define AFIO_V2 (QUICKCPPLIB_BIND_NAMESPACE_VERSION(afio_v2, AFIO_PREVIOUS_COMMIT_UNIQUE))
#else
#define AFIO_V2 (QUICKCPPLIB_BIND_NAMESPACE_VERSION(afio_v2))
#endif
/*! \def AFIO_V2
\ingroup config
\brief The namespace configuration of this AFIO v2. Consists of a sequence
of bracketed tokens later fused by the preprocessor into namespace and C++ module names.
*/
#if DOXYGEN_IS_IN_THE_HOUSE
//! The AFIO namespace
namespace afio_v2_xxx
{
  //! Collection of file system based algorithms
  namespace algorithm
  {
  }
  //! YAML databaseable empirical testing of a storage's behaviour
  namespace storage_profile
  {
  }
  //! Utility routines often useful when using AFIO
  namespace utils
  {
  }
}
/*! \brief The namespace of this AFIO v2 which will be some unknown inline
namespace starting with `v2_` inside the `boost::afio` namespace.
\ingroup config
*/
#define AFIO_V2_NAMESPACE afio_v2_xxx
/*! \brief Expands into the appropriate namespace markup to enter the AFIO v2 namespace.
\ingroup config
*/
#define AFIO_V2_NAMESPACE_BEGIN                                                                                                                                                                                                                                                                                                \
  namespace afio_v2_xxx                                                                                                                                                                                                                                                                                                        \
  {
/*! \brief Expands into the appropriate namespace markup to enter the C++ module
exported AFIO v2 namespace.
\ingroup config
*/
#define AFIO_V2_NAMESPACE_EXPORT_BEGIN                                                                                                                                                                                                                                                                                         \
  export namespace afio_v2_xxx                                                                                                                                                                                                                                                                                                 \
  {
/*! \brief Expands into the appropriate namespace markup to exit the AFIO v2 namespace.
\ingroup config
*/
#define AFIO_V2_NAMESPACE_END }
#elif defined(GENERATING_AFIO_MODULE_INTERFACE)
#define AFIO_V2_NAMESPACE QUICKCPPLIB_BIND_NAMESPACE(AFIO_V2)
#define AFIO_V2_NAMESPACE_BEGIN QUICKCPPLIB_BIND_NAMESPACE_BEGIN(AFIO_V2)
#define AFIO_V2_NAMESPACE_EXPORT_BEGIN QUICKCPPLIB_BIND_NAMESPACE_EXPORT_BEGIN(AFIO_V2)
#define AFIO_V2_NAMESPACE_END QUICKCPPLIB_BIND_NAMESPACE_END(AFIO_V2)
#else
#define AFIO_V2_NAMESPACE QUICKCPPLIB_BIND_NAMESPACE(AFIO_V2)
#define AFIO_V2_NAMESPACE_BEGIN QUICKCPPLIB_BIND_NAMESPACE_BEGIN(AFIO_V2)
#define AFIO_V2_NAMESPACE_EXPORT_BEGIN QUICKCPPLIB_BIND_NAMESPACE_BEGIN(AFIO_V2)
#define AFIO_V2_NAMESPACE_END QUICKCPPLIB_BIND_NAMESPACE_END(AFIO_V2)
#endif

AFIO_V2_NAMESPACE_BEGIN
class handle;
class file_handle;
AFIO_V2_NAMESPACE_END

// Bring in the Boost-lite macros
#include "quickcpplib/include/config.hpp"
// Bring in filesystem
#if defined(__has_include)
// clang-format off
#if __has_include(<filesystem>) && __cplusplus >= 202000
#include <filesystem>
AFIO_V2_NAMESPACE_BEGIN
namespace filesystem = std::filesystem;
AFIO_V2_NAMESPACE_END
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
AFIO_V2_NAMESPACE_BEGIN
namespace filesystem = std::experimental::filesystem;
AFIO_V2_NAMESPACE_END
#endif
// clang-format on
#elif defined(_MSC_VER)
#include <filesystem>
AFIO_V2_NAMESPACE_BEGIN
namespace filesystem = std::experimental::filesystem;
AFIO_V2_NAMESPACE_END
#else
#error No <filesystem> implementation found
#endif
AFIO_V2_NAMESPACE_BEGIN
struct path_hasher
{
  size_t operator()(const filesystem::path &p) const { return std::hash<filesystem::path::string_type>()(p.native()); }
};
AFIO_V2_NAMESPACE_END


// Configure AFIO_DECL
#if(defined(AFIO_DYN_LINK) || defined(BOOST_ALL_DYN_LINK)) && !defined(AFIO_STATIC_LINK)

#if defined(AFIO_SOURCE)
#define AFIO_DECL QUICKCPPLIB_SYMBOL_EXPORT
#define AFIO_BUILD_DLL
#else
#define AFIO_DECL QUICKCPPLIB_SYMBOL_IMPORT
#endif
#else
#define AFIO_DECL
#endif  // building a shared library

// Configure AFIO_THREAD_LOCAL
#ifndef AFIO_THREAD_LOCAL_IS_CXX11
#define AFIO_THREAD_LOCAL_IS_CXX11 QUICKCPPLIB_THREAD_LOCAL_IS_CXX11
#endif
#ifndef AFIO_THREAD_LOCAL
#define AFIO_THREAD_LOCAL QUICKCPPLIB_THREAD_LOCAL
#endif
#ifndef AFIO_TEMPLATE
#define AFIO_TEMPLATE(...) QUICKCPPLIB_TEMPLATE(__VA_ARGS__)
#endif
#ifndef AFIO_TREQUIRES
#define AFIO_TREQUIRES(...) QUICKCPPLIB_TREQUIRES(__VA_ARGS__)
#endif
#ifndef AFIO_TEXPR
#define AFIO_TEXPR(...) QUICKCPPLIB_TEXPR(__VA_ARGS__)
#endif
#ifndef AFIO_TPRED
#define AFIO_TPRED(...) QUICKCPPLIB_TPRED(__VA_ARGS__)
#endif
#ifndef AFIO_REQUIRES
#define AFIO_REQUIRES(...) QUICKCPPLIB_REQUIRES(__VA_ARGS__)
#endif

// A unique identifier generating macro
#define AFIO_GLUE2(x, y) x##y
#define AFIO_GLUE(x, y) AFIO_GLUE2(x, y)
#define AFIO_UNIQUE_NAME AFIO_GLUE(__t, __COUNTER__)

// Used to tag functions which need to be made free by the AST tool
#ifndef AFIO_MAKE_FREE_FUNCTION
#if 0  //__cplusplus >= 201700  // makes annoying warnings
#define AFIO_MAKE_FREE_FUNCTION [[afio::make_free_function]]
#else
#define AFIO_MAKE_FREE_FUNCTION
#endif
#endif

// Bring in bitfields
#include "quickcpplib/include/bitfield.hpp"
// Bring in scoped undo
#include "quickcpplib/include/scoped_undo.hpp"
AFIO_V2_NAMESPACE_BEGIN
using QUICKCPPLIB_NAMESPACE::scoped_undo::undoer;
AFIO_V2_NAMESPACE_END
// Bring in a span implementation
#include "quickcpplib/include/span.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::span;
AFIO_V2_NAMESPACE_END
// Bring in an optional implementation
#include "quickcpplib/include/optional.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::optional;
AFIO_V2_NAMESPACE_END
// Bring in a byte implementation
#include "quickcpplib/include/byte.hpp"
AFIO_V2_NAMESPACE_BEGIN
using QUICKCPPLIB_NAMESPACE::byte::byte;
using QUICKCPPLIB_NAMESPACE::byte::to_byte;
AFIO_V2_NAMESPACE_END
// Bring in a string_view implementation
#include "quickcpplib/include/string_view.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::string_view;
AFIO_V2_NAMESPACE_END
// Bring in a persistent implementation
#include "quickcpplib/include/persistent.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::persistence;
AFIO_V2_NAMESPACE_END


AFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  // Used to cast an unknown input to some unsigned integer
  AFIO_TEMPLATE(class T, class U)
  AFIO_TREQUIRES(AFIO_TPRED(std::is_unsigned<T>::value && !std::is_same<std::decay_t<U>, std::nullptr_t>::value))
  inline T unsigned_integer_cast(U &&v) { return static_cast<T>(v); }
  AFIO_TEMPLATE(class T)
  AFIO_TREQUIRES(AFIO_TPRED(std::is_unsigned<T>::value))
  inline T unsigned_integer_cast(std::nullptr_t /* unused */) { return static_cast<T>(0); }
  AFIO_TEMPLATE(class T, class U)
  AFIO_TREQUIRES(AFIO_TPRED(std::is_unsigned<T>::value))
  inline T unsigned_integer_cast(U *v) { return static_cast<T>(reinterpret_cast<uintptr_t>(v)); }
}  // namespace detail

AFIO_V2_NAMESPACE_END


/* The SG14 status code implementation is quite profoundly different to the
error code implementation. In the error code implementation, std::error_code
is fixed by the standard library, so we wrap it with extra metadata into
an error_info type. error_info then constructs off a code and a code domain
tag.

Status code, on the other hand, is templated and is designed for custom
domains which can set arbitrary payloads. So we define custom domains and
status codes for AFIO with these combinations:

- win32_error{ DWORD }
- ntkernel_error{ LONG }
- posix_error{ int }
- generic_error{ errc }

Each of these is a separate AFIO custom status code domain. We also define
an erased form of these custom domains, and that is typedefed to
error_domain<intptr_t>::value_type.

This design ensure that AFIO can be configured into either std-based error
handling or SG14 experimental status code handling. It defaults to the latter
as that (a) enables safe header only AFIO on Windows (b) produces better codegen
(c) drags in far fewer STL headers.
*/

#if AFIO_EXPERIMENTAL_STATUS_CODE

// Bring in a result implementation based on status_code
#include "outcome/include/outcome/experimental/status_result.hpp"
#include "outcome/include/outcome/try.hpp"

AFIO_V2_NAMESPACE_BEGIN

#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO

namespace detail
{
  template <class T> struct error_domain_value_type
  {
    //! \brief The type of code
    T ec;

    // The id of the thread where this failure occurred
    uint32_t thread_id{0};
    // The TLS path store entry
    uint16_t tls_path_id1{static_cast<uint16_t>(-1)}, _tls_path_id2{static_cast<uint16_t>(-1)};
    // The id of the relevant log entry in the AFIO log (if logging enabled)
    size_t log_id{static_cast<size_t>(-1)};

    //! Implicitly constructs an instance. Defined in handle.hpp as we need to understand handle.
    inline error_domain_value_type(T _sc);  // NOLINT
  };
}

/*! \class error_domain
\brief The SG14 status code domain for errors in AFIO.
*/
template <class BaseStatusCodeDomain> class error_domain : public BaseStatusCodeDomain
{
  using _base = BaseStatusCodeDomain;

public:
  using string_ref = typename BaseStatusCodeDomain::string_ref;
  using atomic_refcounted_string_ref = typename BaseStatusCodeDomain::atomic_refcounted_string_ref;

  //! \brief The value type of errors in AFIO
  using value_type = detail::error_domain_value_type<typename _base::value_type>;

  error_domain() = default;
  error_domain(const error_domain &) = default;
  error_domain(error_domain &&) = default;
  error_domain &operator=(const error_domain &) = default;
  error_domain &operator=(error_domain &&) = default;
  ~error_domain() = default;

protected:
  virtual inline string_ref _message(const SYSTEM_ERROR2_NAMESPACE::status_code<void> &code) const noexcept override final;
};
#else
template <class BaseStatusCodeDomain> using error_domain = BaseStatusCodeDomain;
#endif

namespace detail
{
  using error_domain_value_system_code = error_domain_value_type<SYSTEM_ERROR2_NAMESPACE::system_code::value_type>;
}

//! An erased status code
using error_code = SYSTEM_ERROR2_NAMESPACE::status_code<SYSTEM_ERROR2_NAMESPACE::erased<detail::error_domain_value_system_code>>;


template <class T> using result = OUTCOME_V2_NAMESPACE::experimental::erased_result<T, error_code>;
using OUTCOME_V2_NAMESPACE::success;
using OUTCOME_V2_NAMESPACE::failure;
using OUTCOME_V2_NAMESPACE::in_place_type;

//! Choose an errc implementation
using SYSTEM_ERROR2_NAMESPACE::errc;

//! Helper for constructing an error code from an errc
inline error_code generic_error(errc c)
{
  return SYSTEM_ERROR2_NAMESPACE::status_code<error_domain<SYSTEM_ERROR2_NAMESPACE::generic_code::domain_type>>(c);
}
#ifndef _WIN32
//! Helper for constructing an error code from a POSIX errno
inline error_code posix_error(int c = errno)
{
  return SYSTEM_ERROR2_NAMESPACE::status_code<error_domain<SYSTEM_ERROR2_NAMESPACE::posix_code::domain_type>>(c);
}
#else
//! Helper for constructing an error code from a DWORD
inline error_code win32_error(SYSTEM_ERROR2_NAMESPACE::win32::DWORD c = SYSTEM_ERROR2_NAMESPACE::win32::GetLastError())
{
  return SYSTEM_ERROR2_NAMESPACE::status_code<error_domain<SYSTEM_ERROR2_NAMESPACE::win32_code::domain_type>>(c);
}
//! Helper for constructing an error code from a NTSTATUS
inline error_code ntkernel_error(SYSTEM_ERROR2_NAMESPACE::win32::NTSTATUS c)
{
  return SYSTEM_ERROR2_NAMESPACE::status_code<error_domain<SYSTEM_ERROR2_NAMESPACE::nt_code::domain_type>>(c);
}
#endif

AFIO_V2_NAMESPACE_END


#else  // AFIO_EXPERIMENTAL_STATUS_CODE


// Bring in a result implementation based on std::error_code
#include "outcome/include/outcome.hpp"

AFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  template <class Src> inline void append_path_info(Src &src, std::string &ret);
  template <class Src> inline void fill_failure_info(Src &src);
}

/*! \struct error_info
\brief The cause of the failure of an operation in AFIO.
*/
struct error_info
{
  template <class Src> friend inline void detail::append_path_info(Src &src, std::string &ret);
  template <class Src> friend inline void detail::fill_failure_info(Src &src);

  //! The error code for the failure
  std::error_code ec;

#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
private:
  // The id of the thread where this failure occurred
  uint32_t _thread_id{0};
  // The TLS path store entry
  uint16_t _tls_path_id1{static_cast<uint16_t>(-1)}, _tls_path_id2{static_cast<uint16_t>(-1)};
  // The id of the relevant log entry in the AFIO log (if logging enabled)
  size_t _log_id{static_cast<size_t>(-1)};

public:
#endif

  //! Default constructor
  error_info() = default;
  // Explicit construction from an error code
  explicit inline error_info(std::error_code _ec);  // NOLINT
  /* NOTE TO SELF: The error_info constructor implementation is in handle.hpp as we need that
  defined before we can do useful logging.
  */
  //! Implicit construct from an error condition enum
  OUTCOME_TEMPLATE(class ErrorCondEnum)
  OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
  error_info(ErrorCondEnum &&v)  // NOLINT
  : error_info(make_error_code(std::forward<ErrorCondEnum>(v)))
  {
  }

  //! Retrieve any first path associated with this failure. Note this only works if called from the same thread as where the failure occurred.
  inline filesystem::path path1() const;
  //! Retrieve any second path associated with this failure. Note this only works if called from the same thread as where the failure occurred.
  inline filesystem::path path2() const;
  //! Retrieve a descriptive message for this failure, possibly with paths and stack backtraces. Extra detail only appears if called from the same thread as where the failure occurred.
  inline std::string message() const;
  /*! Throw this failure as a C++ exception. Firstly if the error code matches any of the standard
  C++ exception types e.g. `bad_alloc`, we throw those types using the string from `message()`
  where possible. We then will throw an `error` exception type.
  */
  inline void throw_as_exception() const;
};
inline bool operator==(const error_info &a, const error_info &b)
{
  return a.ec == b.ec;
}
inline bool operator!=(const error_info &a, const error_info &b)
{
  return a.ec != b.ec;
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator==(const error_info &a, const ErrorCondEnum &b)
{
  return a.ec == std::error_condition(b);
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator==(const ErrorCondEnum &a, const error_info &b)
{
  return std::error_condition(a) == b.ec;
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator!=(const error_info &a, const ErrorCondEnum &b)
{
  return a.ec != std::error_condition(b);
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator!=(const ErrorCondEnum &a, const error_info &b)
{
  return std::error_condition(a) != b.ec;
}
#ifndef NDEBUG
// Is trivial in all ways, except default constructibility
static_assert(std::is_trivially_copyable<error_info>::value, "error_info is not a trivially copyable!");
#endif
inline std::ostream &operator<<(std::ostream &s, const error_info &v)
{
  if(v.ec)
  {
    return s << "afio::error_info(" << v.message() << ")";
  }
  return s << "afio::error_info(null)";
}
// Tell Outcome that error_info is to be treated as an error_code
inline std::error_code make_error_code(error_info ei)
{
  return ei.ec;
}
// Tell Outcome to call error_info::throw_as_exception() on no-value observation
inline void outcome_throw_as_system_error_with_payload(const error_info &ei)
{
  ei.throw_as_exception();
}

/*! \class error
\brief The exception type synthesised and thrown when an `afio::result` or `afio::outcome` is no-value observed.
*/
class error : public filesystem::filesystem_error
{
public:
  error_info ei;

  //! Constructs from an error_info
  explicit error(error_info _ei)
      : filesystem::filesystem_error(_ei.message(), _ei.path1(), _ei.path2(), _ei.ec)
      , ei(_ei)
  {
  }
};

inline void error_info::throw_as_exception() const
{
  std::string msg;
  try
  {
    msg = message();
  }
  catch(...)
  {
  }
  OUTCOME_V2_NAMESPACE::try_throw_std_exception_from_error(ec, msg);
  throw error(*this);
}

template <class T> using result = OUTCOME_V2_NAMESPACE::result<T, error_info>;
template <class T> using outcome = OUTCOME_V2_NAMESPACE::outcome<T, error_info>;
using OUTCOME_V2_NAMESPACE::success;
using OUTCOME_V2_NAMESPACE::failure;
inline error_info error_from_exception(std::exception_ptr &&ep = std::current_exception(), std::error_code not_matched = std::make_error_code(std::errc::resource_unavailable_try_again)) noexcept
{
  return error_info(OUTCOME_V2_NAMESPACE::error_from_exception(std::move(ep), not_matched));
}
using OUTCOME_V2_NAMESPACE::in_place_type;

static_assert(OUTCOME_V2_NAMESPACE::trait::has_error_code_v<error_info>, "error_info is not detected to be an error code");

//! Choose an errc implementation
using std::errc;

//! Helper for constructing an error info from an errc
inline error_info generic_error(errc c)
{
  return error_info(make_error_code(c));
}
#ifndef _WIN32
//! Helper for constructing an error info from a POSIX errno
inline error_info posix_error(int c = errno)
{
  return error_info(std::error_code(c, std::system_category()));
}
#endif

AFIO_V2_NAMESPACE_END
#endif  // AFIO_EXPERIMENTAL_STATUS_CODE


#if AFIO_LOGGING_LEVEL
#include "quickcpplib/include/ringbuffer_log.hpp"
#include "quickcpplib/include/utils/thread.hpp"

/*! \todo TODO FIXME Replace in-memory log with memory map file backed log.
*/
AFIO_V2_NAMESPACE_BEGIN

//! The log used by AFIO
inline AFIO_DECL QUICKCPPLIB_NAMESPACE::ringbuffer_log::simple_ringbuffer_log<AFIO_LOGGING_MEMORY> &log() noexcept
{
  static QUICKCPPLIB_NAMESPACE::ringbuffer_log::simple_ringbuffer_log<AFIO_LOGGING_MEMORY> _log(static_cast<QUICKCPPLIB_NAMESPACE::ringbuffer_log::level>(AFIO_LOGGING_LEVEL));
#ifdef AFIO_LOG_TO_OSTREAM
  if(_log.immediate() != &AFIO_LOG_TO_OSTREAM)
  {
    _log.immediate(&AFIO_LOG_TO_OSTREAM);
  }
#endif
  return _log;
}
//! Enum for the log level
using log_level = QUICKCPPLIB_NAMESPACE::ringbuffer_log::level;
//! RAII class for temporarily adjusting the log level
class log_level_guard
{
  log_level _v;

public:
  log_level_guard() = delete;
  log_level_guard(const log_level_guard &) = delete;
  log_level_guard(log_level_guard &&) = delete;
  log_level_guard &operator=(const log_level_guard &) = delete;
  log_level_guard &operator=(log_level_guard &&) = delete;
  explicit log_level_guard(log_level n)
      : _v(log().log_level())
  {
    log().log_level(n);
  }
  ~log_level_guard() { log().log_level(_v); }
};

// Infrastructure for recording the current path for when failure occurs
#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
namespace detail
{
  // Our thread local store
  struct tls_errored_results_t
  {
    uint32_t this_thread_id{QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id()};
    handle *current_handle{nullptr};  // The current handle for this thread. Changed via RAII via AFIO_LOG_FUNCTION_CALL, see below.
    bool reentering_self{false};      // Prevents any failed call to current_path() by us reentering ourselves

    char paths[190][16]{};  // Last 190 chars of path
    uint16_t pathidx{0};
    char *next(uint16_t &idx)
    {
      idx = pathidx++;
      return paths[idx % 16];  // NOLINT
    }
    const char *get(uint16_t idx) const
    {
      // If the idx is stale, return not found
      if(idx - pathidx >= 16)
      {
        return nullptr;
      }
      return paths[idx % 16];  // NOLINT
    }
  };
  inline tls_errored_results_t &tls_errored_results()
  {
#if AFIO_THREAD_LOCAL_IS_CXX11
    static thread_local tls_errored_results_t v;
    return v;
#else
    static AFIO_THREAD_LOCAL tls_errored_results_t *v;
    if(!v)
    {
      v = new tls_errored_results_t;
    }
    return *v;
#endif
  }
  template <bool _enabled> struct tls_current_handle_holder
  {
    handle *old{nullptr};
    bool enabled{false};
    tls_current_handle_holder() = delete;
    tls_current_handle_holder(const tls_current_handle_holder &) = delete;
    tls_current_handle_holder(tls_current_handle_holder &&) = delete;
    tls_current_handle_holder &operator=(const tls_current_handle_holder &) = delete;
    tls_current_handle_holder &operator=(tls_current_handle_holder &&) = delete;
    explicit tls_current_handle_holder(const handle *h)
    {
      if(h != nullptr && log().log_level() >= log_level::error)
      {
        auto &tls = tls_errored_results();
        old = tls.current_handle;
        tls.current_handle = const_cast<handle *>(h);  // NOLINT
        enabled = true;
      }
    }
    ~tls_current_handle_holder()
    {
      if(enabled)
      {
        auto &tls = tls_errored_results();
        tls.current_handle = old;
      }
    }
  };
  template <> struct tls_current_handle_holder<false>
  {
    tls_current_handle_holder() = delete;
    tls_current_handle_holder(const tls_current_handle_holder &) = delete;
    tls_current_handle_holder(tls_current_handle_holder &&) = delete;
    tls_current_handle_holder &operator=(const tls_current_handle_holder &) = delete;
    tls_current_handle_holder &operator=(tls_current_handle_holder &&) = delete;
    ~tls_current_handle_holder() = default;
    template <class T> explicit tls_current_handle_holder(T && /*unused*/) {}
  };
#define AFIO_LOG_INST_TO_TLS(inst) AFIO_V2_NAMESPACE::detail::tls_current_handle_holder<std::is_base_of<AFIO_V2_NAMESPACE::handle, std::decay_t<std::remove_pointer_t<decltype(inst)>>>::value> AFIO_UNIQUE_NAME(inst)
}  // namespace detail
#else  // AFIO_DISABLE_PATHS_IN_FAILURE_INFO
#define AFIO_LOG_INST_TO_TLS(inst)
#endif  // AFIO_DISABLE_PATHS_IN_FAILURE_INFO

AFIO_V2_NAMESPACE_END

#ifndef AFIO_LOG_FATAL_TO_CERR
#include <cstdio>
#define AFIO_LOG_FATAL_TO_CERR(expr)                                                                                                                                                                                                                                                                                           \
  fprintf(stderr, "%s\n", (expr));                                                                                                                                                                                                                                                                                             \
  fflush(stderr)
#endif
#endif

#if AFIO_LOGGING_LEVEL >= 1
#define AFIO_LOG_FATAL(inst, message)                                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                            \
    AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::fatal, (message), AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1U << 1U)) ? nullptr : __func__, __LINE__);                  \
    AFIO_LOG_FATAL_TO_CERR(message);                                                                                                                                                                                                                                                                                           \
  }
#else
#define AFIO_LOG_FATAL(inst, message) AFIO_LOG_FATAL_TO_CERR(message)
#endif
#if AFIO_LOGGING_LEVEL >= 2
#define AFIO_LOG_ERROR(inst, message)                                                                                                                                                                                                                                                                                          \
  AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::error, (message), AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1U << 2U)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_ERROR(inst, message)
#endif
#if AFIO_LOGGING_LEVEL >= 3
#define AFIO_LOG_WARN(inst, message)                                                                                                                                                                                                                                                                                           \
  AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::warn, (message), AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1U << 3U)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_WARN(inst, message)
#endif
#if AFIO_LOGGING_LEVEL >= 4
#define AFIO_LOG_INFO(inst, message)                                                                                                                                                                                                                                                                                           \
  AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::info, (message), AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1U << 4U)) ? nullptr : __func__, __LINE__)

// Need to expand out our namespace into a string
#define AFIO_LOG_STRINGIFY9(s) #s "::"
#define AFIO_LOG_STRINGIFY8(s) AFIO_LOG_STRINGIFY9(s)
#define AFIO_LOG_STRINGIFY7(s) AFIO_LOG_STRINGIFY8(s)
#define AFIO_LOG_STRINGIFY6(s) AFIO_LOG_STRINGIFY7(s)
#define AFIO_LOG_STRINGIFY5(s) AFIO_LOG_STRINGIFY6(s)
#define AFIO_LOG_STRINGIFY4(s) AFIO_LOG_STRINGIFY5(s)
#define AFIO_LOG_STRINGIFY3(s) AFIO_LOG_STRINGIFY4(s)
#define AFIO_LOG_STRINGIFY2(s) AFIO_LOG_STRINGIFY3(s)
#define AFIO_LOG_STRINGIFY(s) AFIO_LOG_STRINGIFY2(s)
AFIO_V2_NAMESPACE_BEGIN
namespace detail
{
  // Returns the AFIO namespace as a string
  inline span<char> afio_namespace_string()
  {
    static char buffer[64];
    static size_t length;
    if(length)
      return span<char>(buffer, length);
    const char *src = AFIO_LOG_STRINGIFY(AFIO_V2_NAMESPACE);
    char *bufferp = buffer;
    for(; *src && (bufferp - buffer) < (ptrdiff_t) sizeof(buffer); src++)
    {
      if(*src != ' ')
        *bufferp++ = *src;
    }
    *bufferp = 0;
    length = bufferp - buffer;
    return span<char>(buffer, length);
  }
  // Returns the Outcome namespace as a string
  inline span<char> outcome_namespace_string()
  {
    static char buffer[64];
    static size_t length;
    if(length)
      return span<char>(buffer, length);
    const char *src = AFIO_LOG_STRINGIFY(OUTCOME_V2_NAMESPACE);
    char *bufferp = buffer;
    for(; *src && (bufferp - buffer) < (ptrdiff_t) sizeof(buffer); src++)
    {
      if(*src != ' ')
        *bufferp++ = *src;
    }
    *bufferp = 0;
    length = bufferp - buffer;
    return span<char>(buffer, length);
  }
  // Strips a __PRETTY_FUNCTION__ of all instances of AFIO_V2_NAMESPACE:: and AFIO_V2_NAMESPACE::
  inline void strip_pretty_function(char *out, size_t bytes, const char *in)
  {
    const span<char> remove1 = afio_namespace_string();
    const span<char> remove2 = outcome_namespace_string();
    for(--bytes; bytes && *in; --bytes)
    {
      if(!strncmp(in, remove1.data(), remove1.size()))
        in += remove1.size();
      if(!strncmp(in, remove2.data(), remove2.size()))
        in += remove2.size();
      *out++ = *in++;
    }
    *out = 0;
  }
  template <class T> void log_inst_to_info(T &&inst, const char *buffer) { AFIO_LOG_INFO(inst, buffer); }
}
AFIO_V2_NAMESPACE_END
#ifdef _MSC_VER
#define AFIO_LOG_FUNCTION_CALL(inst)                                                                                                                                                                                                                                                                                           \
  if(log().log_level() >= log_level::info)                                                                                                                                                                                                                                                                                     \
  {                                                                                                                                                                                                                                                                                                                            \
    char buffer[256];                                                                                                                                                                                                                                                                                                          \
    AFIO_V2_NAMESPACE::detail::strip_pretty_function(buffer, sizeof(buffer), __FUNCSIG__);                                                                                                                                                                                                                                     \
    AFIO_V2_NAMESPACE::detail::log_inst_to_info(inst, buffer);                                                                                                                                                                                                                                                                 \
  }                                                                                                                                                                                                                                                                                                                            \
  AFIO_LOG_INST_TO_TLS(inst)
#else
#define AFIO_LOG_FUNCTION_CALL(inst)                                                                                                                                                                                                                                                                                           \
  if(log().log_level() >= log_level::info)                                                                                                                                                                                                                                                                                     \
  {                                                                                                                                                                                                                                                                                                                            \
    char buffer[256];                                                                                                                                                                                                                                                                                                          \
    AFIO_V2_NAMESPACE::detail::strip_pretty_function(buffer, sizeof(buffer), __PRETTY_FUNCTION__);                                                                                                                                                                                                                             \
    AFIO_V2_NAMESPACE::detail::log_inst_to_info(inst, buffer);                                                                                                                                                                                                                                                                 \
  }                                                                                                                                                                                                                                                                                                                            \
  AFIO_LOG_INST_TO_TLS(inst)
#endif
#else
#define AFIO_LOG_INFO(inst, message)
#define AFIO_LOG_FUNCTION_CALL(inst) AFIO_LOG_INST_TO_TLS(inst)
#endif
#if AFIO_LOGGING_LEVEL >= 5
#define AFIO_LOG_DEBUG(inst, message)                                                                                                                                                                                                                                                                                          \
  AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::debug, AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1U << 5U)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_DEBUG(inst, message)
#endif
#if AFIO_LOGGING_LEVEL >= 6
#define AFIO_LOG_ALL(inst, message)                                                                                                                                                                                                                                                                                            \
  AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::all, (message), AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1U << 6U)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_ALL(inst, message)
#endif

AFIO_V2_NAMESPACE_BEGIN

#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
namespace detail
{
  template <class Src> inline void append_path_info(Src &src, std::string &ret)
  {
    if(QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id() == src._thread_id)
    {
      auto &tls = detail::tls_errored_results();
      const char *path1 = tls.get(src._tls_path_id1), *path2 = tls.get(src._tls_path_id2);
      if(path1 != nullptr)
      {
        ret.append(" [path1 = ");
        ret.append(path1);
        if(path2 != nullptr)
        {
          ret.append(", path2 = ");
          ret.append(path2);
        }
        ret.append("]");
      }
    }
#if AFIO_LOGGING_LEVEL >= 2
    if(src._log_id != static_cast<uint32_t>(-1))
    {
      if(log().valid(src._log_id))
      {
        ret.append(" [location = ");
        ret.append(location(log()[src._log_id]));
        ret.append("]");
      }
    }
#endif
  }
}
#endif

#if AFIO_EXPERIMENTAL_STATUS_CODE
#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
template <class BaseStatusCodeDomain> inline typename error_domain<BaseStatusCodeDomain>::string_ref error_domain<BaseStatusCodeDomain>::_message(const SYSTEM_ERROR2_NAMESPACE::status_code<void> &code) const noexcept  // NOLINT
{
  assert(code.domain() == *this);
  const auto &c = static_cast<const SYSTEM_ERROR2_NAMESPACE::status_code<error_domain> &>(code);  // NOLINT
  auto v = c.value();
  std::string ret = v.sc.message();
  detail::append_path_info(v, ret);
  return atomic_refcounted_string_ref(ret.c_str(), ret.size());
}
#endif

#else  // AFIO_EXPERIMENTAL_STATUS_CODE

inline filesystem::path error_info::path1() const
{
#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
  if(QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id() == _thread_id)
  {
    auto &tls = detail::tls_errored_results();
    const char *path1 = tls.get(_tls_path_id1);
    if(path1 != nullptr)
    {
      return filesystem::path(path1);
    }
  }
#endif
  return {};
}
inline filesystem::path error_info::path2() const
{
#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
  if(QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id() == _thread_id)
  {
    auto &tls = detail::tls_errored_results();
    const char *path2 = tls.get(_tls_path_id2);
    if(path2 != nullptr)
    {
      return filesystem::path(path2);
    }
  }
#endif
  return {};
}
inline std::string error_info::message() const
{
  std::string ret(ec.message());
#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
  detail::append_path_info(*this, ret);
#endif
  return ret;
}

#endif  // AFIO_EXPERIMENTAL_STATUS_CODE

AFIO_V2_NAMESPACE_END


#include <ctime>  // for struct timespec

AFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  // A move only capable lightweight std::function, as std::function can't handle move only callables
  template <class F> class function_ptr;
  template <class R, class... Args> class function_ptr<R(Args...)>
  {
    struct function_ptr_storage
    {
      function_ptr_storage() = default;
      function_ptr_storage(const function_ptr_storage &) = delete;
      function_ptr_storage(function_ptr_storage &&) = delete;
      function_ptr_storage &operator=(const function_ptr_storage &) = delete;
      function_ptr_storage &operator=(function_ptr_storage &&) = delete;
      virtual ~function_ptr_storage() = default;
      virtual R operator()(Args &&... args) = 0;
    };
    template <class U> struct function_ptr_storage_impl : public function_ptr_storage
    {
      U c;
      template <class... Args2>
      constexpr explicit function_ptr_storage_impl(Args2 &&... args)
          : c(std::forward<Args2>(args)...)
      {
      }
      R operator()(Args &&... args) final { return c(std::move(args)...); }
    };
    function_ptr_storage *ptr;
    template <class U> struct emplace_t
    {
    };
    template <class U, class V> friend inline function_ptr<U> make_function_ptr(V &&f);  // NOLINT
    template <class U>
    explicit function_ptr(std::nullptr_t, U &&f)
        : ptr(new function_ptr_storage_impl<typename std::decay<U>::type>(std::forward<U>(f)))
    {
    }
    template <class R_, class U, class... Args2> friend inline function_ptr<R_> emplace_function_ptr(Args2 &&... args);  // NOLINT
    template <class U, class... Args2>
    explicit function_ptr(emplace_t<U> /*unused*/, Args2 &&... args)
        : ptr(new function_ptr_storage_impl<U>(std::forward<Args2>(args)...))
    {
    }

  public:
    constexpr function_ptr() noexcept : ptr(nullptr) {}
    constexpr explicit function_ptr(function_ptr_storage *p) noexcept : ptr(p) {}
    constexpr function_ptr(function_ptr &&o) noexcept : ptr(o.ptr) { o.ptr = nullptr; }
    function_ptr &operator=(function_ptr &&o) noexcept
    {
      delete ptr;
      ptr = o.ptr;
      o.ptr = nullptr;
      return *this;
    }
    function_ptr(const function_ptr &) = delete;
    function_ptr &operator=(const function_ptr &) = delete;
    ~function_ptr() { delete ptr; }
    explicit constexpr operator bool() const noexcept { return !!ptr; }
    constexpr R operator()(Args... args) const { return (*ptr)(std::move(args)...); }
    constexpr function_ptr_storage *get() noexcept { return ptr; }
    constexpr void reset(function_ptr_storage *p = nullptr) noexcept
    {
      delete ptr;
      ptr = p;
    }
    constexpr function_ptr_storage *release() noexcept
    {
      auto p = ptr;
      ptr = nullptr;
      return p;
    }
  };
  template <class R, class U> inline function_ptr<R> make_function_ptr(U &&f) { return function_ptr<R>(nullptr, std::forward<U>(f)); }
  template <class R, class U, class... Args> inline function_ptr<R> emplace_function_ptr(Args &&... args) { return function_ptr<R>(typename function_ptr<R>::template emplace_t<U>(), std::forward<Args>(args)...); }
}  // namespace detail

// Native handle support
namespace win
{
  using handle = void *;
  using dword = unsigned long;
}

AFIO_V2_NAMESPACE_END


#if 0
///////////////////////////////////////////////////////////////////////////////
//  Auto library naming
#if !defined(AFIO_SOURCE) && !defined(BOOST_ALL_NO_LIB) && !defined(AFIO_NO_LIB) && !AFIO_STANDALONE && !AFIO_HEADERS_ONLY

#define BOOST_LIB_NAME boost_afio

// tell the auto-link code to select a dll when required:
#if defined(BOOST_ALL_DYN_LINK) || defined(AFIO_DYN_LINK)
#define BOOST_DYN_LINK
#endif

#include <boost/config/auto_link.hpp>

#endif  // auto-linking disabled
#endif

//#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
//#define BOOST_THREAD_DONT_PROVIDE_FUTURE
//#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#if AFIO_HEADERS_ONLY == 1 && !defined(AFIO_SOURCE)
/*! \brief Expands into the appropriate markup to declare an `extern`
function exported from the AFIO DLL if not building headers only.
\ingroup config
*/
#define AFIO_HEADERS_ONLY_FUNC_SPEC inline
/*! \brief Expands into the appropriate markup to declare a class member
function exported from the AFIO DLL if not building headers only.
\ingroup config
*/
#define AFIO_HEADERS_ONLY_MEMFUNC_SPEC inline
/*! \brief Expands into the appropriate markup to declare a virtual class member
function exported from the AFIO DLL if not building headers only.
\ingroup config
*/
#define AFIO_HEADERS_ONLY_VIRTUAL_SPEC inline virtual
#else
#define AFIO_HEADERS_ONLY_FUNC_SPEC extern AFIO_DECL
#define AFIO_HEADERS_ONLY_MEMFUNC_SPEC
#define AFIO_HEADERS_ONLY_VIRTUAL_SPEC virtual
#endif

#endif
