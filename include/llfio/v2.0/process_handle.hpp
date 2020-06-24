/* A handle to a process
(C) 2016-2020 Niall Douglas <http://www.nedproductions.biz/> (5 commits)
File Created: Mar 2017


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

#ifndef LLFIO_PROCESS_HANDLE_H
#define LLFIO_PROCESS_HANDLE_H

#include "path_view.hpp"
#include "pipe_handle.hpp"

//! \file process_handle.hpp Provides a handle to a process

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class process_handle
\brief A handle to this, or another, process.
*/
class LLFIO_DECL process_handle : public handle
{
public:
  using path_type = handle::path_type;
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;

  //! The behaviour of theprocess handle
  QUICKCPPLIB_BITFIELD_BEGIN(flag){none = 0U,                          //!< No flags
                                   no_redirect_in_pipe = 1U << 1U,     //!< Do not redirect the `stdin` for a launched process
                                   no_redirect_out_pipe = 1U << 2U,    //!< Do not redirect the `stdout` for a launched process
                                   no_redirect_error_pipe = 1U << 3U,  //!< Do not redirect the `stderr` for a launched process

                                   wait_on_close = 1U << 4U,           //!< Wait for the process to exit in `.close()`
                                   release_pipes_on_close = 1U << 5U,  //!< Release the pipes in `.close()`. They are closed otherwise.
                                   no_multiplexable_pipes = 1U << 6U,  //!< Do not create any redirected pipes as multiplexable

                                   // NOTE: IF UPDATING THIS UPDATE THE std::ostream PRINTER BELOW!!!
                                   no_redirect = no_redirect_in_pipe | no_redirect_out_pipe | no_redirect_error_pipe} QUICKCPPLIB_BITFIELD_END(flag);

protected:
  flag _flags{flag::none};
  pipe_handle _in_pipe, _out_pipe, _error_pipe;

  struct _byte_array_deleter
  {
    template <class T> void operator()(T *a) { delete[] reinterpret_cast<byte *>(a); }
  };

public:
  //! Default constructor
  constexpr process_handle() {}  // NOLINT
  //! Construct a handle from a supplied native handle
  explicit constexpr process_handle(native_handle_type h, flag flags = flag::none) noexcept
      : handle(std::move(h))
      , _flags(flags)
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr process_handle(handle &&o, flag flags = flag::none) noexcept
      : handle(std::move(o))
      , _flags(flags)
  {
  }
  virtual ~process_handle() override
  {
    (void) close_pipes();
    if(_v)
    {
      (void) process_handle::close();
      _v = {};
    }
  }
  //! No copy construction (use clone())
  process_handle(const process_handle &) = delete;
  //! No copy assignment
  process_handle &operator=(const process_handle &o) = delete;
  //! Move the handle.
  constexpr process_handle(process_handle &&o) noexcept
      : handle(std::move(o))
      , _flags(o._flags)
      , _in_pipe(std::move(o._in_pipe))
      , _out_pipe(std::move(o._out_pipe))
      , _error_pipe(std::move(o._error_pipe))
  {
  }
  //! Move assignment of handle
  process_handle &operator=(process_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~process_handle();
    new(this) process_handle(std::move(o));
    return *this;
  }
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(process_handle &o) noexcept
  {
    process_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! A pipe with which one can read what another process writes to `stdout`,
  or by which this process can read from `stdin`. Therefore always read-only.
  */
  pipe_handle &in_pipe() noexcept { return _in_pipe; }
  //! \overload
  const pipe_handle &in_pipe() const noexcept { return _in_pipe; }
  /*! A pipe with which one can read what another process writes to `stderr`,
  or by which this process can write to `stderr`.
  */
  pipe_handle &error_pipe() noexcept { return _error_pipe; }
  //! \overload
  const pipe_handle &error_pipe() const noexcept { return _error_pipe; }
  /*! A pipe with which one can write what another process reads from `stdin`,
  or by which this process can write to `stdout`. Therefore always write-only.
  */
  pipe_handle &out_pipe() noexcept { return _out_pipe; }
  //! \overload
  const pipe_handle &out_pipe() const noexcept { return _out_pipe; }
  //! Close or release all the pipes, depending on `flag::release_pipes_on_close`
  result<void> close_pipes() noexcept
  {
    if(!(_flags & flag::release_pipes_on_close))
    {
      // Close the other's side's write pipes first, as closing its
      // read pipe may cause it to immediately exit
      if(_in_pipe.is_valid())
      {
        OUTCOME_TRY(_in_pipe.close());
      }
      if(_error_pipe.is_valid())
      {
        OUTCOME_TRY(_error_pipe.close());
      }
      if(_out_pipe.is_valid())
      {
        OUTCOME_TRY(_out_pipe.close());
      }
    }
    else
    {
      _in_pipe.release();
      _error_pipe.release();
      _out_pipe.release();
    }
    return success();
  }

  //! Returns true if the process is currently running
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool is_running() const noexcept;

  /*! Returns the current path of the binary the process is executing.

  \note If the *current* path cannot be retrieved on this platform
  (i.e. invariant to concurrent filesystem modification), an empty
  path is returned. This includes the current process.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_type> current_path() const noexcept override;
  //! Immediately close this handle, possibly closing the pipes, possibly blocking until a child process exits.
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override;
  /*! Clone this handle (copy constructor is disabled to avoid accidental copying)

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle> clone() const noexcept;

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> set_append_only(bool /*unused*/) noexcept override { return errc::operation_not_supported; }

  /*! Retrieves the current environment of the process.

  \note If the *current* environment cannot be retrieved on this platform,
  a null pointer is returned.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC std::unique_ptr<span<path_view_component>, _byte_array_deleter> environment() const noexcept;

  /*! Waits until a process exits, returning its exit code. */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<intptr_t> wait(deadline d = {}) const noexcept;

  LLFIO_DEADLINE_TRY_FOR_UNTIL(wait)

  /*! Return a process handle referring to the current process.
   */
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const process_handle &current() noexcept;

  /*! Create a new process launching the binary at `path`.
  \param path The absolute path to the binary to launch.
  \param args An array of arguments to pass to the process.
  \param env An array of environment variables to set for the process, which
  defaults to the current process' environment.
  \param flags Any additional custom behaviours.

  This is the only handle creation function in LLFIO which requires an
  absolute path. This is because no platform implements race-free
  process launch, and worse, there is no way of non-intrusively
  emulating race-free process launch either. So we accept the inevitable,
  launching child processes is always racy with respect to concurrent
  filesystem modification.

  \errors Any of the values POSIX `posix_spawn()` or `CreateProcess()` can return.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle> launch_process(path_view path, span<path_view_component> args, span<path_view_component> env = *current().environment(), flag flags = flag::wait_on_close) noexcept;
  //! \overload
  static result<process_handle> launch_process(path_view path, span<path_view_component> args, flag flags = flag::wait_on_close) noexcept { return launch_process(path, args, *current().environment(), flags); }
};

inline std::ostream &operator<<(std::ostream &s, const process_handle::flag &v)
{
  std::string temp;
  if(!!(v & process_handle::flag::no_redirect_in_pipe))
  {
    temp.append("no_redirect_in_pipe|");
  }
  if(!!(v & process_handle::flag::no_redirect_out_pipe))
  {
    temp.append("no_redirect_out_pipe|");
  }
  if(!!(v & process_handle::flag::no_redirect_error_pipe))
  {
    temp.append("no_redirect_error_pipe|");
  }
  if(!!(v & process_handle::flag::wait_on_close))
  {
    temp.append("wait_on_close|");
  }
  if(!!(v & process_handle::flag::release_pipes_on_close))
  {
    temp.append("release_pipes_on_close|");
  }
  if(!!(v & process_handle::flag::no_multiplexable_pipes))
  {
    temp.append("no_multiplexable_pipes|");
  }
  if(!temp.empty())
  {
    temp.resize(temp.size() - 1);
    if(std::count(temp.cbegin(), temp.cend(), '|') > 0)
    {
      temp = "(" + temp + ")";
    }
  }
  else
  {
    temp = "none";
  }
  return s << "llfio::process_handle::flag::" << temp;
}

//! \brief Constructor for `process_handle`
template <> struct construct<process_handle>
{
  path_view _path;
  span<path_view_component> _args;
  span<path_view_component> _env = *process_handle::current().environment();
  process_handle::flag _flags = process_handle::flag::wait_on_close;
  result<process_handle> operator()() const noexcept { return process_handle::launch_process(_path, _args, _env, _flags); }
};

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/process_handle.ipp"
#else
#include "detail/impl/posix/process_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
