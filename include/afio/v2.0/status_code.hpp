/* Configures AFIO
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (24 commits)
File Created: June 2018


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

#ifndef AFIO_STATUS_CODE_HPP
#define AFIO_STATUS_CODE_HPP

#include "logging.hpp"

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
    T sc{};

    // The id of the thread where this failure occurred
    uint32_t _thread_id{0};
    // The TLS path store entry
    uint16_t _tls_path_id1{static_cast<uint16_t>(-1)}, _tls_path_id2{static_cast<uint16_t>(-1)};
    // The id of the relevant log entry in the AFIO log (if logging enabled)
    size_t _log_id{static_cast<size_t>(-1)};

    //! Default construction
    error_domain_value_type() = default;

    //! Implicitly constructs an instance
    constexpr inline error_domain_value_type(T _sc)
        : sc(_sc)
    {
    }  // NOLINT

    //! Compares to a T
    constexpr bool operator==(const T &b) const noexcept { return sc == b; }
  };
}

/*! \class error_domain
\brief The SG14 status code domain for errors in AFIO.
*/
template <class BaseStatusCodeDomain> class error_domain : public BaseStatusCodeDomain
{
  friend class SYSTEM_ERROR2_NAMESPACE::status_code<error_domain>;
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
  virtual inline string_ref _do_message(const SYSTEM_ERROR2_NAMESPACE::status_code<void> &code) const noexcept override final
  {
    assert(code.domain() == *this);
    const auto &v = static_cast<const SYSTEM_ERROR2_NAMESPACE::status_code<error_domain> &>(code);  // NOLINT
    std::string ret = _base::_do_message(code).c_str();
    detail::append_path_info(v.value(), ret);
    char *p = (char *) malloc(ret.size() + 1);
    if(p == nullptr)
    {
      return string_ref("Failed to allocate memory to store error string");
    }
    memcpy(p, ret.c_str(), ret.size() + 1);
    return atomic_refcounted_string_ref(p, ret.size());
  }
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
inline error_code generic_error(errc c);
#ifndef _WIN32
//! Helper for constructing an error code from a POSIX errno
inline error_code posix_error(int c = errno);
#else
//! Helper for constructing an error code from a DWORD
inline error_code win32_error(SYSTEM_ERROR2_NAMESPACE::win32::DWORD c = SYSTEM_ERROR2_NAMESPACE::win32::GetLastError());
//! Helper for constructing an error code from a NTSTATUS
inline error_code ntkernel_error(SYSTEM_ERROR2_NAMESPACE::win32::NTSTATUS c);
#endif

namespace detail
{
  inline std::ostream &operator<<(std::ostream &s, const error_code &v) { return s << "afio::error_code(" << v.message().c_str() << ")"; }
}
inline error_code error_from_exception(std::exception_ptr &&ep = std::current_exception(), error_code not_matched = generic_error(errc::resource_unavailable_try_again)) noexcept
{
  if(!ep)
  {
    return generic_error(errc::success);
  }
  try
  {
    std::rethrow_exception(ep);
  }
  catch(const std::invalid_argument & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::invalid_argument);
  }
  catch(const std::domain_error & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::argument_out_of_domain);
  }
  catch(const std::length_error & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::argument_list_too_long);
  }
  catch(const std::out_of_range & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::result_out_of_range);
  }
  catch(const std::logic_error & /*unused*/) /* base class for this group */
  {
    ep = std::exception_ptr();
    return generic_error(errc::invalid_argument);
  }
  catch(const std::system_error &e) /* also catches ios::failure */
  {
    ep = std::exception_ptr();
    if(e.code().category() == std::generic_category())
    {
      return generic_error(static_cast<errc>(static_cast<int>(e.code().value())));
    }
    // Don't know this error code category, so fall through
  }
  catch(const std::overflow_error & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::value_too_large);
  }
  catch(const std::range_error & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::result_out_of_range);
  }
  catch(const std::runtime_error & /*unused*/) /* base class for this group */
  {
    ep = std::exception_ptr();
    return generic_error(errc::resource_unavailable_try_again);
  }
  catch(const std::bad_alloc & /*unused*/)
  {
    ep = std::exception_ptr();
    return generic_error(errc::not_enough_memory);
  }
  catch(...)
  {
  }
  return not_matched;
}

AFIO_V2_NAMESPACE_END


#else  // AFIO_EXPERIMENTAL_STATUS_CODE


// Bring in a result implementation based on std::error_code
#include "outcome/include/outcome.hpp"

AFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  template <class Dest, class Src> inline void fill_failure_info(Dest &dest, const Src &src);
}

struct error_info;
inline std::error_code make_error_code(error_info ei);

/*! \struct error_info
\brief The cause of the failure of an operation in AFIO.
*/
struct error_info
{
  friend inline std::error_code make_error_code(error_info ei);
  template <class Src> friend inline void detail::append_path_info(Src &src, std::string &ret);
  template <class Dest, class Src> friend inline void detail::fill_failure_info(Dest &dest, const Src &src);

private:
  // The error code for the failure
  std::error_code ec;

#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
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

  //! Retrieve the value of the error code
  int value() const noexcept { return ec.value(); }
  //! Retrieve any first path associated with this failure. Note this only works if called from the same thread as where the failure occurred.
  inline filesystem::path path1() const
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
  //! Retrieve any second path associated with this failure. Note this only works if called from the same thread as where the failure occurred.
  inline filesystem::path path2() const
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
  //! Retrieve a descriptive message for this failure, possibly with paths and stack backtraces. Extra detail only appears if called from the same thread as where the failure occurred.
  inline std::string message() const
  {
    std::string ret(ec.message());
#ifndef AFIO_DISABLE_PATHS_IN_FAILURE_INFO
    detail::append_path_info(*this, ret);
#endif
    return ret;
  }
  /*! Throw this failure as a C++ exception. Firstly if the error code matches any of the standard
  C++ exception types e.g. `bad_alloc`, we throw those types using the string from `message()`
  where possible. We then will throw an `error` exception type.
  */
  inline void throw_exception() const;
};
inline bool operator==(const error_info &a, const error_info &b)
{
  return make_error_code(a) == make_error_code(b);
}
inline bool operator!=(const error_info &a, const error_info &b)
{
  return make_error_code(a) != make_error_code(b);
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator==(const error_info &a, const ErrorCondEnum &b)
{
  return make_error_code(a) == std::error_condition(b);
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator==(const ErrorCondEnum &a, const error_info &b)
{
  return std::error_condition(a) == make_error_code(b);
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator!=(const error_info &a, const ErrorCondEnum &b)
{
  return make_error_code(a) != std::error_condition(b);
}
OUTCOME_TEMPLATE(class ErrorCondEnum)
OUTCOME_TREQUIRES(OUTCOME_TPRED(std::is_error_condition_enum<ErrorCondEnum>::value))
inline bool operator!=(const ErrorCondEnum &a, const error_info &b)
{
  return std::error_condition(a) != make_error_code(b);
}
#ifndef NDEBUG
// Is trivial in all ways, except default constructibility
static_assert(std::is_trivially_copyable<error_info>::value, "error_info is not a trivially copyable!");
#endif
inline std::ostream &operator<<(std::ostream &s, const error_info &v)
{
  if(make_error_code(v))
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
// Tell Outcome to call error_info::throw_exception() on no-value observation
inline void outcome_throw_as_system_error_with_payload(const error_info &ei)
{
  ei.throw_exception();
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
      : filesystem::filesystem_error(_ei.message(), _ei.path1(), _ei.path2(), make_error_code(_ei))
      , ei(_ei)
  {
  }
};

inline void error_info::throw_exception() const
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


#endif
