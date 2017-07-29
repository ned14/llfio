/* Configures AFIO
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (24 commits)
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
//#define AFIO_LOGGING_LEVEL 3

//! \file config.hpp Configures a compiler environment for AFIO header and source code

//! \defgroup config Configuration macros

#if !defined(AFIO_HEADERS_ONLY) && !defined(BOOST_ALL_DYN_LINK)
//! \brief Whether AFIO is a headers only library. Defaults to 1 unless BOOST_ALL_DYN_LINK is defined. \ingroup config
#define AFIO_HEADERS_ONLY 1
#endif

#if !defined(AFIO_LOGGING_LEVEL)
#ifdef NDEBUG
#define AFIO_LOGGING_LEVEL 2  // error
#else
//! \brief How much detail to log. 0=disabled, 1=fatal, 2=error, 3=warn, 4=info, 5=debug, 6=all.
//! Defaults to error if NDEBUG defined, else info level. \ingroup config
#define AFIO_LOGGING_LEVEL 4  // info
#endif
#endif

#if !defined(AFIO_LOG_BACKTRACE_LEVELS)
//! \brief Bit mask of which log levels should be stack backtraced
//! which will slow those logs thirty fold or so. Defaults to (1<<1)|(1<<2)|(1<<3) i.e. stack backtrace
//! on fatal, error and warn logs. \ingroup config
#define AFIO_LOG_BACKTRACE_LEVELS ((1 << 1) | (1 << 2) | (1 << 3))
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

#if !defined(AFIO_DISABLE_RACE_FREE_PATH_FUNCTIONS)
#ifdef __APPLE__
#define AFIO_DISABLE_RACE_FREE_PATH_FUNCTIONS 1
#endif
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

// Pull in detection of __MINGW64_VERSION_MAJOR
#ifdef __MINGW32__
#include <_mingw.h>
#endif

#include "../quickcpplib/include/cpp_feature.h"

#ifndef __cpp_exceptions
#error AFIO needs C++ exceptions to be turned on
#endif
#ifndef __cpp_alias_templates
#error AFIO needs template alias support in the compiler
#endif
#ifndef __cpp_variadic_templates
#error AFIO needs variadic template support in the compiler
#endif
#ifndef __cpp_constexpr
#error AFIO needs constexpr (C++ 11) support in the compiler
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


#include "../quickcpplib/include/import.h"

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
#include "../quickcpplib/include/config.hpp"
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
#ifdef __has_feature
#if __has_feature(cxx_thread_local)
#define AFIO_THREAD_LOCAL_IS_CXX11 1
#endif
#elif defined(_MSC_VER)
#define AFIO_THREAD_LOCAL_IS_CXX11 1
#endif
#ifndef AFIO_THREAD_LOCAL_IS_CXX11
#define AFIO_THREAD_LOCAL_IS_CXX11 0
#define AFIO_THREAD_LOCAL QUICKCPPLIB_THREAD_LOCAL
#endif
#endif

#define AFIO_GLUE2(x, y) x##y
#define AFIO_GLUE(x, y) AFIO_GLUE2(x, y)
#define AFIO_UNIQUE_NAME AFIO_GLUE(__t, __COUNTER__)


// Bring in bitfields
#include "../quickcpplib/include/bitfield.hpp"
// Bring in scoped undo
#include "../quickcpplib/include/scoped_undo.hpp"
AFIO_V2_NAMESPACE_BEGIN
using QUICKCPPLIB_NAMESPACE::scoped_undo::undoer;
AFIO_V2_NAMESPACE_END
// Bring in a span implementation
#include "../quickcpplib/include/span.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::span;
AFIO_V2_NAMESPACE_END
// Bring in an optional implementation
#include "../quickcpplib/include/optional.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::optional;
AFIO_V2_NAMESPACE_END
// Bring in a string_view implementation
#include "../quickcpplib/include/string_view.hpp"
AFIO_V2_NAMESPACE_BEGIN
using namespace QUICKCPPLIB_NAMESPACE::string_view;
AFIO_V2_NAMESPACE_END
// Bring in a result implementation
#include "../outcome/include/outcome.hpp"
AFIO_V2_NAMESPACE_BEGIN
// Specialise error_code into this namespace so we can hook result creation via ADL
/*! \struct error_code
\brief Trampoline to `std::error_code`, used to ADL hook `result<T, E>` creation in Outcome.
*/
struct error_code : public std::error_code
{
  // literally passthrough
  using std::error_code::error_code;
  error_code() = default;
  error_code(std::error_code ec)  // NOLINT
  : std::error_code(ec)
  {
  }
};
template <class T> using result = OUTCOME_V2_NAMESPACE::result<T, error_code>;
template <class T> using outcome = OUTCOME_V2_NAMESPACE::outcome<T, error_code>;
using OUTCOME_V2_NAMESPACE::success;
using OUTCOME_V2_NAMESPACE::failure;
using OUTCOME_V2_NAMESPACE::error_from_exception;
using OUTCOME_V2_NAMESPACE::in_place_type;
AFIO_V2_NAMESPACE_END


#if AFIO_LOGGING_LEVEL
#include "../quickcpplib/include/ringbuffer_log.hpp"
#include "../quickcpplib/include/utils/thread.hpp"

/*! \todo TODO FIXME Replace in-memory log with memory map file backed log.
*/
AFIO_V2_NAMESPACE_BEGIN

//! The log used by AFIO
inline AFIO_DECL QUICKCPPLIB_NAMESPACE::ringbuffer_log::simple_ringbuffer_log<AFIO_LOGGING_MEMORY> &log() noexcept
{
  static QUICKCPPLIB_NAMESPACE::ringbuffer_log::simple_ringbuffer_log<AFIO_LOGGING_MEMORY> _log(static_cast<QUICKCPPLIB_NAMESPACE::ringbuffer_log::level>(AFIO_LOGGING_LEVEL));
#ifdef AFIO_LOG_TO_OSTREAM
  _log.immediate(&AFIO_LOG_TO_OSTREAM);
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
  log_level_guard(log_level n)
      : _v(log().log_level())
  {
    log().log_level(n);
  }
  ~log_level_guard() { log().log_level(_v); }
};

/* Some notes on how error logging works:

Throughout AFIO in almost all of its functions, we call `AFIO_LOG_FUNCTION_CALL(inst)` where `inst` is
usually `this`. We test if inst points to a `handle`, and if so we replace the "current handle" in
TLS storage with the new one, restoring it on stack unwind.

When an errored result is created, Outcome provides ADL based hooks which let us intercept an errored
result creation. We look up TLS storage for the current handle, and if one is set then we fetch its
current path and store it into a round robin store in TLS, storing the index into the result. Later
on, when we are reporting or otherwise dealing with the error, the path the error refers to can be
printed.

All this gets compiled out if AFIO_LOGGING_LEVEL < 2 (error) and is skipped if log().log_level() < 2 (error).
In this situation, no paths are captured on error.
*/
#if AFIO_LOGGING_LEVEL >= 2
namespace detail
{
  // Our thread local store
  struct tls_errored_results_t
  {
    handle *current_handle{nullptr};  // The current handle for this thread
    char paths[190][16];              // The log can only store 190 chars of message anyway
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
    tls_current_handle_holder(const handle *h)
    {
      if(h != nullptr && log().log_level() >= log_level::error)
      {
        auto &tls = tls_errored_results();
        old = tls.current_handle;
        tls.current_handle = const_cast<handle *>(h);
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
    template <class T> tls_current_handle_holder(T &&) {}
  };
#define AFIO_LOG_INST_TO_TLS(inst) AFIO_V2_NAMESPACE::detail::tls_current_handle_holder<std::is_base_of<AFIO_V2_NAMESPACE::handle, std::decay_t<std::remove_pointer_t<decltype(inst)>>>::value> AFIO_UNIQUE_NAME(inst)
}
#else
#define AFIO_LOG_INST_TO_TLS(inst)
#endif
AFIO_V2_NAMESPACE_END

#ifndef AFIO_LOG_FATAL_TO_CERR
#include <stdio.h>
#define AFIO_LOG_FATAL_TO_CERR(expr)                                                                                                                                                                                                                                                                                           \
  fprintf(stderr, "%s\n", (expr));                                                                                                                                                                                                                                                                                             \
  fflush(stderr)
#endif
#endif

#if AFIO_LOGGING_LEVEL >= 1
#define AFIO_LOG_FATAL(inst, message)                                                                                                                                                                                                                                                                                          \
  {                                                                                                                                                                                                                                                                                                                            \
    AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::fatal, (message), (unsigned) (uintptr_t)(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1 << 1)) ? nullptr : __func__, __LINE__);                                                        \
    AFIO_LOG_FATAL_TO_CERR(message);                                                                                                                                                                                                                                                                                           \
  }
#else
#define AFIO_LOG_FATAL(inst, message) AFIO_LOG_FATAL_TO_CERR(message)
#endif
#if AFIO_LOGGING_LEVEL >= 2
#define AFIO_LOG_ERROR(inst, message) AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::error, (message), (unsigned) (uintptr_t)(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1 << 2)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_ERROR(inst, message)
#endif
#if AFIO_LOGGING_LEVEL >= 3
#define AFIO_LOG_WARN(inst, message) AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::warn, (message), (unsigned) (uintptr_t)(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1 << 3)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_WARN(inst, message)
#endif
#if AFIO_LOGGING_LEVEL >= 4
#define AFIO_LOG_INFO(inst, message) AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::info, (message), (unsigned) (uintptr_t)(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1 << 4)) ? nullptr : __func__, __LINE__)

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
#define AFIO_LOG_DEBUG(inst, message) AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::debug, (message), (unsigned) (uintptr_t)(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1 << 5)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_DEBUG(inst, message)
#endif
#if AFIO_LOGGING_LEVEL >= 6
#define AFIO_LOG_ALL(inst, message) AFIO_V2_NAMESPACE::log().emplace_back(QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::all, (message), (unsigned) (uintptr_t)(inst), QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (AFIO_LOG_BACKTRACE_LEVELS & (1 << 6)) ? nullptr : __func__, __LINE__)
#else
#define AFIO_LOG_ALL(inst, message)
#endif

#include <time.h>  // for struct timespec

AFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  // A move only capable lightweight std::function, as std::function can't handle move only callables
  template <class F> class function_ptr;
  template <class R, class... Args> class function_ptr<R(Args...)>
  {
    struct function_ptr_storage
    {
      virtual ~function_ptr_storage() {}
      virtual R operator()(Args &&... args) = 0;
    };
    template <class U> struct function_ptr_storage_impl : public function_ptr_storage
    {
      U c;
      template <class... Args2>
      constexpr function_ptr_storage_impl(Args2 &&... args)
          : c(std::forward<Args2>(args)...)
      {
      }
      virtual R operator()(Args &&... args) override final { return c(std::move(args)...); }
    };
    function_ptr_storage *ptr;
    template <class U> struct emplace_t
    {
    };
    template <class U, class V> friend inline function_ptr<U> make_function_ptr(V &&f);
    template <class U>
    explicit function_ptr(std::nullptr_t, U &&f)
        : ptr(new function_ptr_storage_impl<typename std::decay<U>::type>(std::forward<U>(f)))
    {
    }
    template <class R_, class U, class... Args2> friend inline function_ptr<R_> emplace_function_ptr(Args2 &&... args);
    template <class U, class... Args2>
    explicit function_ptr(emplace_t<U>, Args2 &&... args)
        : ptr(new function_ptr_storage_impl<U>(std::forward<Args2>(args)...))
    {
    }

  public:
    constexpr function_ptr() noexcept : ptr(nullptr) {}
    constexpr function_ptr(function_ptr_storage *p) noexcept : ptr(p) {}
    QUICKCPPLIB_CONSTEXPR function_ptr(function_ptr &&o) noexcept : ptr(o.ptr) { o.ptr = nullptr; }
    function_ptr &operator=(function_ptr &&o)
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
    QUICKCPPLIB_CONSTEXPR R operator()(Args... args) const { return (*ptr)(std::move(args)...); }
    QUICKCPPLIB_CONSTEXPR function_ptr_storage *get() noexcept { return ptr; }
    QUICKCPPLIB_CONSTEXPR void reset(function_ptr_storage *p = nullptr) noexcept
    {
      delete ptr;
      ptr = p;
    }
    QUICKCPPLIB_CONSTEXPR function_ptr_storage *release() noexcept
    {
      auto p = ptr;
      ptr = nullptr;
      return p;
    }
  };
  template <class R, class U> inline function_ptr<R> make_function_ptr(U &&f) { return function_ptr<R>(nullptr, std::forward<U>(f)); }
  template <class R, class U, class... Args> inline function_ptr<R> emplace_function_ptr(Args &&... args) { return function_ptr<R>(typename function_ptr<R>::template emplace_t<U>(), std::forward<Args>(args)...); }
}

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
