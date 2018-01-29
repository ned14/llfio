/* An async handle to a file
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: Dec 2015


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

#include "file_handle.hpp"
#include "io_service.hpp"

//! \file async_file_handle.hpp Provides async_file_handle

#ifndef AFIO_ASYNC_FILE_HANDLE_H
#define AFIO_ASYNC_FILE_HANDLE_H

AFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace detail
{
#if __cplusplus > 201700
  template <class R, class Fn, class... Args> using is_invocable_r = std::is_invocable_r<R, Fn, Args...>;
#else
  template <class R, class Fn, class... Args> using is_invocable_r = std::true_type;
#endif
}

/*! \class async_file_handle
\brief An asynchronous handle to an open something.

\note Unlike the others, `async_file_handle` defaults to `only_metadata` caching as that is the
only use case where using async i/o makes sense given the other options below.

<table>
<tr><th></th><th>Cost of opening</th><th>Cost of i/o</th><th>Concurrency and Atomicity</th><th>Other remarks</th></tr>
<tr><td>`file_handle`</td><td>Least</td><td>Syscall</td><td>POSIX guarantees (usually)</td><td>Least gotcha</td></tr>
<tr><td>`async_file_handle`</td><td>More</td><td>Most (syscall + malloc/free + reactor)</td><td>POSIX guarantees (usually)</td><td>Makes no sense to use with cached i/o as it's a very expensive way to call `memcpy()`</td></tr>
<tr><td>`mapped_file_handle`</td><td>Most</td><td>Least</td><td>None</td><td>Cannot be used with uncached i/o</td></tr>
</table>

\warning i/o initiated by this class MUST be on the same kernel thread as which
created the owning `io_service` which MUST also be the same kernel thread as which
runs the i/o service's `run()` function.

\snippet coroutines.cpp coroutines_example
*/
class AFIO_DECL async_file_handle : public file_handle
{
  friend class io_service;

public:
  using dev_t = file_handle::dev_t;
  using ino_t = file_handle::ino_t;
  using path_view_type = file_handle::path_view_type;
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

protected:
  // Do NOT declare variables here, put them into file_handle to preserve up-conversion

public:
  //! Default constructor
  constexpr async_file_handle() {}  // NOLINT
  ~async_file_handle() = default;

  //! Construct a handle from a supplied native handle
  constexpr async_file_handle(io_service *service, native_handle_type h, dev_t devid, ino_t inode, caching caching = caching::none, flag flags = flag::none)
      : file_handle(std::move(h), devid, inode, caching, flags)
  {
    this->_service = service;
  }
  //! Implicit move construction of async_file_handle permitted
  async_file_handle(async_file_handle &&o) noexcept = default;
  //! No copy construction (use `clone()`)
  async_file_handle(const async_file_handle &) = delete;
  //! Explicit conversion from file_handle permitted
  explicit constexpr async_file_handle(file_handle &&o) noexcept : file_handle(std::move(o)) {}
  //! Explicit conversion from handle and io_handle permitted
  explicit constexpr async_file_handle(handle &&o, io_service *service, dev_t devid, ino_t inode) noexcept : file_handle(std::move(o), devid, inode) { this->_service = service; }
  //! Move assignment of async_file_handle permitted
  async_file_handle &operator=(async_file_handle &&o) noexcept
  {
    this->~async_file_handle();
    new(this) async_file_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  async_file_handle &operator=(const async_file_handle &) = delete;
  //! Swap with another instance
  AFIO_MAKE_FREE_FUNCTION
  void swap(async_file_handle &o) noexcept
  {
    async_file_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create an async file handle opening access to a file on path
  using the given io_service.
  \param service The `io_service` to use.
  \param base Handle to a base location on the filing system. Pass `{}` to indicate that path will be absolute.
  \param _path The path relative to base to open.
  \param _mode How to open the file.
  \param _creation How to create the file.
  \param _caching How to ask the kernel to cache the file.
  \param flags Any additional custom behaviours.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<async_file_handle> async_file(io_service &service, const path_handle &base, path_view_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::only_metadata, flag flags = flag::none) noexcept
  {
    // Open it overlapped, otherwise no difference.
    OUTCOME_TRY(v, file_handle::file(std::move(base), _path, _mode, _creation, _caching, flags | flag::overlapped));
    async_file_handle ret(std::move(v));
    ret._service = &service;
    return std::move(ret);
  }

  /*! Create an async file handle creating a randomly named file on a path.
  The file is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing file.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static inline result<async_file_handle> async_random_file(io_service &service, const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::only_metadata, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<async_file_handle> ret = async_file(service, dirpath, randomname, _mode, creation::only_if_not_exist, _caching, flags);
        if(ret || (!ret && ret.error() != std::errc::file_exists))
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
  /*! Create an async file handle creating the named file on some path which
  the OS declares to be suitable for temporary files. Most OSs are
  very lazy about flushing changes made to these temporary files.
  Note the default flags are to have the newly created file deleted
  on first handle close.
  Note also that an empty name is equivalent to calling
  `async_random_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
  parameter is ignored.

  \note If the temporary file you are creating is not going to have its
  path sent to another process for usage, this is the WRONG function
  to use. Use `temp_inode()` instead, it is far more secure.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static inline result<async_file_handle> async_temp_file(io_service &service, path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::only_metadata, flag flags = flag::unlink_on_close) noexcept
  {
    auto &tempdirh = path_discovery::storage_backed_temporary_files_directory();
    return name.empty() ? async_random_file(service, tempdirh, _mode, _caching, flags) : async_file(service, tempdirh, name, _mode, _creation, _caching, flags);
  }
  /*! \em Securely create an async file handle creating a temporary anonymous inode in
  the filesystem referred to by \em dirpath. The inode created has
  no name nor accessible path on the filing system and ceases to
  exist as soon as the last handle is closed, making it ideal for use as
  a temporary file where other processes do not need to have access
  to its contents via some path on the filing system (a classic use case
  is for backing shared memory maps).

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<async_file_handle> async_temp_inode(io_service &service, const path_handle &dir = path_discovery::storage_backed_temporary_files_directory(), mode _mode = mode::write, flag flags = flag::none) noexcept
  {
    // Open it overlapped, otherwise no difference.
    OUTCOME_TRY(v, file_handle::temp_inode(dir, _mode, flags | flag::overlapped));
    async_file_handle ret(std::move(v));
    ret._service = &service;
    return std::move(ret);
  }

  AFIO_MAKE_FREE_FUNCTION
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false, deadline d = deadline()) noexcept override;
  /*! Clone this handle to a different io_service (copy constructor is disabled to avoid accidental copying)

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  */
  result<async_file_handle> clone(io_service &service, mode mode_ = mode::unchanged, caching caching_ = caching::unchanged, deadline d = std::chrono::seconds(30)) const noexcept
  {
    OUTCOME_TRY(v, file_handle::clone(mode_, caching_, d));
    async_file_handle ret(std::move(v));
    ret._service = &service;
    return std::move(ret);
  }
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<file_handle> clone(mode mode_ = mode::unchanged, caching caching_ = caching::unchanged, deadline d = std::chrono::seconds(30)) const noexcept override
  {
    OUTCOME_TRY(v, file_handle::clone(mode_, caching_, d));
    async_file_handle ret(std::move(v));
    ret._service = _service;
    return static_cast<file_handle &&>(ret);
  }

#if DOXYGEN_SHOULD_SKIP_THIS
private:
#else
protected:
#endif
  using shared_size_type = size_type;
  enum class operation_t
  {
    read,
    write,
    fsync_sync,
    dsync_sync,
    fsync_async,
    dsync_async
  };
  struct _erased_completion_handler;
  // Holds state for an i/o in progress. Will be subclassed with platform specific state and how to implement completion.
  struct _erased_io_state_type
  {
    friend class io_service;
    async_file_handle *parent;
    operation_t operation;
    bool must_deallocate_self;
    size_t items;
    shared_size_type items_to_go;
    union result_storage {
      io_result<buffers_type> read;
      io_result<const_buffers_type> write;
      constexpr result_storage()
          : read(buffers_type())
      {
      }
    } result;
    constexpr _erased_io_state_type(async_file_handle *_parent, operation_t _operation, bool _must_deallocate_self, size_t _items)
        : parent(_parent)
        , operation(_operation)
        , must_deallocate_self(_must_deallocate_self)
        , items(_items)
        , items_to_go(0)
    {
    }
    AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~_erased_io_state_type()
    {
      // i/o still pending is very bad, this should never happen
      assert(!items_to_go);
      if(items_to_go != 0u)
      {
        AFIO_LOG_FATAL(parent->native_handle().h, "FATAL: io_state destructed while i/o still in flight, the derived class should never allow this.");
        abort();
      }
    }

    //! Retrieves a pointer to the copy of the completion handler held inside the i/o state.
    virtual _erased_completion_handler *erased_completion_handler() noexcept = 0;
    /* Called when an i/o is completed by the system, figures out whether to call invoke_completion.

    For Windows:
      - errcode: GetLastError() code
      - bytes_transferred: obvious
      - internal_state: LPOVERLAPPED for this op

    For POSIX AIO:
      - errcode: errno code
      - bytes_transferred: return from aio_return(), usually bytes transferred
      - internal_state: address of pointer to struct aiocb in io_service's _aiocbsv
    */
    virtual void _system_io_completion(long errcode, long bytes_transferred, void *internal_state) noexcept = 0;

  protected:
    _erased_io_state_type(_erased_io_state_type &&) = default;
    _erased_io_state_type(const _erased_io_state_type &) = default;
    _erased_io_state_type &operator=(_erased_io_state_type &&) = default;
    _erased_io_state_type &operator=(const _erased_io_state_type &) = default;
  };
  struct _io_state_deleter
  {
    template <class U> void operator()(U *_ptr) const
    {
      bool must_deallocate_self = _ptr->must_deallocate_self;
      _ptr->~U();
      if(must_deallocate_self)
      {
        auto *ptr = reinterpret_cast<char *>(_ptr);
        ::free(ptr);  // NOLINT
      }
    }
  };

public:
  /*! Smart pointer to state of an i/o in progress. Destroying this before an i/o has completed
  is <b>blocking</b> because the i/o must be cancelled before the destructor can safely exit.
  */
  using io_state_ptr = std::unique_ptr<_erased_io_state_type, _io_state_deleter>;

#if DOXYGEN_SHOULD_SKIP_THIS
private:
#else
protected:
#endif
  // Used to indirect copy and call of unknown completion handler
  struct _erased_completion_handler
  {
    virtual ~_erased_completion_handler() = default;
    // Returns my size including completion handler
    virtual size_t bytes() const noexcept = 0;
    // Moves me and handler to some new location
    virtual void move(_erased_completion_handler *dest) = 0;
    // Invokes my completion handler
    virtual void operator()(_erased_io_state_type *state) = 0;
    // Returns a pointer to the completion handler
    virtual void *address() noexcept = 0;

  protected:
    _erased_completion_handler() = default;
    _erased_completion_handler(_erased_completion_handler &&) = default;
    _erased_completion_handler(const _erased_completion_handler &) = default;
    _erased_completion_handler &operator=(_erased_completion_handler &&) = default;
    _erased_completion_handler &operator=(const _erased_completion_handler &) = default;
  };
  template <class BuffersType, class IORoutine> result<io_state_ptr> AFIO_HEADERS_ONLY_MEMFUNC_SPEC _begin_io(span<char> mem, operation_t operation, io_request<BuffersType> reqs, _erased_completion_handler &&completion, IORoutine &&ioroutine) noexcept;
  AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<io_state_ptr> _begin_io(span<char> mem, operation_t operation, io_request<const_buffers_type> reqs, _erased_completion_handler &&completion) noexcept;

public:
  /*! \brief Schedule a barrier to occur asynchronously.

  \note All the caveats and exclusions which apply to `barrier()` also apply here. Note that Microsoft Windows
  does not support asynchronously executed barriers, and this call will fail on that operating system.

  \return Either an io_state_ptr to the i/o in progress, or an error code.
  \param reqs A scatter-gather and offset request for what range to barrier. May be ignored on some platforms
  which always write barrier the entire file. Supplying a default initialised reqs write barriers the entire file.
  \param completion A callable to call upon i/o completion. Spec is `void(async_file_handle *, io_result<const_buffers_type> &)`.
  Note that buffers returned may not be buffers input, see documentation for `barrier()`.
  \param wait_for_device True if you want the call to wait until data reaches storage and that storage
  has acknowledged the data is physically written. Slow.
  \param and_metadata True if you want the call to sync the metadata for retrieving the writes before the
  barrier after a sudden power loss event. Slow.
  \param mem Optional span of memory to use to avoid using `calloc()`. Note span MUST be all bits zero on entry.
  \errors As for `barrier()`, plus `ENOMEM`.
  \mallocs If mem is not set, one calloc, one free. The allocation is unavoidable due to the need to store a type
  erased completion handler of unknown type and state per buffers input.
  */
  AFIO_MAKE_FREE_FUNCTION
  template <class CompletionRoutine>                                                                                           //
  AFIO_REQUIRES(detail::is_invocable_r<void, CompletionRoutine, async_file_handle *, io_result<const_buffers_type> &>::value)  //
  result<io_state_ptr> async_barrier(io_request<const_buffers_type> reqs, CompletionRoutine &&completion, bool wait_for_device = false, bool and_metadata = false, span<char> mem = {}) noexcept
  {
    AFIO_LOG_FUNCTION_CALL(this);
    struct completion_handler : _erased_completion_handler
    {
      CompletionRoutine completion;
      explicit completion_handler(CompletionRoutine c)
          : completion(std::move(c))
      {
      }
      size_t bytes() const noexcept final { return sizeof(*this); }
      void move(_erased_completion_handler *_dest) final
      {
        auto *dest = reinterpret_cast<void *>(_dest);
        new(dest) completion_handler(std::move(*this));
      }
      void operator()(_erased_io_state_type *state) final { completion(state->parent, state->result.write); }
      void *address() noexcept final { return &completion; }
    } ch{std::forward<CompletionRoutine>(completion)};
    operation_t operation = operation_t::fsync_sync;
    if(!wait_for_device && and_metadata)
    {
      operation = operation_t::fsync_async;
    }
    else if(wait_for_device && !and_metadata)
    {
      operation = operation_t::dsync_sync;
    }
    else if(!wait_for_device && !and_metadata)
    {
      operation = operation_t::dsync_async;
    }
    return _begin_io(mem, operation, reinterpret_cast<io_request<const_buffers_type> &>(reqs), std::move(ch));
  }

  /*! \brief Schedule a read to occur asynchronously.

  \return Either an io_state_ptr to the i/o in progress, or an error code.
  \param reqs A scatter-gather and offset request.
  \param completion A callable to call upon i/o completion. Spec is `void(async_file_handle *, io_result<buffers_type> &)`.
  Note that buffers returned may not be buffers input, see documentation for `read()`.
  \param mem Optional span of memory to use to avoid using `calloc()`. Note span MUST be all bits zero on entry.
  \errors As for `read()`, plus `ENOMEM`.
  \mallocs If mem is not set, one calloc, one free. The allocation is unavoidable due to the need to store a type
  erased completion handler of unknown type and state per buffers input.
  */
  AFIO_MAKE_FREE_FUNCTION
  template <class CompletionRoutine>                                                                                     //
  AFIO_REQUIRES(detail::is_invocable_r<void, CompletionRoutine, async_file_handle *, io_result<buffers_type> &>::value)  //
  result<io_state_ptr> async_read(io_request<buffers_type> reqs, CompletionRoutine &&completion, span<char> mem = {}) noexcept
  {
    AFIO_LOG_FUNCTION_CALL(this);
    struct completion_handler : _erased_completion_handler
    {
      CompletionRoutine completion;
      explicit completion_handler(CompletionRoutine c)
          : completion(std::move(c))
      {
      }
      size_t bytes() const noexcept final { return sizeof(*this); }
      void move(_erased_completion_handler *_dest) final
      {
        auto *dest = reinterpret_cast<void *>(_dest);
        new(dest) completion_handler(std::move(*this));
      }
      void operator()(_erased_io_state_type *state) final { completion(state->parent, state->result.read); }
      void *address() noexcept final { return &completion; }
    } ch{std::forward<CompletionRoutine>(completion)};
    return _begin_io(mem, operation_t::read, io_request<const_buffers_type>({reinterpret_cast<const_buffer_type *>(reqs.buffers.data()), reqs.buffers.size()}, reqs.offset), std::move(ch));
  }

  /*! \brief Schedule a write to occur asynchronously.

  \return Either an io_state_ptr to the i/o in progress, or an error code.
  \param reqs A scatter-gather and offset request.
  \param completion A callable to call upon i/o completion. Spec is `void(async_file_handle *, io_result<const_buffers_type> &)`.
  Note that buffers returned may not be buffers input, see documentation for `write()`.
  \param mem Optional span of memory to use to avoid using `calloc()`. Note span MUST be all bits zero on entry.
  \errors As for `write()`, plus `ENOMEM`.
  \mallocs If mem in not set, one calloc, one free. The allocation is unavoidable due to the need to store a type
  erased completion handler of unknown type and state per buffers input.
  */
  AFIO_MAKE_FREE_FUNCTION
  template <class CompletionRoutine>                                                                                           //
  AFIO_REQUIRES(detail::is_invocable_r<void, CompletionRoutine, async_file_handle *, io_result<const_buffers_type> &>::value)  //
  result<io_state_ptr> async_write(io_request<const_buffers_type> reqs, CompletionRoutine &&completion, span<char> mem = {}) noexcept
  {
    AFIO_LOG_FUNCTION_CALL(this);
    struct completion_handler : _erased_completion_handler
    {
      CompletionRoutine completion;
      explicit completion_handler(CompletionRoutine c)
          : completion(std::move(c))
      {
      }
      size_t bytes() const noexcept final { return sizeof(*this); }
      void move(_erased_completion_handler *_dest) final
      {
        auto *dest = reinterpret_cast<void *>(_dest);
        new(dest) completion_handler(std::move(*this));
      }
      void operator()(_erased_io_state_type *state) final { completion(state->parent, state->result.write); }
      void *address() noexcept final { return &completion; }
    } ch{std::forward<CompletionRoutine>(completion)};
    return _begin_io(mem, operation_t::write, reqs, std::move(ch));
  }

  using file_handle::read;
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  using file_handle::write;
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override;

#if defined(__cpp_coroutines) || defined(DOXYGEN_IS_IN_THE_HOUSE)
private:
  template <class BuffersType> class awaitable_state
  {
    friend class async_file_handle;
    optional<coroutine_handle<>> _suspended;
    optional<io_result<BuffersType>> _result;

    // Called on completion of the i/o
    void operator()(async_file_handle * /*unused*/, io_result<BuffersType> &result)
    {
      // store the result and resume the coroutine
      _result = std::move(result);
      if(_suspended)
      {
        _suspended->resume();
      }
    }
  };

public:
  //! Type sugar to tell `co_await` what to do
  template <class BuffersType> class awaitable
  {
    friend class async_file_handle;
    io_state_ptr _state;
    awaitable_state<BuffersType> *_astate;

    explicit awaitable(io_state_ptr state)
        : _state(std::move(state))
        , _astate(reinterpret_cast<awaitable_state<BuffersType> *>(_state->erased_completion_handler()->address()))
    {
    }

  public:
    //! Called by `co_await` to determine whether to suspend the coroutine.
    bool await_ready() { return _astate->_result.has_value(); }
    //! Called by `co_await` to suspend the coroutine.
    void await_suspend(coroutine_handle<> co) { _astate->_suspended = co; }
    //! Called by `co_await` after resuming the coroutine to return a value.
    io_result<BuffersType> await_resume() { return std::move(*_astate->_result); }
  };

public:
  /*! \brief Schedule a read to occur asynchronously.

  \return An awaitable, which when `co_await`ed upon, suspends execution of the coroutine
  until the operation has completed, resuming with the buffers read, which may
  not be the buffers input. The size of each scatter-gather buffer is updated with the number
  of bytes of that buffer transferred, and the pointer to the data may be \em completely
  different to what was submitted (e.g. it may point into a memory map).
  \param reqs A scatter-gather and offset request.
  \errors As for read(), plus ENOMEM.
  \mallocs One calloc, one free.
  */
  AFIO_MAKE_FREE_FUNCTION
  result<awaitable<buffers_type>> co_read(io_request<buffers_type> reqs) noexcept
  {
    OUTCOME_TRY(r, async_read(reqs, awaitable_state<buffers_type>()));
    return awaitable<buffers_type>(std::move(r));
  }

  /*! \brief Schedule a write to occur asynchronously

  \return An awaitable, which when `co_await`ed upon, suspends execution of the coroutine
  until the operation has completed, resuming with the buffers written, which
  may not be the buffers input. The size of each scatter-gather buffer is updated with
  the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \errors As for write(), plus ENOMEM.
  \mallocs One calloc, one free.
  */
  AFIO_MAKE_FREE_FUNCTION
  result<awaitable<const_buffers_type>> co_write(io_request<const_buffers_type> reqs) noexcept
  {
    OUTCOME_TRY(r, async_write(reqs, awaitable_state<const_buffers_type>()));
    return awaitable<const_buffers_type>(std::move(r));
  }
#endif
};

//! \brief Constructor for `async_file_handle`
template <> struct construct<async_file_handle>
{
  io_service &service;
  const path_handle &base;
  async_file_handle::path_view_type _path;
  async_file_handle::mode _mode = async_file_handle::mode::read;
  async_file_handle::creation _creation = async_file_handle::creation::open_existing;
  async_file_handle::caching _caching = async_file_handle::caching::only_metadata;
  async_file_handle::flag flags = async_file_handle::flag::none;
  result<async_file_handle> operator()() const noexcept { return async_file_handle::async_file(service, base, _path, _mode, _creation, _caching, flags); }
};


// BEGIN make_free_functions.py
//! Swap with another instance
inline void swap(async_file_handle &self, async_file_handle &o) noexcept
{
  return self.swap(std::forward<decltype(o)>(o));
}
/*! Create an async file handle opening access to a file on path
using the given io_service.
\param service The `io_service` to use.
\param base Handle to a base location on the filing system. Pass `{}` to indicate that path will be absolute.
\param _path The path relative to base to open.
\param _mode How to open the file.
\param _creation How to create the file.
\param _caching How to ask the kernel to cache the file.
\param flags Any additional custom behaviours.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_file(io_service &service, const path_handle &base, async_file_handle::path_view_type _path, async_file_handle::mode _mode = async_file_handle::mode::read, async_file_handle::creation _creation = async_file_handle::creation::open_existing,
                                            async_file_handle::caching _caching = async_file_handle::caching::only_metadata, async_file_handle::flag flags = async_file_handle::flag::none) noexcept
{
  return async_file_handle::async_file(std::forward<decltype(service)>(service), std::forward<decltype(base)>(base), std::forward<decltype(_path)>(_path), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching),
                                       std::forward<decltype(flags)>(flags));
}
/*! Create an async file handle creating a randomly named file on a path.
The file is opened exclusively with `creation::only_if_not_exist` so it
will never collide with nor overwrite any existing file.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_random_file(io_service &service, const path_handle &dirpath, async_file_handle::mode _mode = async_file_handle::mode::write, async_file_handle::caching _caching = async_file_handle::caching::only_metadata, async_file_handle::flag flags = async_file_handle::flag::none) noexcept
{
  return async_file_handle::async_random_file(std::forward<decltype(service)>(service), std::forward<decltype(dirpath)>(dirpath), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create an async file handle creating the named file on some path which
the OS declares to be suitable for temporary files. Most OSs are
very lazy about flushing changes made to these temporary files.
Note the default flags are to have the newly created file deleted
on first handle close.
Note also that an empty name is equivalent to calling
`async_random_file(path_discovery::storage_backed_temporary_files_directory())` and the creation
parameter is ignored.

\note If the temporary file you are creating is not going to have its
path sent to another process for usage, this is the WRONG function
to use. Use `temp_inode()` instead, it is far more secure.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_temp_file(io_service &service, async_file_handle::path_view_type name = async_file_handle::path_view_type(), async_file_handle::mode _mode = async_file_handle::mode::write, async_file_handle::creation _creation = async_file_handle::creation::if_needed,
                                                 async_file_handle::caching _caching = async_file_handle::caching::only_metadata, async_file_handle::flag flags = async_file_handle::flag::unlink_on_close) noexcept
{
  return async_file_handle::async_temp_file(std::forward<decltype(service)>(service), std::forward<decltype(name)>(name), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! \em Securely create an async file handle creating a temporary anonymous inode in
the filesystem referred to by \em dirpath. The inode created has
no name nor accessible path on the filing system and ceases to
exist as soon as the last handle is closed, making it ideal for use as
a temporary file where other processes do not need to have access
to its contents via some path on the filing system (a classic use case
is for backing shared memory maps).

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_temp_inode(io_service &service, const path_handle &dir = path_discovery::storage_backed_temporary_files_directory(), async_file_handle::mode _mode = async_file_handle::mode::write, async_file_handle::flag flags = async_file_handle::flag::none) noexcept
{
  return async_file_handle::async_temp_inode(std::forward<decltype(service)>(service), std::forward<decltype(dir)>(dir), std::forward<decltype(_mode)>(_mode), std::forward<decltype(flags)>(flags));
}
inline async_file_handle::io_result<async_file_handle::const_buffers_type> barrier(async_file_handle &self, async_file_handle::io_request<async_file_handle::const_buffers_type> reqs = async_file_handle::io_request<async_file_handle::const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false,
                                                                                   deadline d = deadline()) noexcept
{
  return self.barrier(std::forward<decltype(reqs)>(reqs), std::forward<decltype(wait_for_device)>(wait_for_device), std::forward<decltype(and_metadata)>(and_metadata), std::forward<decltype(d)>(d));
}
#if defined(__cpp_coroutines) || defined(DOXYGEN_IS_IN_THE_HOUSE)
/*! \brief Schedule a read to occur asynchronously.

\return An awaitable, which when `co_await`ed upon, suspends execution of the coroutine
until the operation has completed, resuming with the buffers read, which may
not be the buffers input. The size of each scatter-gather buffer is updated with the number
of bytes of that buffer transferred, and the pointer to the data may be \em completely
different to what was submitted (e.g. it may point into a memory map).
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\errors As for read(), plus ENOMEM.
\mallocs One calloc, one free.
*/
inline result<async_file_handle::awaitable<async_file_handle::buffers_type>> co_read(async_file_handle &self, async_file_handle::io_request<async_file_handle::buffers_type> reqs) noexcept
{
  return self.co_read(std::forward<decltype(reqs)>(reqs));
}
/*! \brief Schedule a write to occur asynchronously

\return An awaitable, which when `co_await`ed upon, suspends execution of the coroutine
until the operation has completed, resuming with the buffers written, which
may not be the buffers input. The size of each scatter-gather buffer is updated with
the number of bytes of that buffer transferred.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\errors As for write(), plus ENOMEM.
\mallocs One calloc, one free.
*/
inline result<async_file_handle::awaitable<async_file_handle::const_buffers_type>> co_write(async_file_handle &self, async_file_handle::io_request<async_file_handle::const_buffers_type> reqs) noexcept
{
  return self.co_write(std::forward<decltype(reqs)>(reqs));
}
#endif
// END make_free_functions.py

AFIO_V2_NAMESPACE_END

#if AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/async_file_handle.ipp"
#else
#include "detail/impl/posix/async_file_handle.ipp"
#endif
#undef AFIO_INCLUDED_BY_HEADER
#endif

#endif
