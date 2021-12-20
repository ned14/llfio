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

//! \file byte_socket_handle.hpp Provides `byte_socket_handle`.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nameless struct/union
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

//! Inspired by ASIO's `ip` namespace
namespace ip
{
  /*! \class address
  \brief A version independent IP address.

  This is pretty close to `asio::ip::address`, but it also adds `port()` from `asio::ip::endpoint`
  and a few other observer member functions i.e. it fuses ASIO's many types into one.

  The reason why is that this type is a simple wrap of `struct sockaddr_in` or `struct sockaddr_in6`,
  it doesn't split those structures.
  */
  class LLFIO_DECL address
  {
  protected:
    union
    {
      struct
      {
        unsigned short _family;  // sa_family_t
        uint16_t _port;          // in_port_t
        union
        {
          struct
          {
            uint32_t _flowinfo{0};
            byte _addr[16]{(byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0,
                           (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0, (byte) 0};
            uint32_t _scope_id{0};
          } ipv6;
          union
          {
            byte _addr[4];
            uint32_t _addr_be;
          } ipv4;
        };
      };
      byte _storage[32];  // struct sockaddr_?
    };

  public:
    constexpr address() noexcept
        : _family(0)
        , _port(0)
        , ipv4{}
    {
    }
    address(int family, const byte *storage) noexcept { memcpy(_storage, storage, sizeof(_storage)); }
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
    //! True if address is unspecified
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_unspecified() const noexcept;
    //! True if address is v4
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_v4() const noexcept;
    //! True if address is v6
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_v6() const noexcept;

    //! Returns the raw family of the address
    unsigned short family() const noexcept { return _family; }
    //! Returns the port of the address
    uint16_t port() const noexcept { return _port; }
    //! Returns the IPv6 flow info, if address is v6.
    uint32_t flowinfo() const noexcept { return is_v6() ? ipv6._flowinfo : 0; }
    //! Returns the IPv6 scope id, if address is v6.
    uint32_t scope_id() const noexcept { return is_v6() ? ipv6._scope_id : 0; }

    //! Returns the bytes of the address in network order
    span<const byte> as_bytes() const noexcept { return is_v6() ? span<const byte>(ipv6._addr) : span<const byte>(ipv4._addr); }
  };
  //! Write address to stream
  LLFIO_HEADERS_ONLY_FUNC_SPEC std::ostream &operator<<(std::ostream &s, const address &v);
  /*! \class address_v4
  \brief A v4 IP address.
  */
  class LLFIO_DECL address_v4 : public address
  {
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
  static_assert(std::is_trivially_copyable<address>::value, "ip::address is not trivially copyable!");
  //! Make an `address_v4`
  inline result<address_v4> make_address_v4(const address_v4::bytes_type &bytes, uint16_t port = 0) noexcept { return address_v4(bytes, port); }
  //! Make an `address_v4`
  inline result<address_v4> make_address_v4(const address_v4::uint_type &bytes, uint16_t port = 0) noexcept { return address_v4(bytes, port); }
  //! Make an `address_v4`
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v4> make_address_v4(string_view str) noexcept;
  /*! \class address_v6
  \brief A v6 IP address.
  */
  class LLFIO_DECL address_v6 : public address
  {
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
  //! Make an `address_v6`
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<address_v6> make_address_v6(string_view str) noexcept;
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

If `flag::multiplexable` is specified which causes the handle to
be created as `native_handle_type::disposition::nonblocking`, opening
sockets for reads no longer blocks in the constructor. However it will then
block in `read()`, unless its deadline is zero. Opening sockets for write
in nonblocking mode will now fail if there is no reader present on the
other side of the socket.
*/
class LLFIO_DECL byte_socket_handle : public byte_io_handle
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

public:
  //! Default constructor
  constexpr byte_socket_handle() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  constexpr byte_socket_handle(native_handle_type h, caching caching, flag flags, byte_io_multiplexer *ctx)
      : byte_io_handle(std::move(h), caching, flags, ctx)
  {
  }
  //! No copy construction (use clone())
  byte_socket_handle(const byte_socket_handle &) = delete;
  //! No copy assignment
  byte_socket_handle &operator=(const byte_socket_handle &) = delete;
  //! Implicit move construction of byte_socket_handle permitted
  constexpr byte_socket_handle(byte_socket_handle &&o) noexcept
      : byte_io_handle(std::move(o))
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr byte_socket_handle(handle &&o, byte_io_multiplexer *ctx) noexcept
      : byte_io_handle(std::move(o), ctx)
  {
  }
  //! Explicit conversion from byte_io_handle permitted
  explicit constexpr byte_socket_handle(byte_io_handle &&o) noexcept
      : byte_io_handle(std::move(o))
  {
  }
  //! Move assignment of byte_socket_handle permitted
  byte_socket_handle &operator=(byte_socket_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
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

  //! Returns the local endpoint of this socket instance
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const ip::address &local_endpoint() const noexcept;
  //! Returns the remote endpoint of this socket instance
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const ip::address &remote_endpoint() const noexcept;

  /*! Create a socket handle connecting to a specified address.
  \param address The address to create (`creation::only_if_not_exist|creation::always_new`)  or connect to.
  \param _mode How to open the socket.
  \param _creation How to create the socket.
  \param _caching How to ask the kernel to cache the socket.
  \param flags Any additional custom behaviours.

  \errors Any of the values POSIX `socket()`or `WSASocket()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<byte_socket_handle> byte_socket(const ip::address &addr, mode _mode, creation _creation,
                                                                                caching _caching = caching::all, flag flags = flag::none) noexcept;
  /*! Convenience overload for `byte_socket()` creating a new local endpoint.
  Unless `flag::multiplexable` is specified, this will block until the other end connects.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<byte_socket_handle> endpoint_create(const ip::address &addr = {}, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    return byte_socket(addr, mode::read, creation::only_if_not_exist, _caching, flags);
  }
  /*! Convenience overload for `byte_socket()` opening an existing remote endpoint.
  This will fail if no reader is waiting on the other end of the socket.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<byte_socket_handle> endpoint_open(const ip::address &addr, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    return byte_socket(addr, mode::append, creation::open_existing, _caching, flags);
  }

  /*! \em Securely create two ends of an anonymous socket handle. The first
  handle returned is the read end; the second is the write end.

  \errors Any of the values POSIX `socketpair()` or `WSASocket()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<std::pair<byte_socket_handle, byte_socket_handle>> anonymous_socket(caching _caching = caching::all,
                                                                                                                    flag flags = flag::none) noexcept;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~byte_socket_handle() override
  {
    if(_v)
    {
      (void) byte_socket_handle::close();
    }
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
#ifndef NDEBUG
    if(_v)
    {
      // Tell handle::close() that we have correctly executed
      _v.behaviour |= native_handle_type::disposition::_child_close_executed;
    }
#endif
    return byte_io_handle::close();
  }
};

//! \brief Constructor for `byte_socket_handle`
template <> struct construct<byte_socket_handle>
{
  const ip::address &addr;
  byte_socket_handle::mode _mode = byte_socket_handle::mode::read;
  byte_socket_handle::creation _creation = byte_socket_handle::creation::open_existing;
  byte_socket_handle::caching _caching = byte_socket_handle::caching::all;
  byte_socket_handle::flag flags = byte_socket_handle::flag::none;
  result<byte_socket_handle> operator()() const noexcept { return byte_socket_handle::byte_socket(addr, _mode, _creation, _caching, flags); }
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
