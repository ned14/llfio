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

#ifndef AFIO_CONFIG_HPP
#error You must include the master afio.hpp, not individual header files directly
#endif
#include "config.hpp"

//! \file native_handle_type.hpp Provides native_handle_type

#ifndef AFIO_NATIVE_HANDLE_TYPE_H
#define AFIO_NATIVE_HANDLE_TYPE_H

AFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \struct native_handle_type
\brief A native handle type used for wrapping file descriptors, process ids or HANDLEs.
Unmanaged, wrap in a handle object to manage.
*/
struct native_handle_type
{
  //! The type of handle.
  QUICKCPPLIB_BITFIELD_BEGIN(disposition)
  {
    invalid = 0,  //!< Invalid handle

    readable = 1 << 0,     //!< Is readable
    writable = 1 << 1,     //!< Is writable
    append_only = 1 << 2,  //!< Is append only

    overlapped = 1 << 4,  //!< Requires additional synchronisation
    seekable = 1 << 5,    //!< Is seekable
    aligned_io = 1 << 6,  //!< Requires sector aligned i/o (typically 512 or 4096)

    file = 1 << 8,          //!< Is a regular file
    directory = 1 << 9,     //!< Is a directory
    symlink = 1 << 10,      //!< Is a symlink
    multiplexer = 1 << 11,  //!< Is a kqueue/epoll/iocp
    process = 1 << 12,      //!< Is a child process
    section = 1 << 13       //!< Is a memory section
  }
  QUICKCPPLIB_BITFIELD_END(disposition)
  disposition behaviour;  //! The behaviour of the handle
  union {
    intptr_t _init;
    int fd;         //!< A POSIX file descriptor
    int pid;        //!< A POSIX process identifier
    win::handle h;  //!< A Windows HANDLE
  };
  //! Constructs a default instance
  constexpr native_handle_type() noexcept : behaviour(), _init(-1) {}
  //! Construct from a POSIX file descriptor
  constexpr native_handle_type(disposition _behaviour, int _fd) noexcept : behaviour(_behaviour), fd(_fd) {}
  //! Construct from a Windows HANDLE
  constexpr native_handle_type(disposition _behaviour, win::handle _h) noexcept : behaviour(_behaviour), h(_h) {}

  //! Copy construct
  constexpr native_handle_type(const native_handle_type &) = default;
  //! Move construct
  constexpr native_handle_type(native_handle_type &&o) noexcept : behaviour(std::move(o.behaviour)), _init(std::move(o._init))
  {
    o.behaviour = disposition();
    o._init = -1;
  }
  //! Copy assign
  native_handle_type &operator=(const native_handle_type &) = default;
  //! Move assign
  native_handle_type &operator=(native_handle_type &&o) noexcept
  {
    behaviour = std::move(o.behaviour);
    _init = std::move(o._init);
    o.behaviour = disposition();
    o._init = -1;
    return *this;
  }
  //! Swaps with another instance
  void swap(native_handle_type &o) noexcept
  {
    std::swap(behaviour, o.behaviour);
    std::swap(_init, o._init);
  }

  //! True if valid
  explicit constexpr operator bool() const noexcept { return is_valid(); }
  //! True if invalid
  constexpr bool operator!() const noexcept { return !is_valid(); }

  //! True if the handle is valid
  constexpr bool is_valid() const noexcept { return _init != -1 && static_cast<unsigned>(behaviour) != 0; }

  //! True if the handle is readable
  constexpr bool is_readable() const noexcept { return (behaviour & disposition::readable) ? true : false; }
  //! True if the handle is writable
  constexpr bool is_writable() const noexcept { return (behaviour & disposition::writable) ? true : false; }
  //! True if the handle is append only
  constexpr bool is_append_only() const noexcept { return (behaviour & disposition::append_only) ? true : false; }

  //! True if overlapped
  constexpr bool is_overlapped() const noexcept { return (behaviour & disposition::overlapped) ? true : false; }
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
  //! True if a multiplexer like BSD kqueues, Linux epoll or Windows IOCP
  constexpr bool is_multiplexer() const noexcept { return (behaviour & disposition::multiplexer) ? true : false; }
  //! True if a process
  constexpr bool is_process() const noexcept { return (behaviour & disposition::process) ? true : false; }
  //! True if a memory section
  constexpr bool is_section() const noexcept { return (behaviour & disposition::section) ? true : false; }
};

AFIO_V2_NAMESPACE_END


#endif
