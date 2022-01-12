/* A source of byte-orientated socket handles
(C) 2021-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Dec 2021


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

#ifndef LLFIO_BYTE_SOCKET_SOURCE_H
#define LLFIO_BYTE_SOCKET_SOURCE_H

#include "byte_socket_handle.hpp"

#include <memory>  // for shared_ptr

//! \file byte_socket_source.hpp Provides a source of `byte_socket_handle`.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nameless struct/union
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class byte_socket_source;

namespace detail
{
  struct byte_socket_source_handle_deleter
  {
    void operator()(byte_socket_handle *p) const { p->_deleter(); }
  };
  struct LLFIO_DECL listening_socket_source_handle_deleter
  {
    void operator()(listening_socket_handle *p) const { p->_deleter(); }
  };
}  // namespace detail

//! A pointer to a byte socket handle returned by a byte socket source
using byte_socket_source_handle_ptr = std::unique_ptr<byte_socket_handle, detail::byte_socket_source_handle_deleter>;
//! A pointer to a listening socket handle returned by a byte socket source
using listening_socket_source_handle_ptr = std::unique_ptr<listening_socket_handle, detail::listening_socket_source_handle_deleter>;

//! Feature bits for byte socket sources
QUICKCPPLIB_BITFIELD_BEGIN_T(byte_socket_source_implementation_features, uint32_t){
none = 0U,

kernel_sockets =
(1U << 0U),  //!< This socket source provides kernel sockets i.e. their native handles will be valid kernel sockets which can be used in `poll()` etc
tls_sockets = (1U << 1U),             //!< This socket source provides TLS secured sockets
io_multiplexer = (1U << 2U),          //!< This socket source provides an i/o multiplexer
foreign_io_multiplexer = (1U << 3U),  //!< This socket source may be able to work with compatible io multiplexers
system_implementation = (1U << 4U),   //!< This socket source is the "system" rather than "third party" implementation for whatever it is.

all = 0xffffffff} QUICKCPPLIB_BITFIELD_END(byte_socket_source_implementation_features)

struct byte_socket_source_implementation_information;

/*! \class byte_socket_source
\brief A source of `byte_socket_handle` and `listening_socket` and possibly a `byte_io_multiplexer`
to multiplex i/o on multiple socket instances at the same time.

The socket handles returned by this source may be _very_ different implementations
to the kernel socket handles returned by `byte_socket_handle::byte_socket()` -- they
may have all virtual functions overridden AND a custom i/o multiplexer set, plus RTTI
may show an internal derived type from the public types.

However, they may still be a valid kernel socket handle e.g. a TLS implementation
may act upon a base kernel socket handle. If so, `is_kernel_handle()` will be true.
*/
class byte_socket_source
{
  const byte_socket_source_implementation_information &_implementation_information;
  byte_io_multiplexer *_multiplexer{nullptr};

protected:
  constexpr byte_socket_source(const byte_socket_source_implementation_information &implementation_information, byte_io_multiplexer *multiplexer = nullptr)
      : _implementation_information(implementation_information)
      , _multiplexer(multiplexer)
  {
  }

public:
  using check_for_any_completed_io_statistics = byte_io_multiplexer::check_for_any_completed_io_statistics;

public:
  virtual ~byte_socket_source() {}

  virtual void _deleter() { delete this; }

  //! Returns implementation information about this byte socket source
  const byte_socket_source_implementation_information &implementation_information() const noexcept { return _implementation_information; }

  //! Returns the i/o multiplexer for this byte socket handle source. All handles created from this source will have this multiplexer set upon them.
  byte_io_multiplexer *multiplexer() const noexcept { return _multiplexer; }

  //! Convenience indirect to `multiplexer()->check_for_any_completed_io()`
  result<check_for_any_completed_io_statistics> check_for_any_completed_io(deadline d = std::chrono::seconds(0), size_t max_completions = (size_t) -1) noexcept
  {
    if(_multiplexer == nullptr)
    {
      return errc::invalid_argument;
    }
    return _multiplexer->check_for_any_completed_io(d, max_completions);
  }

  //! Returns a pointer to a new `byte_socket_handle` instance, which may be a recycled instance recently deleted.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<byte_socket_source_handle_ptr>
  byte_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
              byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
              byte_socket_handle::flag flags = byte_socket_handle::flag::none) noexcept = 0;

  //! Returns a pointer to a new `listening_socket_handle` instance, which may be a recycled instance recently deleted.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<listening_socket_source_handle_ptr>
  listening_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
                   byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
                   byte_socket_handle::flag flags = byte_socket_handle::flag::none) noexcept = 0;
};

namespace detail
{
  struct LLFIO_DECL byte_socket_source_deleter
  {
    void operator()(byte_socket_source *p) const { p->_deleter(); }
  };
}  // namespace detail
//! A pointer to a byte socket source
using byte_socket_source_ptr = std::unique_ptr<byte_socket_source, detail::byte_socket_source_deleter>;

//! The implementation information returned.
struct LLFIO_DECL byte_socket_source_implementation_information
{
  string_view name;  //!< The name of the underlying implementation e.g. "OpenSSL", "IOCP", "io_uring", "Windows RIO" etc.
  struct
  {
    uint16_t major{0}, minor{0}, patch{0};
  } version;            //!< Version of the underlying implementation. Could be a kernel version if appropriate.
  string_view postfix;  //!< The build config or other disambiguator from others with the same name and version.

  //! The features this implementation provides
  byte_socket_source_implementation_features features;

  //! Returns true if this byte socket source is compatible with the specified i/o multiplexer
  bool (*is_compatible_with)(const byte_io_multiplexer_ptr &multiplexer) noexcept {_is_compatible_with_default};

  //! Create an instance of this byte socket source. The instance may be shared or recycled or a singleton.
  result<byte_socket_source_ptr> (*instantiate)() noexcept {_instantiate_default};

  //! Create an instance of this byte socket source with a specified i/o multiplexer. The instance may be shared or recycled or a singleton.
  result<byte_socket_source_ptr> (*instantiate_with)(byte_io_multiplexer *multiplexer) noexcept {_instantiate_with_default};

  constexpr byte_socket_source_implementation_information() {}

private:
  static bool _is_compatible_with_default(const byte_io_multiplexer_ptr & /*unused*/) noexcept { return false; }
  static result<byte_socket_source_ptr> _instantiate_default() noexcept { return errc::invalid_argument; }
  static result<byte_socket_source_ptr> _instantiate_with_default(byte_io_multiplexer * /*unused*/) noexcept { return errc::invalid_argument; }
};
static_assert(std::is_trivially_copyable<byte_socket_source_implementation_information>::value,
              "byte_socket_source_implementation_information_t is not trivially copyable!");
inline std::ostream &operator<<(std::ostream &s, const byte_socket_source_implementation_information &v)
{
  return s << v.name << "_v" << v.version.major << "." << v.version.minor << "." << v.version.patch << "_" << v.postfix;
}

/*! \class byte_socket_source_registry
\brief A process-wide registry of `byte_socket_source`.

Probably the most common use case for this will be fetching the default source of TLS
secured sockets, so here is some example boilerplate:

```c++
// Get a source able to manufacture TLS sockets
byte_socket_source_ptr secure_socket_source =
  byte_socket_source_registry::tls_socket_source().instantiate().value();

// Get a new TLS socket able to connect
byte_socket_source_handle_ptr sock =
  secure_socket_source->byte_socket(ip::family::v6).value();

// Resolve the name "host.org" and service "1234" into an IP address,
// and connect to it over TLS
sock->connect(ip::resolve("host.org", "1234").value()->get()).value();

// Write "Hello" to the connected TLS socket
sock->write(0, {{(const byte *) "Hello", 5}}).value();

// With TLS sockets it is important to perform a proper shutdown
// rather than hard close
sock->shutdown_and_close().value();
```
*/
class LLFIO_DECL byte_socket_source_registry
{
public:
  //! The current total number of socket sources known to this registry
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t size() noexcept;

  //! Fills an array with implementation informations for the byte socket sources in the registry with features matching `set` after being masked with `mask`.
  //! The default parameters fetch all sources.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC span<byte_socket_source_implementation_information>
  sources(span<byte_socket_source_implementation_information> tofill,
          byte_socket_source_implementation_features set = byte_socket_source_implementation_features::none,
          byte_socket_source_implementation_features mask = byte_socket_source_implementation_features::none) noexcept;

  //! Convenience overload retrieving TLS socket sources, preferring system over third party implementations.
  static byte_socket_source_implementation_information
  tls_socket_source(byte_socket_source_implementation_features set = byte_socket_source_implementation_features::none) noexcept
  {
    byte_socket_source_implementation_information ret;
    set |= byte_socket_source_implementation_features::tls_sockets;
    set |= byte_socket_source_implementation_features::system_implementation;
    auto filled = sources({&ret, 1}, set, byte_socket_source_implementation_features::all);
    if(!filled.empty())
    {
      return ret;
    }
    set &= ~byte_socket_source_implementation_features::system_implementation;
    filled = sources({&ret, 1}, set, byte_socket_source_implementation_features::all);
    if(!filled.empty())
    {
      return ret;
    }
    return {};
  }

  //! Registers a new source of byte sockets.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> register_source(byte_socket_source_implementation_information i) noexcept;

  //! Deregisters a new source of byte sockets.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC void unregister_source(byte_socket_source_implementation_information i) noexcept;
};

// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/byte_socket_source.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
