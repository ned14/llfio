/* LLFIO logging
(C) 2015-2020 Niall Douglas <http://www.nedproductions.biz/> (24 commits)
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

#ifndef LLFIO_LOGGING_HPP
#define LLFIO_LOGGING_HPP

#include "config.hpp"

#if LLFIO_LOGGING_LEVEL

/*! \todo TODO FIXME Replace in-memory log with memory map file backed log.
 */
LLFIO_V2_NAMESPACE_BEGIN

//! Enum for the log level
using log_level = QUICKCPPLIB_NAMESPACE::ringbuffer_log::level;

namespace detail
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC char &thread_local_log_level();
  struct thread_local_log_level_filter
  {
    log_level operator()(log_level v) const { return std::min(v, (log_level) thread_local_log_level()); }
  };
}  // namespace detail

//! Type of the logger
using log_implementation_type = QUICKCPPLIB_NAMESPACE::ringbuffer_log::simple_ringbuffer_log<LLFIO_LOGGING_MEMORY, detail::thread_local_log_level_filter>;

//! The log used by LLFIO
inline LLFIO_DECL log_implementation_type &log() noexcept
{
  static log_implementation_type _log(static_cast<QUICKCPPLIB_NAMESPACE::ringbuffer_log::level>(LLFIO_LOGGING_LEVEL));
#ifdef LLFIO_LOG_TO_OSTREAM
  if(_log.immediate() != &LLFIO_LOG_TO_OSTREAM)
  {
    _log.immediate(&LLFIO_LOG_TO_OSTREAM);
  }
#endif
  return _log;
}

//! RAII class for temporarily adjusting the log level for the current thread
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
      : _v((log_level) detail::thread_local_log_level())
  {
    reinterpret_cast<log_level &>(detail::thread_local_log_level()) = n;
  }
  ~log_level_guard() { reinterpret_cast<log_level &>(detail::thread_local_log_level()) = _v; }
};

// Infrastructure for recording the current path for when failure occurs
#ifndef LLFIO_DISABLE_PATHS_IN_FAILURE_INFO
namespace detail
{
  // Our thread local store
  struct tls_errored_results_t
  {
    uint32_t this_thread_id{QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id()};
    handle *current_handle{nullptr};  // The current handle for this thread. Changed via RAII via LLFIO_LOG_FUNCTION_CALL, see below.
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
#if LLFIO_THREAD_LOCAL_IS_CXX11
    static thread_local tls_errored_results_t v;
    return v;
#else
    static LLFIO_THREAD_LOCAL tls_errored_results_t *v;
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
#define LLFIO_LOG_INST_TO_TLS(inst)                                                                                                                            \
  ::LLFIO_V2_NAMESPACE::detail::tls_current_handle_holder<                                                                                                     \
  std::is_base_of<::LLFIO_V2_NAMESPACE::handle, std::decay_t<std::remove_pointer_t<decltype(inst)>>>::value>                                                   \
  LLFIO_UNIQUE_NAME(inst)
}  // namespace detail
#else  // LLFIO_DISABLE_PATHS_IN_FAILURE_INFO
#define LLFIO_LOG_INST_TO_TLS(inst)
#endif  // LLFIO_DISABLE_PATHS_IN_FAILURE_INFO

LLFIO_V2_NAMESPACE_END

#ifndef LLFIO_LOG_FATAL_TO_CERR
#include <cstdio>
#define LLFIO_LOG_FATAL_TO_CERR(expr)                                                                                                                          \
  fprintf(stderr, "%s\n", (expr));                                                                                                                             \
  fflush(stderr)
#endif
#endif  // LLFIO_LOGGING_LEVEL

#if LLFIO_LOGGING_LEVEL >= 1
#define LLFIO_LOG_FATAL(inst, message)                                                                                                                         \
  {                                                                                                                                                            \
    ::LLFIO_V2_NAMESPACE::log().emplace_back(                                                                                                                  \
    QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::fatal, (message), ::LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst),                       \
    QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (LLFIO_LOG_BACKTRACE_LEVELS & (1U << 1U)) ? nullptr : __func__, __LINE__);                         \
    LLFIO_LOG_FATAL_TO_CERR(message);                                                                                                                          \
  }
#else
#define LLFIO_LOG_FATAL(inst, message) LLFIO_LOG_FATAL_TO_CERR(message)
#endif
#if LLFIO_LOGGING_LEVEL >= 2
#define LLFIO_LOG_ERROR(inst, message)                                                                                                                         \
  ::LLFIO_V2_NAMESPACE::log().emplace_back(                                                                                                                    \
  QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::error, (message), ::LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst),                         \
  QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (LLFIO_LOG_BACKTRACE_LEVELS & (1U << 2U)) ? nullptr : __func__, __LINE__)
#else
#define LLFIO_LOG_ERROR(inst, message)
#endif
#if LLFIO_LOGGING_LEVEL >= 3
#define LLFIO_LOG_WARN(inst, message)                                                                                                                          \
  ::LLFIO_V2_NAMESPACE::log().emplace_back(                                                                                                                    \
  QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::warn, (message), ::LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst),                          \
  QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (LLFIO_LOG_BACKTRACE_LEVELS & (1U << 3U)) ? nullptr : __func__, __LINE__)
#else
#define LLFIO_LOG_WARN(inst, message)
#endif
#if LLFIO_LOGGING_LEVEL >= 4
#define LLFIO_LOG_INFO(inst, message)                                                                                                                          \
  ::LLFIO_V2_NAMESPACE::log().emplace_back(                                                                                                                    \
  QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::info, (message), ::LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst),                          \
  QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (LLFIO_LOG_BACKTRACE_LEVELS & (1U << 4U)) ? nullptr : __func__, __LINE__)

// Need to expand out our namespace into a string
#define LLFIO_LOG_STRINGIFY9(s) #s "::"
#define LLFIO_LOG_STRINGIFY8(s) LLFIO_LOG_STRINGIFY9(s)
#define LLFIO_LOG_STRINGIFY7(s) LLFIO_LOG_STRINGIFY8(s)
#define LLFIO_LOG_STRINGIFY6(s) LLFIO_LOG_STRINGIFY7(s)
#define LLFIO_LOG_STRINGIFY5(s) LLFIO_LOG_STRINGIFY6(s)
#define LLFIO_LOG_STRINGIFY4(s) LLFIO_LOG_STRINGIFY5(s)
#define LLFIO_LOG_STRINGIFY3(s) LLFIO_LOG_STRINGIFY4(s)
#define LLFIO_LOG_STRINGIFY2(s) LLFIO_LOG_STRINGIFY3(s)
#define LLFIO_LOG_STRINGIFY(s) LLFIO_LOG_STRINGIFY2(s)
LLFIO_V2_NAMESPACE_BEGIN
namespace detail
{
  // Returns the LLFIO namespace as a string
  inline span<char> llfio_namespace_string()
  {
    static char buffer[64];
    static size_t length;
    if(length)
      return span<char>(buffer, length);
    const char *src = LLFIO_LOG_STRINGIFY(LLFIO_V2_NAMESPACE);
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
    const char *src = LLFIO_LOG_STRINGIFY(OUTCOME_V2_NAMESPACE);
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
  // Strips a __PRETTY_FUNCTION__ of all instances of ::LLFIO_V2_NAMESPACE:: and ::LLFIO_V2_NAMESPACE::
  inline void strip_pretty_function(char *_out, size_t bytes, const char *in)
  {
    char *out = _out;
    const span<char> remove1 = llfio_namespace_string();
    const span<char> remove2 = outcome_namespace_string();
    for(--bytes; bytes && *in; --bytes)
    {
      if(!strncmp(in, remove1.data(), remove1.size()))
        in += remove1.size();
      if(!strncmp(in, remove2.data(), remove2.size()))
        in += remove2.size();
      if(!strncmp(in, "basic_result<", 13))
      {
        int count = 13;
        for(--bytes; bytes && *in && count; --bytes, --count)
        {
          *out++ = *in++;
        }
        if(!*in || bytes ==0)
        {
          break;
        }
        count = 1;
        while(*in && count > 0)
        {
          if(*in == '<')
          {
            count++;
          }
          else if(*in == '>')
          {
            count--;
          }
          in++;
        }
        in--;
      }
      *out++ = *in++;
    }
    *out = 0;
  }
  template <class T> void log_inst_to_info(T &&inst, const char *buffer) { LLFIO_LOG_INFO(inst, buffer); }
}  // namespace detail
LLFIO_V2_NAMESPACE_END
#ifdef _MSC_VER
#define LLFIO_LOG_FUNCTION_CALL(inst)                                                                                                                          \
  if(log().log_level() >= log_level::info)                                                                                                                     \
  {                                                                                                                                                            \
    char buffer[256];                                                                                                                                          \
    ::LLFIO_V2_NAMESPACE::detail::strip_pretty_function(buffer, sizeof(buffer), __FUNCSIG__);                                                                  \
    ::LLFIO_V2_NAMESPACE::detail::log_inst_to_info(inst, buffer);                                                                                              \
  }                                                                                                                                                            \
  LLFIO_LOG_INST_TO_TLS(inst)
#else
#define LLFIO_LOG_FUNCTION_CALL(inst)                                                                                                                          \
  if(log().log_level() >= log_level::info)                                                                                                                     \
  {                                                                                                                                                            \
    char buffer[256];                                                                                                                                          \
    ::LLFIO_V2_NAMESPACE::detail::strip_pretty_function(buffer, sizeof(buffer), __PRETTY_FUNCTION__);                                                          \
    ::LLFIO_V2_NAMESPACE::detail::log_inst_to_info(inst, buffer);                                                                                              \
  }                                                                                                                                                            \
  LLFIO_LOG_INST_TO_TLS(inst)
#endif
#else
#define LLFIO_LOG_INFO(inst, message)
#define LLFIO_LOG_FUNCTION_CALL(inst) LLFIO_LOG_INST_TO_TLS(inst)
#endif
#if LLFIO_LOGGING_LEVEL >= 5
#define LLFIO_LOG_DEBUG(inst, message)                                                                                                                         \
  ::LLFIO_V2_NAMESPACE::log().emplace_back(                                                                                                                    \
  QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::debug, ::LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst),                                    \
  QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (LLFIO_LOG_BACKTRACE_LEVELS & (1U << 5U)) ? nullptr : __func__, __LINE__)
#else
#define LLFIO_LOG_DEBUG(inst, message)
#endif
#if LLFIO_LOGGING_LEVEL >= 6
#define LLFIO_LOG_ALL(inst, message)                                                                                                                           \
  ::LLFIO_V2_NAMESPACE::log().emplace_back(                                                                                                                    \
  QUICKCPPLIB_NAMESPACE::ringbuffer_log::level::all, (message), ::LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<unsigned>(inst),                           \
  QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id(), (LLFIO_LOG_BACKTRACE_LEVELS & (1U << 6U)) ? nullptr : __func__, __LINE__)
#else
#define LLFIO_LOG_ALL(inst, message)
#endif


#if !LLFIO_EXPERIMENTAL_STATUS_CODE
#ifndef LLFIO_DISABLE_PATHS_IN_FAILURE_INFO
LLFIO_V2_NAMESPACE_BEGIN

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
#if LLFIO_LOGGING_LEVEL >= 2
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
}  // namespace detail

LLFIO_V2_NAMESPACE_END
#endif
#endif

#endif
