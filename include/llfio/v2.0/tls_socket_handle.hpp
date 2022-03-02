/* A handle to a TLS secure socket
(C) 2021-2022 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Jan 2022


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

#ifndef LLFIO_TLS_SOCKET_HANDLE_H
#define LLFIO_TLS_SOCKET_HANDLE_H

#include "byte_socket_handle.hpp"

//! \file tls_socket_handle.hpp Provides `tls_socket_handle`.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nameless struct/union
#pragma warning(disable : 4251)  // dll interface
#pragma warning(disable : 4275)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \brief TLS algorithm categories
 */
QUICKCPPLIB_BITFIELD_BEGIN(tls_algorithm)  //
{                                          //
 /*! The default set of TLS algorithms offered during handshake is a short list
 of strong ciphers (typically 128 - 256 bits for the symmetric cipher)
 to suit embedded and low end devices, ordered by strength-cpuload e.g. ChaCha20
 would be chosen preferentially to AES, **if** the CPUs do not have hardware
 accelerated AES, and longer key sizes would come before shorter key sizes.
 */
 default_ = (0U),
 /*! The set of TLS algorithms compliant with the FIPS 140-2 standard.
 Implementations may refuse to work if this is configured.
 */
 FIPS_140_2 = (1U << 0U)

} QUICKCPPLIB_BITFIELD_END(tls_algorithm)


/*! \class tls_socket_handle
\brief A handle to a TLS secure socket-like entity.

As you cannot create one of these on your own, one generally acquires one of these
from a `tls_socket_source`.
*/
class LLFIO_DECL tls_socket_handle : public byte_socket_handle
{
protected:
  constexpr tls_socket_handle() {}  // NOLINT
  using byte_socket_handle::byte_socket_handle;
  tls_socket_handle &operator=(tls_socket_handle &&) = default;

  explicit tls_socket_handle(byte_socket_handle &&sock)
      : byte_socket_handle(std::move(sock))
  {
    this->_v.behaviour |= native_handle_type::disposition::tls_socket;
  }

public:
  ~tls_socket_handle() = default;

  /*! \brief Returns an implementation defined string describing the algorithms
  either to be chosen during connection, or if after connection the algorithms chosen.
  Can be an empty string if the implementation currently has no mechanism for determining
  the algorithms available or in use (if the latter you may wish to retry later).
  */
  virtual std::string algorithms_description() const = 0;

  /*! \brief Sets the chunk size for registered buffer allocation.

  Some TLS socket handle implementations are able to use registered buffers from their
  underlying plain socket. If so, this sets the granularity of registered buffer allocation,
  otherwise an error is returned if registered buffers are not supported.
  */
  virtual result<void> set_registered_buffer_chunk_size(size_t bytes) noexcept = 0;

  /*! \brief Sets the algorithms to be used by the TLS connection.
   */
  virtual result<void> set_algorithms(tls_algorithm set) noexcept = 0;

  /*! \brief Sets the CA certificates by which this connecting socket identifies itself to
  servers. Defaults to **empty** i.e. clients do not authenticate themselves to servers.

  Note that setting this to an empty path **disables** authentication by the client,
  so client impersonation attacks become possible. This can be useful however for situations
  where setting up client authentication certificates is non-trivial or unnecessary
  (e.g. a HTTPS client connecting to a HTTPS web service), and all that is wanted is an
  encrypted network transport.

  Be aware that the path may not be a filesystem path, but some other sort of implementation
  defined identifier.
  */
  virtual result<void> set_authentication_certificates_path(path_view identifier) noexcept = 0;

  /*! \brief Sets the name of the server which will be connected to for TLS.
  \return The port rendered into a string.
  \param host The hostname to connect to. The TLS connection will use this hostname.
  \param port The port to connect to. The TLS connection will use this port.

  TLS requires the hostname and port to which it will connect for identity verification.
  This can be different to the actual IP address to which you connect, if you wish.

  If you don't call this before `connect()` (or don't use the `connect()` overload
  also taking `host` and `port`), then the client will NOT authenticate the server's certificate.
  */
  virtual result<string_view> set_connect_hostname(string_view host, uint16_t port) noexcept = 0;

  using byte_socket_handle::co_connect;
  using byte_socket_handle::connect;

  /*! \brief Connects to a host, setting the TLS connetion hostname. Convenience overload.
  \param host The hostname to connect to. The TLS connection will use this hostname.
  \param port The port to connect to. The TLS connection will use this port.
  \param d How long to wait for a connection.

  This is a convenience overload combining `set_connect_hostname()` and `connect()`
  from the base class.
  */
  result<void> connect(string_view host, uint16_t port, deadline d = {}) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    OUTCOME_TRY(auto &&service, set_connect_hostname(host, port));
    OUTCOME_TRY(auto &&resolver, ip::resolve(host, service, family(), d, ip::resolve_flag::blocking));
    OUTCOME_TRY(auto &&addresses, resolver->get());
    result<void> lasterror(success());
    for(auto &address : addresses)
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      lasterror = byte_socket_handle::connect(address, nd);
      if(lasterror)
      {
        return lasterror;
      }
      LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
    }
    return lasterror;
  }

  /*! \brief A coroutinised equivalent to `.connect()` which suspends the coroutine until
  a connection occurs. **Blocks execution** i.e is equivalent to `.connect()` if no i/o multiplexer
  has been set on this handle!

  The awaitable returned is **eager** i.e. it immediately begins the i/o. If the i/o completes
  and finishes immediately, no coroutine suspension occurs.
  */
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<void>> co_connect(string_view host, uint16_t port, deadline d = {}) noexcept;
#if 0  // TODO BLOCKED on co_resolve()
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<void>>(connect(addr, d));
    }
    awaitable<io_result<void>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, d, addr));
    return ret;
  }
#endif
};

namespace detail
{
  struct tls_socket_handle_deleter
  {
    void operator()(tls_socket_handle *p) const { p->_deleter(); }
  };
}  // namespace detail

//! A pointer to a TLS socket handle returned by a TLS socket source
using tls_socket_handle_ptr = std::unique_ptr<tls_socket_handle, detail::tls_socket_handle_deleter>;

/*! \class listening_tls_socket_handle
\brief A handle to a TLS socket-like entity able to receive incoming connections.

As you cannot create one of these on your own, one generally acquires one of these
from a `tls_socket_source`.
*/
class LLFIO_DECL listening_tls_socket_handle : public listening_socket_handle_impl<tls_socket_handle_ptr>
{
  using _base = listening_socket_handle_impl<tls_socket_handle_ptr>;

protected:
  constexpr listening_tls_socket_handle() {}
  explicit listening_tls_socket_handle(listening_socket_handle &&sock)
      : _base(sock.release(), sock.kernel_caching(), sock.flags(), sock.multiplexer())
  {
    this->_v.behaviour |= native_handle_type::disposition::tls_socket;
  }
  using _base::_base;

public:
  /*! \brief Returns an implementation defined string describing the algorithms
  to be chosen during connection. Can be an empty string if the implementation
  has no mechanism for determining the algorithms available.
  */
  virtual std::string algorithms_description() const = 0;

  /*! \brief Sets the chunk size for registered buffer allocation.

  Some TLS socket handle implementations are able to use registered buffers from their
  underlying plain socket. If so, this sets the granularity of registered buffer allocation,
  otherwise an error is returned if registered buffers are not supported.
  */
  virtual result<void> set_registered_buffer_chunk_size(size_t bytes) noexcept = 0;

  /*! \brief Sets the algorithms to be used by the TLS connection.
   */
  virtual result<void> set_algorithms(tls_algorithm set) noexcept = 0;

  /*! \brief Sets the CA certificates by which this listening socket identifies itself to
  clients. Defaults to the system certificates store.

  Note that setting this to an empty path **disables** authentication by the server,
  so server impersonation attacks become possible. This can be useful however for situations
  where setting up server authentication certificates is non-trivial or unnecessary, and all
  that is wanted is an encrypted network transport.

  Be aware that the path may not be a filesystem path, but some other sort of implementation
  defined identifier.
  */
  virtual result<void> set_authentication_certificates_path(path_view identifier) noexcept = 0;
};

namespace detail
{
  struct listening_tls_socket_handle_deleter
  {
    void operator()(listening_tls_socket_handle *p) const { p->_deleter(); }
  };
}  // namespace detail

//! A pointer to a listening TLS socket handle returned by a TLS socket source
using listening_tls_socket_handle_ptr = std::unique_ptr<listening_tls_socket_handle, detail::listening_tls_socket_handle_deleter>;

//! Feature bits for TLS socket sources
QUICKCPPLIB_BITFIELD_BEGIN_T(tls_socket_source_implementation_features, uint32_t){
none = 0U,  //!< No bits set

kernel_sockets =
(1U << 0U),  //!< This socket source provides kernel sockets i.e. their native handles will be valid kernel sockets which can be used in `poll()` etc
system_implementation = (1U << 1U),  //!< This socket source is the "system" rather than "third party" implementation for TLS sockets
io_multiplexer = (1U << 2U),         //!< This socket source provides an i/o multiplexer
supports_wrap = (1U << 3U),          //!< This socket source may be able to wrap third party plain sockets

all = 0xffffffff  //!< All bits set
}  //
QUICKCPPLIB_BITFIELD_END(tls_socket_source_implementation_features)

struct tls_socket_source_implementation_information;

/*! \class tls_socket_source
\brief A source of `tls_socket_handle` and `listening_tls_socket_handle` and possibly a `byte_io_multiplexer`
to multiplex i/o on multiple socket instances at the same time.

The socket handles returned by this source may be _very_ different implementations
to the kernel socket handles returned by `byte_socket_handle::byte_socket()` -- they
may have all virtual functions overridden AND a custom i/o multiplexer set, plus RTTI
may show an internal derived type from the public types.

However, they may still be a valid kernel socket handle e.g. a TLS implementation
may act upon a base kernel socket handle. If so, `is_kernel_handle()` will be true,
and then `poll()` will work on these handles same as a plain socket handle.
*/
class tls_socket_source
{
  const tls_socket_source_implementation_information &_implementation_information;
  byte_io_multiplexer *_multiplexer{nullptr};

protected:
  constexpr tls_socket_source(const tls_socket_source_implementation_information &implementation_information, byte_io_multiplexer *multiplexer = nullptr)
      : _implementation_information(implementation_information)
      , _multiplexer(multiplexer)
  {
  }

public:
  using check_for_any_completed_io_statistics = byte_io_multiplexer::check_for_any_completed_io_statistics;

public:
  virtual ~tls_socket_source() {}

  virtual void _deleter() { delete this; }

  //! Returns implementation information about this byte socket source
  const tls_socket_source_implementation_information &implementation_information() const noexcept { return _implementation_information; }

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

  //! Returns a pointer to a new `tls_socket_handle` instance, which may be a recycled instance recently deleted.
  virtual result<tls_socket_handle_ptr> connecting_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
                                                          byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
                                                          byte_socket_handle::flag flags = byte_socket_handle::flag::none) noexcept = 0;
  //! \brief Convenience function defaulting `flag::multiplexable` set.
  result<tls_socket_handle_ptr> multiplexable_connecting_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
                                                                byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
                                                                byte_socket_handle::flag flags = byte_socket_handle::flag::multiplexable) noexcept
  {
    return connecting_socket(family, _mode, _caching, flags);
  }

  //! Returns a pointer to a new `listening_tls_socket_handle` instance, which may be a recycled instance recently deleted.
  virtual result<listening_tls_socket_handle_ptr> listening_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
                                                                   byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
                                                                   byte_socket_handle::flag flags = byte_socket_handle::flag::none) noexcept = 0;
  //! \brief Convenience function defaulting `flag::multiplexable` set.
  result<listening_tls_socket_handle_ptr> multiplexable_listening_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
                                                                         byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
                                                                         byte_socket_handle::flag flags = byte_socket_handle::flag::multiplexable) noexcept
  {
    return listening_socket(family, _mode, _caching, flags);
  }

  //! Returns a pointer to a new `tls_socket_handle` instance, which will wrap `transport`. `transport` must NOT change address until the `tls_socket_handle` is
  //! closed.
  virtual result<tls_socket_handle_ptr> wrap(byte_socket_handle *transport) noexcept = 0;

  //! Returns a pointer to a new `listening_tls_socket_handle` instance, which will wrap `listening`. `listening` must NOT change address until the
  //! `listening_tls_socket_handle` is closed.
  virtual result<listening_tls_socket_handle_ptr> wrap(listening_socket_handle *listening) noexcept = 0;
};

namespace detail
{
  struct LLFIO_DECL tls_socket_source_deleter
  {
    void operator()(tls_socket_source *p) const { p->_deleter(); }
  };
}  // namespace detail
//! A pointer to a TLS socket source
using tls_socket_source_ptr = std::unique_ptr<tls_socket_source, detail::tls_socket_source_deleter>;

//! The implementation information returned.
struct LLFIO_DECL tls_socket_source_implementation_information
{
  string_view name;  //!< The name of the underlying implementation e.g. "openssl", "schannel" etc.
  struct
  {
    uint16_t major{0}, minor{0}, patch{0};
  } version;            //!< Version of the underlying implementation. Could be a kernel version if appropriate.
  string_view postfix;  //!< The build config or other disambiguator from others with the same name and version.

  //! The features this implementation provides
  tls_socket_source_implementation_features features;

  //! Returns true if this TLS socket source is compatible with the specified i/o multiplexer
  bool (*is_compatible_with)(const byte_io_multiplexer_ptr &multiplexer) noexcept {_is_compatible_with_default};

  //! Create an instance of this TLS socket source. The instance may be shared or recycled or a singleton.
  result<tls_socket_source_ptr> (*instantiate)() noexcept {_instantiate_default};

  //! Create an instance of this TLS socket source with a specified i/o multiplexer. The instance may be shared or recycled or a singleton.
  result<tls_socket_source_ptr> (*instantiate_with)(byte_io_multiplexer *multiplexer) noexcept {_instantiate_with_default};

  constexpr tls_socket_source_implementation_information() {}
  constexpr tls_socket_source_implementation_information(string_view _name)
      : name(_name)
  {
  }

private:
  static bool _is_compatible_with_default(const byte_io_multiplexer_ptr & /*unused*/) noexcept { return false; }
  static result<tls_socket_source_ptr> _instantiate_default() noexcept { return errc::invalid_argument; }
  static result<tls_socket_source_ptr> _instantiate_with_default(byte_io_multiplexer * /*unused*/) noexcept { return errc::invalid_argument; }
};
static_assert(std::is_trivially_copyable<tls_socket_source_implementation_information>::value,
              "tls_socket_source_implementation_information_t is not trivially copyable!");
inline std::ostream &operator<<(std::ostream &s, const tls_socket_source_implementation_information &v)
{
  return s << v.name << "_v" << v.version.major << "." << v.version.minor << "." << v.version.patch << "_" << v.postfix;
}

/*! \class tls_socket_source_registry
\brief A process-wide registry of `tls_socket_source`.

Probably the most common use case for this will be fetching the default source of TLS
secured sockets, so here is some example boilerplate:

Blocking:

```c++
// Get a source able to manufacture TLS sockets
tls_socket_source_ptr secure_socket_source =
  tls_socket_source_registry::default_source().instantiate().value();

// Get a new TLS socket able to connect
tls_socket_source_handle_ptr sock =
  secure_socket_source->connecting_socket(ip::family::v6).value();

// Resolve the name "host.org" and service "1234" into an IP address,
// and connect to it over TLS. If the remote's TLS certificate is
// not trusted by this system, or the remote certificate does not
// match the host, this call will fail.
sock->connect("host.org", 1234).value();

// Write "Hello" to the connected TLS socket
sock->write(0, {{(const byte *) "Hello", 5}}).value();

// Blocking read the response
char buffer[5];
tls_socket_handle::buffer_type b((byte *) buffer, 5);
auto readed = sock->read({{&b, 1}, 0}).value();
if(string_view(buffer, b.size()) != "World") {
  abort();
}

// With TLS sockets it is important to perform a proper shutdown
// rather than hard close
sock->shutdown_and_close().value();
```

Non-blocking:

```c++
// Get a source able to manufacture TLS sockets
tls_socket_source_ptr secure_socket_source =
  tls_socket_source_registry::default_source().instantiate().value();

// Get a new TLS socket able to connect
tls_socket_source_handle_ptr sock =
  secure_socket_source->multiplexable_connecting_socket(ip::family::v6).value();

// Resolve the name "host.org" and service "1234" into an IP address,
// and connect to it over TLS. If the remote's TLS certificate is
// not trusted by this system, or the remote certificate does not
// match the host, this call will fail.
//
// If the connection does not complete within three seconds, fail.
sock->connect("host.org", 1234, std::chrono::seconds(3)).value();

// Write "Hello" to the connected TLS socket
sock->write(0, {{(const byte *) "Hello", 5}}, std::chrono::seconds(3)).value();

// Blocking read the response, but only up to three seconds.
char buffer[5];
tls_socket_handle::buffer_type b((byte *) buffer, 5);
auto readed = sock->read({{&b, 1}, 0}, std::chrono::seconds(3)).value();
if(string_view(buffer, b.size()) != "World") {
  abort();
}

// With TLS sockets it is important to perform a proper shutdown
// rather than hard close
sock->shutdown_and_close(std::chrono::seconds(3)).value();
```
*/
class LLFIO_DECL tls_socket_source_registry
{
public:
  //! True if there are no socket sources known to this registry
  LLFIO_NODISCARD static bool empty() noexcept { return size() == 0; }
  //! The current total number of socket sources known to this registry
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t size() noexcept;

  //! Fills an array with implementation informations for the TLS socket sources in the registry with features matching `set` after being masked with `mask`.
  //! The default parameters fetch all sources.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC span<tls_socket_source_implementation_information>
  sources(span<tls_socket_source_implementation_information> tofill,
          tls_socket_source_implementation_features set = tls_socket_source_implementation_features::none,
          tls_socket_source_implementation_features mask = tls_socket_source_implementation_features::none) noexcept;

  //! Convenience overload retrieving TLS socket sources, preferring system over third party implementations.
  static tls_socket_source_implementation_information
  default_source(tls_socket_source_implementation_features set = tls_socket_source_implementation_features::none,
                 tls_socket_source_implementation_features mask = tls_socket_source_implementation_features::system_implementation) noexcept
  {
    tls_socket_source_implementation_information ret{"no implementation available"};
    set |= tls_socket_source_implementation_features::system_implementation;
    auto filled = sources({&ret, 1}, set, mask);
    if(!filled.empty())
    {
      return ret;
    }
    set &= ~tls_socket_source_implementation_features::system_implementation;
    filled = sources({&ret, 1}, set, mask);
    if(!filled.empty())
    {
      return ret;
    }
    return {};
  }

  //! Registers a new source of byte sockets.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> register_source(tls_socket_source_implementation_information i) noexcept;

  //! Deregisters a new source of byte sockets.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC void unregister_source(tls_socket_source_implementation_information i) noexcept;
};

// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/tls_socket_handle.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
