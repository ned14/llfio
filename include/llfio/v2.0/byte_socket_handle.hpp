/* A handle to a byte-orientated socket
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

#ifndef LLFIO_BYTE_SOCKET_HANDLE_H
#define LLFIO_BYTE_SOCKET_HANDLE_H

#include "byte_io_handle.hpp"

struct sockaddr;
struct sockaddr_in;
struct sockaddr_in6;

//! \file byte_socket_handle.hpp Provides `byte_socket_handle`.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nameless struct/union
#pragma warning(disable : 4251)  // dll interface
#pragma warning(disable : 4275)  // dll interface
#pragma warning(disable : 4661)  // missing implementation
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class byte_socket_handle;
class listening_socket_handle;
namespace ip
{
  class address;
  LLFIO_HEADERS_ONLY_FUNC_SPEC std::ostream &operator<<(std::ostream &s, const address &v);
}  // namespace ip

namespace detail
{
#ifdef _WIN32
  LLFIO_HEADERS_ONLY_FUNC_SPEC void register_socket_handle_instance(void *i) noexcept;
  LLFIO_HEADERS_ONLY_FUNC_SPEC void unregister_socket_handle_instance(void *i) noexcept;
#endif
}  // namespace detail

//! Inspired by ASIO's `ip` namespace
namespace ip
{
  class address_v4;
  class address_v6;

  //! The family of IP
  enum class family
  {
    unknown,
    v4,  //!< IP version 4
    v6,  //!< IP version 6
    any  //!< Either v4 or v6
  };
  /*! \class address
  \brief A version independent IP address.

  This is inspired by `asio::ip::address`, but it also adds `port()` from `asio::ip::endpoint`
  and a few other observer member functions i.e. it fuses ASIO's many types into one.

  The reason why is that this type is a simple wrap of `struct sockaddr_in` or `struct sockaddr_in6`,
  it doesn't split those structures into multiple C++ types.
  */
  class LLFIO_DECL address
  {
    friend class LLFIO_V2_NAMESPACE::byte_socket_handle;
    friend class LLFIO_V2_NAMESPACE::listening_socket_handle;
    friend LLFIO_HEADERS_ONLY_MEMFUNC_SPEC std::ostream &operator<<(std::ostream &s, const address &v);

  protected:
    union
    {
      // This is a struct sockaddr, so endian varies
      struct
      {
        unsigned short _family;  // sa_family_t, host endian
        uint16_t _port;          // in_port_t, big endian
        union
        {
          struct
          {
            uint32_t _flowinfo;  // big endian
            byte _addr[16];      // big endian
            uint32_t _scope_id;  // big endian
          } ipv6;
          union
          {
            byte _addr[4];      // big endian
            uint32_t _addr_be;  // big endian
          } ipv4;
        };
      };
      byte _storage[32];  // struct sockaddr_?
    };

  public:
    constexpr address() noexcept
        : _storage{to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0),
                   to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0),
                   to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0), to_byte(0)}
    {
    }
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC explicit address(const sockaddr_in &storage) noexcept;
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC explicit address(const sockaddr_in6 &storage) noexcept;
    address(const address &) = default;
    address(address &&) = default;
    address &operator=(const address &) = default;
    address &operator=(address &&) = default;
    ~address() = default;

    //! True if addresses are equal
    bool operator==(const address &o) const noexcept { return 0 == memcmp(_storage, o._storage, sizeof(_storage)); }
    //! True if addresses are not equal
    bool operator!=(const address &o) const noexcept { return 0 != memcmp(_storage, o._storage, sizeof(_storage)); }
    //! True if address is less than
    bool operator<(const address &o) const noexcept { return memcmp(_storage, o._storage, sizeof(_storage)) < 0; }

    //! True if address is loopback
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_loopback() const noexcept;
    //! True if address is multicast
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_multicast() const noexcept;
    //! True if address is any
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_any() const noexcept;
    //! True if address is default constructed
    bool is_default() const noexcept { return _family == 0; }
    //! True if address is v4
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_v4() const noexcept;
    //! True if address is v6
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_v6() const noexcept;

    //! Returns the raw family of the address
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC unsigned short raw_family() const noexcept;
    //! Returns the family of the addres
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC enum family family() const noexcept;
    //! Returns the port of the address
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint16_t port() const noexcept;
    //! Returns the IPv6 flow info, if address is v6.
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint32_t flowinfo() const noexcept;
    //! Returns the IPv6 scope id, if address is v6.
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint32_t scope_id() const noexcept;

    //! Returns the bytes of the address in network order
    span<const byte> to_bytes() const noexcept { return is_v6() ? span<const byte>(ipv6._addr) : span<const byte>(ipv4._addr); }
    //! Returns the address as a `sockaddr *`.
    const sockaddr *to_sockaddr() const noexcept { return (const sockaddr *) _storage; }
    //! Returns the size of the `sockaddr`
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC int sockaddrlen() const noexcept;
  };
  static_assert(std::is_trivially_copyable<address>::value, "ip::address is not trivially copyable!");
  //! Write address to stream
  LLFIO_HEADERS_ONLY_FUNC_SPEC std::ostream &operator<<(std::ostream &s, const address &v);

  class resolver;
  namespace detail
  {
    struct LLFIO_DECL resolver_deleter
    {
      LLFIO_HEADERS_ONLY_MEMFUNC_SPEC void operator()(resolver *p) const;
    };
  }  // namespace detail
  //! Returned by `resolve()` as a handle to the asynchronous name resolution operation.
  class LLFIO_DECL resolver
  {
  public:
    //! Returns the name being resolved
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const std::string &name() const noexcept;
    //! Returns the service being resolved
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const std::string &service() const noexcept;
    //! Returns true if the deadline expired, and the returned list of addresses is incomplete. Until `get()` is called, always is true.
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool incomplete() const noexcept;
    //! Returns the array of addresses, blocking until completion if necessary, returning any error if one occurred.
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<span<address>> get() noexcept;
    //! Wait for up the deadline for the array of addresses to be retrieved.
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> wait(deadline d = {}) noexcept;
    //! \overload
    template <class Rep, class Period> result<bool> wait_for(const std::chrono::duration<Rep, Period> &duration) const noexcept
    {
      auto r = wait(duration);
      if(!r && r.error() == errc::timed_out)
      {
        return false;
      }
      OUTCOME_TRY(std::move(r));
      return true;
    }
    //! \overload
    template <class Clock, class Duration> result<bool> wait_until(const std::chrono::time_point<Clock, Duration> &timeout) const noexcept
    {
      auto r = wait(timeout);
      if(!r && r.error() == errc::timed_out)
      {
        return false;
      }
      OUTCOME_TRY(std::move(r));
      return true;
    }
  };
  //! A pointer to a resolver
  using resolver_ptr = std::unique_ptr<resolver, detail::resolver_deleter>;

  //! Flags for `resolve()`
  QUICKCPPLIB_BITFIELD_BEGIN(resolve_flag){
  none = 0,              //!< No flags
  passive = (1U << 0U),  //!< Return addresses for binding to this machine.
  blocking = (1U << 1U)  //!< Execute address resolution synchronously.
  } QUICKCPPLIB_BITFIELD_END(resolve_flag)

  /*! \brief Retrieve a list of potential `address` for a given name and service e.g.
  "www.google.com" and "https" optionally within a bounded deadline.

  The object returned by this function can take many seconds to become ready as multiple network requests may
  need to be made. The deadline can be used to bound execution times -- like in a few
  other places in LLFIO, this deadline does not cause timed out errors, rather it aborts
  any remaining name resolution after the deadline expires and returns whatever addresses
  have been resolved by the deadline.

  This function has a future-like API as several major platforms provide native asynchronous
  name resolution (currently: Linux, Windows). On other platforms, `std::async` with
  `getaddrinfo()` is used as an emulation, and therefore deadline expiry means no partial
  list of addresses are returned.

  If you become no longer interested in the results, simply reset or delete the pointer
  and the resolution will be aborted asynchronously.

  \mallocs This is one of those very few APIs in LLFIO where dynamic memory allocation
  is unbounded and uncontrollable thanks to how the platform APIs are implemented.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<resolver_ptr> resolve(string_view name, string_view service, family _family = family::any, deadline d = {},
                                                            resolve_flag flags = resolve_flag::none) noexcept;
  //! `resolve()` may utilise a process-wide cache, if so this function will trim that cache.
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> resolve_trim_cache(size_t maxitems = 64) noexcept;

  //! Make an `address_v4`
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v4> make_address_v4(string_view str) noexcept;
  /*! \class address_v4
  \brief A v4 IP address.
  */
  class LLFIO_DECL address_v4 : public address
  {
    friend result<address_v4> make_address_v4(string_view str) noexcept;

  public:
#if QUICKCPPLIB_USE_STD_SPAN
    using bytes_type = span<const byte, 4>;
#else
    using bytes_type = span<const byte>;
#endif
    using uint_type = uint32_t;
    constexpr address_v4() noexcept {}
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC explicit address_v4(const bytes_type &bytes, uint16_t port = 0) noexcept;
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC explicit address_v4(uint_type addr, uint16_t port = 0) noexcept;
    address_v4(const address_v4 &) = default;
    address_v4(address_v4 &&) = default;
    address_v4 &operator=(const address_v4 &) = default;
    address_v4 &operator=(address_v4 &&) = default;
    ~address_v4() = default;

    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bytes_type to_bytes() const noexcept;
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint_type to_uint() const noexcept;

    static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4 any() noexcept;
    static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v4 loopback() noexcept;
  };
  //! Make an `address_v4`
  inline result<address_v4> make_address_v4(const address_v4::bytes_type &bytes, uint16_t port = 0) noexcept { return address_v4(bytes, port); }
  //! Make an `address_v4`
  inline result<address_v4> make_address_v4(const address_v4::uint_type &bytes, uint16_t port = 0) noexcept { return address_v4(bytes, port); }

  //! Make an `address_v6`
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v6> make_address_v6(string_view str) noexcept;
  /*! \class address_v6
  \brief A v6 IP address.
  */
  //! Make an `address_v6`. v6 addresses need to have the form `[::]:port`.
  class LLFIO_DECL address_v6 : public address
  {
    friend result<address_v6> make_address_v6(string_view str) noexcept;

  public:
#if QUICKCPPLIB_USE_STD_SPAN
    using bytes_type = span<const byte, 16>;
#else
    using bytes_type = span<const byte>;
#endif

    constexpr address_v6() noexcept {}
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC explicit address_v6(const bytes_type &bytes, uint16_t port = 0, uint32_t scope_id = 0) noexcept;
    address_v6(const address_v6 &) = default;
    address_v6(address_v6 &&) = default;
    address_v6 &operator=(const address_v6 &) = default;
    address_v6 &operator=(address_v6 &&) = default;
    ~address_v6() = default;

    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bytes_type to_bytes() const noexcept;

    static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6 any() noexcept;
    static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC address_v6 loopback() noexcept;
  };
  //! Make an `address_v6`
  inline result<address_v6> make_address_v6(const address_v6::bytes_type &bytes, uint16_t port = 0, uint32_t scope_id = 0) noexcept
  {
    return address_v6(bytes, port, scope_id);
  }
  //! Make a v4 or v6 `address`. v6 addresses need to have the form `[::]:port`.
  inline result<address> make_address(string_view str) noexcept
  {
    if(str.size() > 0 && str[0] == '[')
    {
      OUTCOME_TRY(auto &&addr, make_address_v6(str));
      return addr;
    }
    OUTCOME_TRY(auto &&addr, make_address_v4(str));
    return addr;
  }
}  // namespace ip


/*! \class byte_socket_handle
\brief A handle to a byte-orientated socket-like entity.

This handle, or subclasses thereof, may refer to:

- a BSD socket in the kernel configured for TCP.
- a TLS socket in a userspace library.
- a userspace socket for certain types of high end network card.
- or indeed, anything which quacks like a `SOCK_STREAM` socket.

If you construct it directly and assign it a socket that you created,
then it refers to a kernel BSD socket, as the default implementation
is for a kernel BSD socket. If you get an instance from elsewhere,
it may have a *very* different implementation.

The default is blocking sockets, on which timed out i/o is not possible.
In this use case, `byte_socket()` will block until a successful
connection is established with the remote address. Thereafter `read()`
and `write()` block based on i/o from the other side, returning
immediately if at least one byte is transferred.

If `flag::multiplexable` is specified which causes the handle to
be created as `native_handle_type::disposition::nonblocking`, `byte_socket()`
no longer blocks. However it will then block in `read()` or `write()`,
unless its deadline is zero.

If you want to create a socket which awaits connections, you need
to instance a `listening_socket_handle`. Reads from that handle yield
new `byte_socket_handle` instances.

### `caching::safety_barriers`

TCP connections need to be closed by both parties in a specific way
to not cause the tail of data sent to get truncated:

1. Local side calls `shutdown(SHUT_WR)` to send the FIN packet.
2. Remote side calls `shutdown(SHUT_WR)` to send the FIN packet.
3. Local side `read()` returns no bytes read as remote side has closed
down. Local side can now call `close()`.
4. Remote side `read()` returns no bytes read as local side has closed
down. Remote side can now call `close()`.

This is obviously inefficient and prone to issues if the remote side
is not a good faith actor, so most TCP based protocols such as HTTP
send the length of the data to be transferred, and one loops reading
until that length of data is read, whereupon the TCP connection is
immediately forced closed without the TCP shutdown ceremony.

The default caching is when `close()` is called it immediately
closes the socket handle, causing an abort in the connection if any
data remains in buffers. If you wish `close()` to instead issue
a shutdown and to then block on `read()` until it returns no bytes
before closing, set `caching::safety_barriers`.

This *should* avoid the need for `SO_LINGER` for remote sides
acting in good faith. If you don't control remote side code quality,
you may still need to set `SO_LINGER`, though be aware that that
socket option is full of gotchas.

If you don't wish to have this operation occur during close, you
can call `shutdown_and_close()` manually instead.
*/
class LLFIO_DECL byte_socket_handle : public byte_io_handle, public pollable_handle
{
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }

public:
  using path_type = byte_io_handle::path_type;
  using extent_type = byte_io_handle::extent_type;
  using size_type = byte_io_handle::size_type;
  using mode = byte_io_handle::mode;
  using creation = byte_io_handle::creation;
  using caching = byte_io_handle::caching;
  using flag = byte_io_handle::flag;
  using buffer_type = byte_io_handle::buffer_type;
  using const_buffer_type = byte_io_handle::const_buffer_type;
  using buffers_type = byte_io_handle::buffers_type;
  using const_buffers_type = byte_io_handle::const_buffers_type;
  template <class T> using io_request = byte_io_handle::io_request<T>;
  template <class T> using io_result = byte_io_handle::io_result<T>;

  // Used by byte_socket_source
  virtual void _deleter() { delete this; }

protected:
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> _do_connect(const ip::address &addr, deadline d) noexcept;

  result<void> _do_multiplexer_connect(const ip::address &addr, deadline d) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    const auto state_reqs = _ctx->io_state_requirements();
    auto *storage = (byte *) alloca(state_reqs.first + state_reqs.second);
    const auto diff = (uintptr_t) storage & (state_reqs.second - 1);
    storage += state_reqs.second - diff;
    auto *state = _ctx->construct_and_init_io_operation({storage, state_reqs.first}, this, nullptr, d, addr);
    if(state == nullptr)
    {
      return errc::resource_unavailable_try_again;
    }
    OUTCOME_TRY(_ctx->flush_inited_io_operations());
    while(!is_finished(_ctx->check_io_operation(state)))
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(_ctx->check_for_any_completed_io(nd));
    }
    OUTCOME_TRY(std::move(*state).get_completed_read());
    state->~io_operation_state();
    return success();
  }

public:
  //! Default constructor
  constexpr byte_socket_handle() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  constexpr byte_socket_handle(native_handle_type h, caching caching, flag flags, byte_io_multiplexer *ctx)
      : byte_io_handle(std::move(h), caching, flags, ctx)
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
    }
#endif
  }
  //! No copy construction (use clone())
  byte_socket_handle(const byte_socket_handle &) = delete;
  //! No copy assignment
  byte_socket_handle &operator=(const byte_socket_handle &) = delete;
  //! Implicit move construction of byte_socket_handle permitted
  constexpr byte_socket_handle(byte_socket_handle &&o) noexcept
      : byte_io_handle(std::move(o))
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
      detail::unregister_socket_handle_instance(&o);
    }
#endif
  }
  //! Explicit conversion from handle permitted
  explicit constexpr byte_socket_handle(handle &&o, byte_io_multiplexer *ctx) noexcept
      : byte_io_handle(std::move(o), ctx)
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
    }
#endif
  }
  //! Explicit conversion from byte_io_handle permitted
  explicit constexpr byte_socket_handle(byte_io_handle &&o) noexcept
      : byte_io_handle(std::move(o))
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
    }
#endif
  }
  //! Move assignment of byte_socket_handle permitted
  byte_socket_handle &operator=(byte_socket_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
#ifdef _WIN32
    if(_v)
    {
      detail::unregister_socket_handle_instance(this);
    }
#endif
    this->~byte_socket_handle();
    new(this) byte_socket_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(byte_socket_handle &o) noexcept
  {
    byte_socket_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  //! Returns the IP family of this socket instance
  ip::family family() const noexcept { return (this->_v.behaviour & native_handle_type::disposition::is_alternate) ? ip::family::v6 : ip::family::v4; }
  //! Returns the local endpoint of this socket instance
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<ip::address> local_endpoint() const noexcept;
  //! Returns the remote endpoint of this socket instance
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<ip::address> remote_endpoint() const noexcept;

  //! The channels which can be shut down.
  enum shutdown_kind
  {
    shutdown_read,   //!< Shutdown further reads.
    shutdown_write,  //!< Shutdown further writes.
    shutdown_both    //!< Shutdown both further reads and writes.
  };
  /*! \brief Initiates shutting down further communication on the socket.

  The default is `shutdown_write`, as generally if shutting down you want send
  a FIN packet to remote and loop polling reads until you receive a FIN from
  remote.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> shutdown(shutdown_kind = shutdown_write) noexcept;

  /*! \brief Connects to an address.
  \param addr The address to connect to.
  \param d How long to wait for a connection.

  The connection begins upon first call, if it times out then you can call this function again with a
  new timeout to poll the socket for when it connects. Eventually this function will either succeed,
  or fail with an error if the connection failed.

  \errors Any of the values `connect()` can return;
  */
  result<void> connect(const ip::address &addr, deadline d = {}) noexcept
  {
    return (_ctx == nullptr) ? _do_connect(addr, d) : _do_multiplexer_connect(addr, d);
  }

  /*! \brief A coroutinised equivalent to `.connect()` which suspends the coroutine until
  a connection occurs. **Blocks execution** i.e is equivalent to `.connect()` if no i/o multiplexer
  has been set on this handle!

  The awaitable returned is **eager** i.e. it immediately begins the i/o. If the i/o completes
  and finishes immediately, no coroutine suspension occurs.
  */
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<void>> co_connect(const ip::address &addr, deadline d = {}) noexcept;
#if 0  // TODO
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

  /*! Create a socket handle.
  \param family Which IP family to create the socket in.
  \param _mode How to open the socket. If this is `mode::append`, the read side of the socket
  is shutdown; if this is `mode::read`, the write side of the socket is shutdown.
  \param _caching How to ask the kernel to cache the socket. If writes are not cached,
  `SO_SNDBUF` to the minimum possible value and `TCP_NODELAY` is set, this should cause
  writes to hit the network as quickly as possible.
  \param flags Any additional custom behaviours.

  \errors Any of the values POSIX `socket()` or `WSASocket()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle> byte_socket(ip::family family, mode _mode = mode::write, caching _caching = caching::all,
                                                                                flag flags = flag::none) noexcept;
  //! \brief Convenience function defaulting `flag::multiplexable` set.
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle>
  multiplexable_byte_socket(ip::family family, mode _mode = mode::write, caching _caching = caching::all, flag flags = flag::multiplexable) noexcept;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~byte_socket_handle() override
  {
    if(_v)
    {
      auto r = byte_socket_handle::close();
      if(!r)
      {
        // std::cout << r.error().message() << std::endl;
        LLFIO_LOG_FATAL(_v.fd, "byte_socket_handle::~byte_socket_handle() close failed");
        abort();
      }
    }
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override;

  /*! \brief Convenience function to shut down the outbound connection and wait for the other side to shut down our
  inbound connection by throwing away any bytes read, then closing the socket.
  */
  result<void> shutdown_and_close(deadline d = {}) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    OUTCOME_TRY(shutdown());
    byte buffer[4096];
    for(;;)
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(auto readed, read(0, {{buffer}}, nd));
      if(readed == 0)
      {
        break;
      }
    }
    return close();
  }

  using byte_io_handle::co_read;
  using byte_io_handle::co_write;
  using byte_io_handle::read;
  using byte_io_handle::write;

  //! \overload Convenience initialiser list based overload for `read()`
  LLFIO_MAKE_FREE_FUNCTION
  io_result<size_type> read(std::initializer_list<buffer_type> lst, deadline d = deadline()) noexcept
  {
    buffer_type *_reqs = reinterpret_cast<buffer_type *>(alloca(sizeof(buffer_type) * lst.size()));
    memcpy(_reqs, lst.begin(), sizeof(buffer_type) * lst.size());
    io_request<buffers_type> reqs(buffers_type(_reqs, lst.size()));
    auto ret = read(reqs, d);
    if(ret)
    {
      return ret.bytes_transferred();
    }
    return std::move(ret).error();
  }
  //! \overload Convenience initialiser list based overload for `write()`
  LLFIO_MAKE_FREE_FUNCTION
  io_result<size_type> write(std::initializer_list<const_buffer_type> lst, deadline d = deadline()) noexcept
  {
    const_buffer_type *_reqs = reinterpret_cast<const_buffer_type *>(alloca(sizeof(const_buffer_type) * lst.size()));
    memcpy(_reqs, lst.begin(), sizeof(const_buffer_type) * lst.size());
    io_request<const_buffers_type> reqs(const_buffers_type(_reqs, lst.size()));
    auto ret = write(reqs, d);
    if(ret)
    {
      return ret.bytes_transferred();
    }
    return std::move(ret).error();
  }
};

//! \brief Constructor for `byte_socket_handle`
template <> struct construct<byte_socket_handle>
{
  ip::family family;
  byte_socket_handle::mode _mode = byte_socket_handle::mode::write;
  byte_socket_handle::caching _caching = byte_socket_handle::caching::all;
  byte_socket_handle::flag flags = byte_socket_handle::flag::none;
  result<byte_socket_handle> operator()() const noexcept { return byte_socket_handle::byte_socket(family, _mode, _caching, flags); }
};

/* \class listening_socket_handle_impl
\brief A handle to a socket-like entity able to receive incoming connections.
*/
template <class SocketType> class listening_socket_handle_impl : public handle, public pollable_handle
{
  template <class ST> friend class listening_socket_handle_impl;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }

protected:
  byte_io_multiplexer *_ctx{nullptr};  // +4 or +8 bytes

  template <class Impl>
  result<typename Impl::buffers_type> _underlying_read(typename Impl::template io_request<typename Impl::buffers_type> req, deadline d) noexcept
  {
    if(_v.behaviour & native_handle_type::disposition::is_pointer)
    {
      return reinterpret_cast<Impl *>(_v.ptr)->read(std::move(req), d);
    }
    auto *sock = static_cast<Impl *>(static_cast<handle *>(this));
    return (_ctx == nullptr) ? sock->Impl::_do_read(std::move(req), d) : sock->Impl::_do_multiplexer_read(std::move(req), d);
  }

public:
  //! The buffer type used by this handle, which is a pair of `SocketType` and `ip::address`
  using buffer_type = std::pair<SocketType, ip::address>;
  //! The const buffer type used by this handle, which is a pair of `SocketType` and `ip::address`
  using const_buffer_type = std::pair<SocketType, ip::address>;
  //! The buffers type used by this handle for reads, which is a single item sequence of `buffer_type`.
  struct buffers_type
  {
    //! Type of the pointer to the buffer.
    using pointer = buffer_type *;
    //! Type of the iterator to the buffer.
    using iterator = buffer_type *;
    //! Type of the iterator to the buffer.
    using const_iterator = const buffer_type *;
    //! Type of the length of the buffers.
    using size_type = size_t;

    //! Default constructor
    constexpr buffers_type() {}  // NOLINT

    /*! Constructor
     */
    constexpr buffers_type(buffer_type &sock)
        : _sock(&sock)
    {
    }
    ~buffers_type() = default;
    //! Move constructor
    buffers_type(buffers_type &&o) noexcept
        : _sock(o._sock)
    {
      o._sock = nullptr;
    }
    //! No copy construction
    buffers_type(const buffers_type &) = delete;
    //! Move assignment
    buffers_type &operator=(buffers_type &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      this->~buffers_type();
      new(this) buffers_type(std::move(o));
      return *this;
    }
    //! No copy assignment
    buffers_type &operator=(const buffers_type &) = delete;

    //! True if empty
    LLFIO_NODISCARD constexpr bool empty() const noexcept { return _sock == nullptr; }
    //! Returns an iterator to the beginning of the buffers
    constexpr iterator begin() noexcept { return _sock; }
    //! Returns an iterator to the beginning of the buffers
    constexpr const_iterator begin() const noexcept { return _sock; }
    //! Returns an iterator to the beginning of the buffers
    constexpr const_iterator cbegin() const noexcept { return _sock; }
    //! Returns an iterator to after the end of the buffers
    constexpr iterator end() noexcept { return _sock + 1; }
    //! Returns an iterator to after the end of the buffers
    constexpr const_iterator end() const noexcept { return _sock + 1; }
    //! Returns an iterator to after the end of the buffers
    constexpr const_iterator cend() const noexcept { return _sock + 1; }

    //! The socket referenced by the buffers
    const buffer_type &connected_socket() const &noexcept
    {
      assert(_sock != nullptr);
      return *_sock;
    }
    //! The socket referenced by the buffers
    buffer_type &connected_socket() &noexcept
    {
      assert(_sock != nullptr);
      return *_sock;
    }
    //! The socket and its connected address referenced by the buffers
    buffer_type connected_socket() &&noexcept
    {
      assert(_sock != nullptr);
      return std::move(*_sock);
    }

  private:
    buffer_type *_sock{nullptr};
  };
  //! The const buffers type used by this handle for reads, which is a single item sequence of `buffer_type`.
  using const_buffers_type = buffers_type;
  //! The i/o request type used by this handle.
  template <class /*unused*/> struct io_request
  {
    buffers_type buffers{};

    /*! Construct a request to listen for new socket connections.

    \param _buffers The buffers to fill with connected sockets.
    */
    constexpr io_request(buffers_type _buffers)
        : buffers(std::move(_buffers))
    {
    }
  };
  template <class T> using io_result = result<T>;
  template <class T> using awaitable = byte_io_multiplexer::awaitable<T>;

  // Used by byte_socket_source
  virtual void _deleter() { delete this; }

protected:
  virtual result<buffers_type> _do_read(io_request<buffers_type> req, deadline d) noexcept = 0;

  virtual io_result<buffers_type> _do_multiplexer_read(io_request<buffers_type> reqs, deadline d) noexcept = 0;

protected:
  //! Default constructor
  constexpr listening_socket_handle_impl() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  constexpr listening_socket_handle_impl(native_handle_type h, caching caching, flag flags, byte_io_multiplexer *ctx)
      : handle(std::move(h), caching, flags)
      , _ctx(ctx)
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
    }
#endif
  }
  //! No copy construction (use clone())
  listening_socket_handle_impl(const listening_socket_handle_impl &) = delete;
  //! No copy assignment
  listening_socket_handle_impl &operator=(const listening_socket_handle_impl &) = delete;
  //! Implicit move construction of listening socket handle permitted
  constexpr listening_socket_handle_impl(listening_socket_handle_impl &&o) noexcept
      : handle(std::move(o))
      , _ctx(o._ctx)
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
      detail::unregister_socket_handle_instance(&o);
    }
#endif
  }
  //! Explicit conversion from handle permitted
  explicit constexpr listening_socket_handle_impl(handle &&o, byte_io_multiplexer *ctx) noexcept
      : handle(std::move(o))
      , _ctx(ctx)
  {
#ifdef _WIN32
    if(_v)
    {
      detail::register_socket_handle_instance(this);
    }
#endif
  }

public:
  /*! \brief The i/o multiplexer this handle will use to multiplex i/o. If this returns null,
  then this handle has not been registered with an i/o multiplexer yet.
  */
  byte_io_multiplexer *multiplexer() const noexcept { return _ctx; }

  /*! \brief Sets the i/o multiplexer this handle will use to implement `read()`, `write()` and `barrier()`.

  Note that this call deregisters this handle from any existing i/o multiplexer, and registers
  it with the new i/o multiplexer. You must therefore not call it if any i/o is currently
  outstanding on this handle. You should also be aware that multiple dynamic memory
  allocations and deallocations may occur, as well as multiple syscalls (i.e. this is
  an expensive call, try to do it from cold code).

  If the handle was not created as multiplexable, this call always fails.

  \mallocs Multiple dynamic memory allocations and deallocations.
  */
  virtual result<void> set_multiplexer(byte_io_multiplexer *c = this_thread::multiplexer()) noexcept = 0;

  //! Returns the IP family of this socket instance
  ip::family family() const noexcept { return (this->_v.behaviour & native_handle_type::disposition::is_alternate) ? ip::family::v6 : ip::family::v4; }

  //! Returns the local endpoint of this socket instance
  virtual result<ip::address> local_endpoint() const noexcept = 0;

  /*! \brief Binds a socket to a local endpoint and sets the socket to listen for new connections.
  \param addr The local endpoint to which to bind the socket.
  \param _creation Whether to apply `SO_REUSEADDR` before binding.
  \param backlog The maximum queue length of pending connections. `-1` chooses `SOMAXCONN`.

  You should set any socket options etc that you need on `native_handle()` before binding
  the socket to its local endpoint.

  \errors Any of the values `bind()` and `listen()` can return.
  */
  virtual result<void> bind(const ip::address &addr, creation _creation = creation::only_if_not_exist, int backlog = -1) noexcept = 0;

  /*! Read the contents of the listening socket for newly connected byte sockets.

  \return Returns the buffers filled, with its socket handle and address set to the newly connected socket.
  \param req A buffer to fill with a newly connected socket.
  \param d An optional deadline by which to time out.

  \errors Any of the errors which `accept()` or `WSAAccept()` might return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  result<buffers_type> read(io_request<buffers_type> req, deadline d = {}) noexcept
  {
    return (_ctx == nullptr) ? _do_read(std::move(req), d) : _do_multiplexer_read(std::move(req), d);
  }

  /*! \brief A coroutinised equivalent to `.read()` which suspends the coroutine until
  a new incoming connection occurs. **Blocks execution** i.e is equivalent to `.read()` if no i/o multiplexer
  has been set on this handle!

  The awaitable returned is **eager** i.e. it immediately begins the i/o. If the i/o completes
  and finishes immediately, no coroutine suspension occurs.
  */
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<buffers_type>> co_read(io_request<buffers_type> reqs, deadline d = {}) noexcept;
#if 0  // TODO
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<buffers_type>>(read(std::move(reqs), d));
    }
    awaitable<io_result<buffers_type>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, d, reqs.buffers.connected_socket()));
    return ret;
  }
#endif
};

LLFIO_TEMPLATE(class T, class U)
LLFIO_TREQUIRES(LLFIO_TEXPR(static_cast<T *>(static_cast<handle *>(nullptr))))
T *socket_cast(listening_socket_handle_impl<U> *v)
{
  return static_cast<T *>(static_cast<handle *>(v));
}
LLFIO_TEMPLATE(class T, class U)
LLFIO_TREQUIRES(LLFIO_TEXPR(static_cast<const T *>(static_cast<const handle *>(nullptr))))
const T *socket_cast(const listening_socket_handle_impl<U> *v)
{
  return static_cast<const T *>(static_cast<const handle *>(v));
}

/* \class listening_socket_handle
\brief A handle to a socket-like entity able to receive incoming connections.
*/
class LLFIO_DECL listening_socket_handle : public listening_socket_handle_impl<byte_socket_handle>
{
  template <class ST> friend class LLFIO_V2_NAMESPACE::listening_socket_handle_impl;
  using _base = listening_socket_handle_impl<byte_socket_handle>;

protected:
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<buffers_type> _do_read(io_request<buffers_type> req, deadline d) noexcept override;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_multiplexer_read(io_request<buffers_type> reqs, deadline d) noexcept override
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    const auto state_reqs = _ctx->io_state_requirements();
    auto *storage = (byte *) alloca(state_reqs.first + state_reqs.second);
    const auto diff = (uintptr_t) storage & (state_reqs.second - 1);
    storage += state_reqs.second - diff;
    auto *state = _ctx->construct_and_init_io_operation({storage, state_reqs.first}, this, nullptr, d, reqs.buffers.connected_socket());
    if(state == nullptr)
    {
      return errc::resource_unavailable_try_again;
    }
    OUTCOME_TRY(_ctx->flush_inited_io_operations());
    while(!is_finished(_ctx->check_io_operation(state)))
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(_ctx->check_for_any_completed_io(nd));
    }
    OUTCOME_TRY(std::move(*state).get_completed_read());
    state->~io_operation_state();
    return {std::move(reqs.buffers)};
  }

public:
  constexpr listening_socket_handle() {}
  using _base::_base;
  listening_socket_handle(const listening_socket_handle &) = delete;
  listening_socket_handle(listening_socket_handle &&) = default;
  listening_socket_handle &operator=(const listening_socket_handle &) = delete;
  //! Move assignment of listening_socket_handle_impl permitted
  listening_socket_handle_impl &operator=(listening_socket_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
#ifdef _WIN32
    if(_v)
    {
      detail::unregister_socket_handle_instance(this);
    }
#endif
    this->~listening_socket_handle();
    new(this) listening_socket_handle(std::move(o));
    return *this;
  }

  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(listening_socket_handle &o) noexcept
  {
    listening_socket_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  virtual ~listening_socket_handle() override
  {
    if(_v)
    {
      (void) listening_socket_handle_impl::close();
    }
  }
  virtual result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_ctx != nullptr)
    {
      OUTCOME_TRY(set_multiplexer(nullptr));
    }
#ifndef NDEBUG
    if(_v)
    {
      // Tell handle::close() that we have correctly executed
      _v.behaviour |= native_handle_type::disposition::_child_close_executed;
    }
#endif
    auto ret = handle::close();
#ifdef _WIN32
    if(_v)
    {
      detail::unregister_socket_handle_instance(this);
    }
#endif
    return ret;
  }

  virtual result<void> set_multiplexer(byte_io_multiplexer *c = this_thread::multiplexer()) noexcept override
  {
    if(!is_multiplexable())
    {
      return errc::operation_not_supported;
    }
    if(c == _ctx)
    {
      return success();
    }
    if(_ctx != nullptr)
    {
      OUTCOME_TRY(_ctx->do_byte_io_handle_deregister(this));
      _ctx = nullptr;
    }
    if(c != nullptr)
    {
      OUTCOME_TRY(auto &&state, c->do_byte_io_handle_register(this));
      _v.behaviour = (_v.behaviour & ~(native_handle_type::disposition::_multiplexer_state_bit0 | native_handle_type::disposition::_multiplexer_state_bit1));
      if((state & 1) != 0)
      {
        _v.behaviour |= native_handle_type::disposition::_multiplexer_state_bit0;
      }
      if((state & 2) != 0)
      {
        _v.behaviour |= native_handle_type::disposition::_multiplexer_state_bit1;
      }
    }
    _ctx = c;
    return success();
  }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<ip::address> local_endpoint() const noexcept override;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> bind(const ip::address &addr, creation _creation = creation::only_if_not_exist,
                                                    int backlog = -1) noexcept override;

  /*! Create a listening socket handle.
  \param _family Which IP family to create the socket in.
  \param _mode How to open the socket. If this is `mode::append`, the read side of the socket
  is shutdown; if this is `mode::read`, the write side of the socket is shutdown.
  \param _caching How to ask the kernel to cache the socket. If writes are not cached,
  `SO_SNDBUF` to the minimum possible value and `TCP_NODELAY` is set, this should cause
  writes to hit the network as quickly as possible.
  \param flags Any additional custom behaviours.

  \errors Any of the values POSIX `socket()` or `WSASocket()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_socket_handle> listening_socket(ip::family _family, mode _mode = mode::write,
                                                                                          caching _caching = caching::all, flag flags = flag::none) noexcept;
  //! \brief Convenience function defaulting `flag::multiplexable` set.
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<listening_socket_handle>
  multiplexable_listening_socket(ip::family _family, mode _mode = mode::write, caching _caching = caching::all, flag flags = flag::multiplexable) noexcept
  {
    return listening_socket(_family, _mode, _caching, flags);
  }
};

//! \brief Constructor for `listening_socket_handle`
template <> struct construct<listening_socket_handle>
{
  ip::family family;
  byte_socket_handle::mode _mode = byte_socket_handle::mode::write;
  byte_socket_handle::caching _caching = byte_socket_handle::caching::all;
  byte_socket_handle::flag flags = byte_socket_handle::flag::none;
  result<listening_socket_handle> operator()() const noexcept { return listening_socket_handle::listening_socket(family, _mode, _caching, flags); }
};

// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/byte_socket_handle.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
