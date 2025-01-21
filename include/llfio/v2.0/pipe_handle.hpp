/* A handle to a pipe
(C) 2015-2019 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Nov 2019


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

#ifndef LLFIO_PIPE_HANDLE_H
#define LLFIO_PIPE_HANDLE_H

#include "byte_io_handle.hpp"

//! \file pipe_handle.hpp Provides `pipe_handle`

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class pipe_handle
\brief A handle to a named or anonymous pipe.

The only fully portable use of this class is to *create* a named pipe with
read-only privileges (`pipe_create()`), and then to *open* an existing named pipe with
append-only privileges (`pipe_open()`). This ordering is important - it
works irrespective of whether the pipe is multiplexable or not.

This class doesn't prevent you opening fully duplex pipes
(i.e. `mode::write`) if your system supports them, but semantics in this
situation are implementation defined. Linux and Windows support fully
duplex pipes, and the Windows implementation matches the Linux bespoke
semantics.

For the static functions which create a pipe, `flag::unlink_on_first_close`
is the default. This is due to portability reasons -
on some platforms (e.g. Windows), named pipes always get deleted when
the last handle to them is closed in the system, so the closest
matching semantic on POSIX is for the creating handle to unlink its creation on
first close on all platforms. If you don't want this, change the flags
given during creation. Note that on Windows, `flag::unlink_on_first_close`
is always masked out, this is because Windows appears to not permit
renaming nor unlinking of open pipes.

If `flag::multiplexable` is specified which causes the handle to
be created as `native_handle_type::disposition::nonblocking`, opening
pipes for reads no longer blocks in the constructor. However it will then
block in `read()`, unless its deadline is zero. Opening pipes for write
in nonblocking mode will now fail if there is no reader present on the
other side of the pipe.

\warning On POSIX neither `creation::only_if_not_exist` nor
`creation::always_new` is atomic due to lack of kernel API support.

# Windows only

On Microsoft Windows, all pipes (including anonymous) are created within
the `\Device\NamedPipe\` region within the NT kernel namespace, which is the
ONLY place where pipes can exist on Windows (i.e. you cannot place them in
the filing system like on POSIX).

Because pipes can only exist in a single, global namespace shared amongst
all applications, and this is the same whether for Win32 or the NT kernel,
`pipe_handle` does not bother implementing the `\!!\` extension which forces
use of the NT kernel API. All named pipes always operate out of the NT kernel
namespace. `.parent_path_handle()` always returns
`path_discovery::temporary_named_pipes_directory()` on Windows.

So long as you use `path_discovery::temporary_named_pipes_directory()`
as your base directory, you can write quite portable code between POSIX
and Windows.
*/
class LLFIO_DECL pipe_handle : public byte_io_handle,
                               public fs_handle
#ifndef _WIN32
    ,
                               public pollable_handle
#endif
{
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> _replace_handle(handle &&o_) noexcept override
  {
    auto &o = static_cast<decltype(*this) &>(o_);
    if(_v.behaviour != o._v.behaviour)
    {
      return errc::invalid_argument;
    }
    _v.swap(o._v);
    return success();
  }

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
  using dev_t = fs_handle::dev_t;
  using ino_t = fs_handle::ino_t;
  using path_view_type = fs_handle::path_view_type;

public:
  //! Default constructor
  constexpr pipe_handle() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  constexpr pipe_handle(native_handle_type h, dev_t devid, ino_t inode, flag flags, byte_io_multiplexer *ctx)
      : byte_io_handle(std::move(h), flags, ctx)
      , fs_handle(devid, inode)
  {
  }
  //! Construct a handle from a supplied native handle
  constexpr pipe_handle(native_handle_type h, flag flags, byte_io_multiplexer *ctx)
      : byte_io_handle(std::move(h), flags, ctx)
  {
  }
  //! No copy construction (use clone())
  pipe_handle(const pipe_handle &) = delete;
  //! No copy assignment
  pipe_handle &operator=(const pipe_handle &) = delete;
  //! Implicit move construction of `pipe_handle` permitted
  constexpr pipe_handle(pipe_handle &&o) noexcept
      : byte_io_handle(std::move(o))
      , fs_handle(std::move(o))
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr pipe_handle(handle &&o, dev_t devid, ino_t inode, byte_io_multiplexer *ctx) noexcept
      : byte_io_handle(std::move(o), ctx)
      , fs_handle(devid, inode)
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr pipe_handle(handle &&o, byte_io_multiplexer *ctx) noexcept
      : byte_io_handle(std::move(o), ctx)
  {
  }
  //! Explicit conversion from byte_io_handle permitted
  explicit constexpr pipe_handle(byte_io_handle &&o, dev_t devid, ino_t inode) noexcept
      : byte_io_handle(std::move(o))
      , fs_handle(devid, inode)
  {
  }
  //! Explicit conversion from byte_io_handle permitted
  explicit constexpr pipe_handle(byte_io_handle &&o) noexcept
      : byte_io_handle(std::move(o))
  {
  }
  //! Move assignment of `pipe_handle` permitted
  pipe_handle &operator=(pipe_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~pipe_handle();
    new(this) pipe_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(pipe_handle &o) noexcept
  {
    pipe_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create a pipe handle opening access to a named pipe
  \param path The path relative to base to open.
  \param _mode How to open the pipe.
  \param _creation How to create the pipe.
  \param _caching How to ask the kernel to cache the pipe.
  \param flags Any additional custom behaviours.
  \param base Handle to a base location on the filing system.
  Defaults to `path_discovery::temporary_named_pipes_directory()`.

  \errors Any of the values POSIX `open()`, `mkfifo()`, `NtCreateFile()` or `NtCreateNamedPipeFile()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<pipe_handle> pipe(path_view_type path, mode _mode, creation _creation, caching _caching = caching::all,
                                                                  flag flags = flag::none,
                                                                  const path_handle &base = path_discovery::temporary_named_pipes_directory()) noexcept;
  /*! Convenience overload for `pipe()` creating a new named pipe if
  needed, and with read-only privileges. Unless `flag::multiplexable`
  is specified, this will block until the other end connects.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<pipe_handle> pipe_create(path_view_type path, caching _caching = caching::all, flag flags = flag::unlink_on_first_close,
                                                const path_handle &base = path_discovery::temporary_named_pipes_directory()) noexcept
  {
    return pipe(path, mode::read, creation::if_needed, _caching, flags, base);
  }
  /*! Convenience overload for `pipe()` opening an existing named pipe
  with write-only privileges. This will fail if no reader is waiting
  on the other end of the pipe.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<pipe_handle> pipe_open(path_view_type path, caching _caching = caching::all, flag flags = flag::none,
                                              const path_handle &base = path_discovery::temporary_named_pipes_directory()) noexcept
  {
    return pipe(path, mode::append, creation::open_existing, _caching, flags, base);
  }

  /*! Create a pipe handle creating a randomly named pipe on a path.
  The pipe is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing pipe.

  \errors Any of the values POSIX `open()`, `mkfifo()`, `NtCreateFile()` or `NtCreateNamedPipeFile()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<pipe_handle> random_pipe(mode _mode = mode::read, caching _caching = caching::all, flag flags = flag::unlink_on_first_close,
                                                const path_handle &dirpath = path_discovery::temporary_named_pipes_directory()) noexcept
  {
    LLFIO_TRY
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<pipe_handle> ret = pipe(randomname, _mode, creation::only_if_not_exist, _caching, flags, dirpath);
        if(ret || (!ret && ret.error() != errc::file_exists))
        {
          return ret;
        }
      }
    }
    LLFIO_CATCH(...)
    {
      return error_from_exception();
    }
  }
  /*! \em Securely create two ends of an anonymous pipe handle. The first
  handle returned is the read end; the second is the write end.

  Unlike Windows' `CreatePipe()`, this function can create non-blocking
  anonymous pipes. These are truly anonymous, not just randomly named.

  \errors Any of the values POSIX `pipe()` or `NtCreateNamedPipeFile()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<std::pair<pipe_handle, pipe_handle>> anonymous_pipe(caching _caching = caching::all,
                                                                                                    flag flags = flag::none) noexcept;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~pipe_handle() override
  {
    if(_v)
    {
      (void) pipe_handle::close();
    }
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_.flags & flag::unlink_on_first_close)
    {
      auto ret = unlink();
      if(!ret)
      {
        // File may have already been deleted, if so ignore
        if(ret.error() != errc::no_such_file_or_directory)
        {
          return std::move(ret).error();
        }
      }
    }
#ifndef NDEBUG
    if(_v)
    {
      // Tell handle::close() that we have correctly executed
      _v.behaviour |= native_handle_type::disposition::_child_close_executed;
    }
#endif
    return byte_io_handle::close();
  }

#ifdef _WIN32
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_handle> parent_path_handle(deadline /*unused*/ = std::chrono::seconds(30)) const noexcept override
  {
    // Pipes parent handle is always the NT kernel namespace for pipes
    return path_discovery::temporary_named_pipes_directory().clone_to_path_handle();
  }

protected:
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override;

public:
#endif
};

//! \brief Constructor for `pipe_handle`
template <> struct construct<pipe_handle>
{
  pipe_handle::path_view_type _path;
  pipe_handle::mode _mode = pipe_handle::mode::read;
  pipe_handle::creation _creation = pipe_handle::creation::if_needed;
  pipe_handle::caching _caching = pipe_handle::caching::all;
  pipe_handle::flag flags = pipe_handle::flag::none;
  const path_handle &base = path_discovery::temporary_named_pipes_directory();
  result<pipe_handle> operator()() const noexcept { return pipe_handle::pipe(_path, _mode, _creation, _caching, flags, base); }
};

// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/pipe_handle.ipp"
#else
#include "detail/impl/posix/pipe_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif
