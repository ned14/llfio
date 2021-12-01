/* Wraps the platform specific i/o reference object
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
File Created: March 2016


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

#ifndef LLFIO_CONFIG_HPP
#error You must include the master llfio.hpp, not individual header files directly
#endif
#include "config.hpp"

//! \file native_handle_type.hpp Provides native_handle_type

#ifndef LLFIO_NATIVE_HANDLE_TYPE_H
#define LLFIO_NATIVE_HANDLE_TYPE_H

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

#pragma pack(push, 4)
/*! \struct native_handle_type
\brief A native handle type used for wrapping file descriptors, process ids or HANDLEs.
Unmanaged, wrap in a handle object to manage.
*/
struct native_handle_type  // NOLINT
{
  //! The type of handle.
  QUICKCPPLIB_BITFIELD_BEGIN(disposition){
  invalid = 0U,  //!< Invalid handle

  readable = 1U << 0U,     //!< Is readable
  writable = 1U << 1U,     //!< Is writable
  append_only = 1U << 2U,  //!< Is append only

  nonblocking = 1U << 4U,  //!< Requires additional synchronisation (Windows: `OVERLAPPED`; POSIX: `O_NONBLOCK`)
  seekable = 1U << 5U,     //!< Is seekable
  aligned_io = 1U << 6U,   //!< Requires sector aligned i/o (typically 512 or 4096)

  file = 1U << 8U,          //!< Is a regular file
  directory = 1U << 9U,     //!< Is a directory
  symlink = 1U << 10U,      //!< Is a symlink
  pipe = 1U << 11U,         //!< Is a pipe
  socket = 1U << 12U,       //!< Is a socket
  multiplexer = 1U << 13U,  //!< Is a kqueue/epoll/iocp
  process = 1U << 14U,      //!< Is a child process
  section = 1U << 15U,      //!< Is a memory section
  allocation = 1U << 16U,   //!< Is a memory allocation
  path = 1U << 17U,         //!< Is a path

  safety_barriers = 1U << 20U,  //!< Issue write reordering barriers at various points
  cache_metadata = 1U << 21U,   //!< Is serving metadata from the kernel cache
  cache_reads = 1U << 22U,      //!< Is serving reads from the kernel cache
  cache_writes = 1U << 23U,     //!< Is writing back from kernel cache rather than writing through
  cache_temporary = 1U << 24U,  //!< Writes are not flushed to storage quickly

  _is_connected = 1U << 28U,            // used by pipe_handle on Windows to store connectedness
  _multiplexer_state_bit0 = 1U << 29U,  // per-handle state bits used by an i/o multiplexer
  _multiplexer_state_bit1 = 1U << 30U,  // per-handle state bits used by an i/o multiplexer
  _child_close_executed = 1U << 31U     // used to trap when vptr has become corrupted
  } QUICKCPPLIB_BITFIELD_END(disposition)

  union
  {
    intptr_t _init{-1};
    //! A POSIX file descriptor
    int fd;  // NOLINT
    //! A POSIX process identifier
    int pid;  // NOLINT
    //! A Windows HANDLE
    win::handle h;  // NOLINT
    //! A third party pointer
    void *ptr;
  };
  disposition behaviour{disposition::invalid};  //! The behaviour of the handle
  //! Constructs a default instance
  constexpr native_handle_type()
      : _init{-1}
  {
  }  // NOLINT
  ~native_handle_type() = default;
  //! Construct from a POSIX file descriptor
  constexpr native_handle_type(disposition _behaviour, int _fd) noexcept
      : fd(_fd)
      , behaviour(_behaviour)
  {
  }  // NOLINT
  //! Construct from a Windows HANDLE
  constexpr native_handle_type(disposition _behaviour, win::handle _h) noexcept
      : h(_h)
      , behaviour(_behaviour)
  {
  }  // NOLINT

  //! Copy construct
  native_handle_type(const native_handle_type &) = default;
  //! Move construct
  constexpr native_handle_type(native_handle_type &&o) noexcept  // NOLINT
      : _init(o._init)
      , behaviour(o.behaviour)
  {
    o.behaviour = disposition();
    o._init = -1;
  }
  //! Copy assign
  native_handle_type &operator=(const native_handle_type &) = default;
  //! Move assign
  constexpr native_handle_type &operator=(native_handle_type &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    _init = o._init;
    behaviour = o.behaviour;
    o.behaviour = disposition();
    o._init = -1;
    return *this;
  }
  //! Swaps with another instance
  void swap(native_handle_type &o) noexcept
  {
    std::swap(_init, o._init);
    std::swap(behaviour, o.behaviour);
  }

  //! True if valid
  explicit constexpr operator bool() const noexcept { return is_valid(); }
  //! True if invalid
  constexpr bool operator!() const noexcept { return !is_valid(); }

  //! True if equal
  constexpr bool operator==(const native_handle_type &o) const noexcept { return behaviour == o.behaviour && _init == o._init; }
  //! True if unequal
  constexpr bool operator!=(const native_handle_type &o) const noexcept { return behaviour != o.behaviour || _init != o._init; }

  //! True if the handle is valid
  constexpr bool is_valid() const noexcept { return _init != -1 && static_cast<unsigned>(behaviour) != 0; }

  //! True if the handle is readable
  constexpr bool is_readable() const noexcept { return (behaviour & disposition::readable) ? true : false; }
  //! True if the handle is writable
  constexpr bool is_writable() const noexcept { return (behaviour & disposition::writable) ? true : false; }
  //! True if the handle is append only
  constexpr bool is_append_only() const noexcept { return (behaviour & disposition::append_only) ? true : false; }

  //! True if nonblocking
  constexpr bool is_nonblocking() const noexcept { return (behaviour & disposition::nonblocking) ? true : false; }
  //! True if seekable
  constexpr bool is_seekable() const noexcept { return (behaviour & disposition::seekable) ? true : false; }
  //! True if requires aligned i/o
  constexpr bool requires_aligned_io() const noexcept { return (behaviour & disposition::aligned_io) ? true : false; }

  //! True if a regular file or device
  constexpr bool is_regular() const noexcept { return (behaviour & disposition::file) ? true : false; }
  //! True if a directory
  constexpr bool is_directory() const noexcept { return (behaviour & disposition::directory) ? true : false; }
  //! True if a symlink
  constexpr bool is_symlink() const noexcept { return (behaviour & disposition::symlink) ? true : false; }
  //! True if a pipe
  constexpr bool is_pipe() const noexcept { return (behaviour & disposition::pipe) ? true : false; }
  //! True if a socket
  constexpr bool is_socket() const noexcept { return (behaviour & disposition::socket) ? true : false; }
  //! True if a multiplexer like BSD kqueues, Linux epoll or Windows IOCP
  constexpr bool is_multiplexer() const noexcept { return (behaviour & disposition::multiplexer) ? true : false; }
  //! True if a process
  constexpr bool is_process() const noexcept { return (behaviour & disposition::process) ? true : false; }
  //! True if a memory section
  constexpr bool is_section() const noexcept { return (behaviour & disposition::section) ? true : false; }
  //! True if a memory allocation
  constexpr bool is_allocation() const noexcept { return (behaviour & disposition::allocation) ? true : false; }
  //! True if a path or a directory
  constexpr bool is_path() const noexcept { return (behaviour & disposition::path) ? true : false; }
};
static_assert((sizeof(void *) == 4 && sizeof(native_handle_type) == 8) || (sizeof(void *) == 8 && sizeof(native_handle_type) == 12),
              "native_handle_type is not 8 or 12 bytes in size!");
// Not trivially copyable, as has non-trivial move.
static_assert(std::is_trivially_destructible<native_handle_type>::value, "native_handle_type is not trivially destructible!");
static_assert(std::is_trivially_copy_constructible<native_handle_type>::value, "native_handle_type is not trivially copy constructible!");
static_assert(std::is_trivially_copy_assignable<native_handle_type>::value, "native_handle_type is not trivially copy assignable!");

#pragma pack(pop)

LLFIO_V2_NAMESPACE_END


#endif
