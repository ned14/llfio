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

#include "io_handle.hpp"

//! \file pipe_handle.hpp Provides `pipe_handle`

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class pipe_handle
\brief A handle to a named or anonymous pipe.

Note that `flag::unlink_on_first_close` is always on for
handles created by this class. This is due to portability reasons -
on some platforms (e.g. Windows), named pipes always get deleted when
the last handle to them is closed in the system, so the closest
matching semantic is to unlink them on first close on all platforms.
If you don't want this, release the native handle before closing the
handle instance, and close it manually.

For similar portability reasons, we do not expose the ability to rename
or unlink named pipes, as these operations are not possible on some
platforms (Windows). Similarly, portably determining a unique id for
the pipe which does not clash with file unique id's is problematic.

Be aware that `mode::write` opens a pipe in full duplex mode -- generally,
you don't want full duplex pipes (and indeed some systems don't support
them or have weird semantics with them), so if you want a write-only pipe,
specify `mode::append` instead.

Unless `flag::multiplexable` is specified which causes the handle to
be created as `native_handle_type::disposition::nonblocking`,
creating or opening a pipe handle with only read privileges blocks until
the other end is opened with write privileges. Similarly, creating or
opening a pipe handle with only write privileges fails unless
the other end is opened with read privileges. This means that there is
a potential race between initiating whomever will do a write to a pipe,
and you opening the pipe for reads -- you may wish to thus loop opening
a pipe for writing, checking for an error code comparing equal to
`errc::no_such_device_or_address`. Note that creating or opening a
pipe handle with both read and write privileges has implementation defined
semantics, as POSIX does not define what happens.

\warning On POSIX neither `creation::only_if_not_exist` nor
`creation::always_new` is atomic due to lack of kernel API support.
*/
class LLFIO_DECL pipe_handle : public io_handle, public fs_handle
{
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC const handle &_get_handle() const noexcept final { return *this; }

  void _set_is_connected(bool v) noexcept { this->_spare1 = v; }
  bool _is_connected() const noexcept { return this->_spare1 != 0; }

public:
  using path_type = io_handle::path_type;
  using extent_type = io_handle::extent_type;
  using size_type = io_handle::size_type;
  using mode = io_handle::mode;
  using creation = io_handle::creation;
  using caching = io_handle::caching;
  using flag = io_handle::flag;
  using buffer_type = io_handle::buffer_type;
  using const_buffer_type = io_handle::const_buffer_type;
  using buffers_type = io_handle::buffers_type;
  using const_buffers_type = io_handle::const_buffers_type;
  template <class T> using io_request = io_handle::io_request<T>;
  template <class T> using io_result = io_handle::io_result<T>;
  using dev_t = fs_handle::dev_t;
  using ino_t = fs_handle::ino_t;
  using path_view_type = fs_handle::path_view_type;

public:
  //! Default constructor
  constexpr pipe_handle() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  constexpr pipe_handle(native_handle_type h, dev_t devid, ino_t inode, caching caching = caching::none, flag flags = flag::none, io_context *ctx = nullptr)
      : io_handle(std::move(h), caching, flags, ctx)
      , fs_handle(devid, inode)
  {
  }
  //! No copy construction (use clone())
  pipe_handle(const pipe_handle &) = delete;
  //! No copy assignment
  pipe_handle &operator=(const pipe_handle &) = delete;
  //! Implicit move construction of `pipe_handle` permitted
  constexpr pipe_handle(pipe_handle &&o) noexcept
      : io_handle(std::move(o))
      , fs_handle(std::move(o))
  {
  }
  //! Explicit conversion from handle and io_handle permitted
  explicit constexpr pipe_handle(handle &&o, dev_t devid, ino_t inode) noexcept
      : io_handle(std::move(o))
      , fs_handle(devid, inode)
  {
  }
  //! Move assignment of `pipe_handle` permitted
  pipe_handle &operator=(pipe_handle &&o) noexcept
  {
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
  \param base Handle to a base location on the filing system.
  Pass `{}` to indicate that path will be absolute. Will be ignored on
  some platforms (Windows).
  \param path The path relative to base to open.
  \param _mode How to open the pipe.
  \param _creation How to create the pipe.
  \param _caching How to ask the kernel to cache the pipe.
  \param flags Any additional custom behaviours.

  \errors Any of the values POSIX open(), mkfifo(), CreateFile() or CreateNamedPipe() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<pipe_handle> pipe(const path_handle &base, path_view_type path, mode _mode = mode::write, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept;
  /*! Create a pipe handle creating a randomly named pipe on a path.
  The pipe is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing pipe.

  \errors Any of the values POSIX open(), mkfifo(), CreateFile() or CreateNamedPipe() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<pipe_handle> random_pipe(const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<pipe_handle> ret = pipe(dirpath, randomname, _mode, creation::only_if_not_exist, _caching, flags);
        if(ret || (!ret && ret.error() != errc::file_exists))
        {
          return ret;
        }
      }
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
  /*! Create a pipe handle creating the named pipe on some path which
  the OS declares to be suitable for temporary files.
  Note also that an empty name is equivalent to calling
  `random_pipe(path_discovery::storage_backed_temporary_files_directory())` and the creation
  parameter is ignored.

  \note If the temporary pipe you are creating is not going to have its
  path sent to another process for usage, this is the WRONG function
  to use. Use `anonymous_pipe()` instead, it is far more secure.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<pipe_handle> temp_pipe(path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    auto &tempdirh = path_discovery::storage_backed_temporary_files_directory();
    return name.empty() ? random_pipe(tempdirh, _mode, _caching, flags) : pipe(tempdirh, name, _mode, _creation, _caching, flags);
  }
  /*! \em Securely create two ends of an anonymous pipe handle. The first
  handle returned is the read end; the second is the write end.

  \errors Any of the values POSIX pipe() or CreatePipe() can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<std::pair<pipe_handle, pipe_handle>> anonymous_pipe(caching _caching = caching::all, flag flags = flag::none) noexcept;

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
    if(_flags & flag::unlink_on_first_close)
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
    return io_handle::close();
  }

  using io_handle::read;
  using io_handle::write;
#ifdef _WIN32
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override;
#endif
  //! Convenience initialiser list based overload for `read()`
  LLFIO_MAKE_FREE_FUNCTION
  io_result<size_type> read(extent_type offset, std::initializer_list<buffer_type> lst, deadline d = deadline()) noexcept
  {
    buffer_type *_reqs = reinterpret_cast<buffer_type *>(alloca(sizeof(buffer_type) * lst.size()));
    memcpy(_reqs, lst.begin(), sizeof(buffer_type) * lst.size());
    io_request<buffers_type> reqs(buffers_type(_reqs, lst.size()), offset);
    auto ret = read(reqs, d);
    if(ret)
    {
      return ret.bytes_transferred();
    }
    return std::move(ret).error();
  }

  LLFIO_DEADLINE_TRY_FOR_UNTIL(read)
};

//! \brief Constructor for `pipe_handle`
template <> struct construct<pipe_handle>
{
  const path_handle &base;
  pipe_handle::path_view_type _path;
  pipe_handle::mode _mode = pipe_handle::mode::read;
  pipe_handle::creation _creation = pipe_handle::creation::open_existing;
  pipe_handle::caching _caching = pipe_handle::caching::all;
  pipe_handle::flag flags = pipe_handle::flag::none;
  result<pipe_handle> operator()() const noexcept { return pipe_handle::pipe(base, _path, _mode, _creation, _caching, flags); }
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
