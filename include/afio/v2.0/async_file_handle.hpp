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

/*! \class async_file_handle
\brief An asynchronous handle to an open something

\todo Direct use of `calloc()` ought to be replaced with a user supplied STL allocator instance.
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
  async_file_handle() = default;

  //! Construct a handle from a supplied native handle
  constexpr async_file_handle(io_service *service, native_handle_type h, dev_t devid, ino_t inode, caching caching = caching::none, flag flags = flag::none)
      : file_handle(std::move(h), devid, inode, std::move(caching), std::move(flags))
  {
    this->_service = service;
  }
  //! Implicit move construction of async_file_handle permitted
  async_file_handle(async_file_handle &&o) noexcept = default;
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
  //! Swap with another instance
  void swap(async_file_handle &o) noexcept
  {
    async_file_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create an async file handle opening access to a file on path
  using the given io_service.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<async_file_handle> async_file(io_service &service, const path_handle &base, path_view_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept
  {
    // Open it overlapped, otherwise no difference.
    OUTCOME_TRY(v, file_handle::file(std::move(base), std::move(_path), std::move(_mode), std::move(_creation), std::move(_caching), flags | flag::overlapped));
    async_file_handle ret(std::move(v));
    ret._service = &service;
    return std::move(ret);
  }

  /*! Create an async file handle creating a randomly named file on a path.
  The file is opened exclusively with `creation::only_if_not_exist` so it
  will never collide with nor overwrite any existing file. Note also
  that caching defaults to temporary which hints to the OS to only
  flush changes to physical storage as lately as possible.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static inline result<async_file_handle> async_random_file(io_service &service, const path_handle &dirpath, mode _mode = mode::write, caching _caching = caching::temporary, flag flags = flag::none) noexcept
  {
    try
    {
      for(;;)
      {
        auto randomname = utils::random_string(32);
        randomname.append(".random");
        result<async_file_handle> ret = async_file(service, dirpath, randomname, _mode, creation::only_if_not_exist, _caching, flags);
        if(ret || (!ret && ret.error() != std::errc::file_exists))
          return ret;
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
  `async_random_file(temporary_files_directory())` and the creation
  parameter is ignored.

  \note If the temporary file you are creating is not going to have its
  path sent to another process for usage, this is the WRONG function
  to use. Use `temp_inode()` instead, it is far more secure.

  \errors Any of the values POSIX open() or CreateFile() can return.
  */
  AFIO_MAKE_FREE_FUNCTION
  static inline result<async_file_handle> async_temp_file(io_service &service, path_view_type name = path_view_type(), mode _mode = mode::write, creation _creation = creation::if_needed, caching _caching = caching::temporary, flag flags = flag::unlink_on_close) noexcept
  {
    OUTCOME_TRY(tempdirh, path_handle::path(temporary_files_directory()));
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
  static AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<async_file_handle> async_temp_inode(io_service &service, path_view_type dirpath = temporary_files_directory(), mode _mode = mode::write, flag flags = flag::none) noexcept
  {
    // Open it overlapped, otherwise no difference.
    OUTCOME_TRY(v, file_handle::temp_inode(std::move(dirpath), std::move(_mode), flags | flag::overlapped));
    async_file_handle ret(std::move(v));
    ret._service = &service;
    return std::move(ret);
  }

  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false, deadline d = deadline()) noexcept override;
  /*! Clone this handle to a different io_service (copy constructor is disabled to avoid accidental copying)

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<async_file_handle> clone(io_service &service) const noexcept;
  using file_handle::clone;

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
    fsync,
    dsync
  };
  // Holds state for an i/o in progress. Will be subclassed with platform specific state and how to implement completion.
  // Note this is allocated using malloc not new to avoid memory zeroing, and therefore it has a custom deleter.
  struct _erased_io_state_type
  {
    async_file_handle *parent;
    operation_t operation;
    size_t items;
    shared_size_type items_to_go;
    constexpr _erased_io_state_type(async_file_handle *_parent, operation_t _operation, size_t _items)
        : parent(_parent)
        , operation(_operation)
        , items(_items)
        , items_to_go(0)
    {
    }
    /*
    For Windows:
      - errcode: GetLastError() code
      - bytes_transferred: obvious
      - internal_state: LPOVERLAPPED for this op

    For POSIX AIO:
      - errcode: errno code
      - bytes_transferred: return from aio_return(), usually bytes transferred
      - internal_state: address of pointer to struct aiocb in io_service's _aiocbsv
    */
    virtual void operator()(long errcode, long bytes_transferred, void *internal_state) noexcept = 0;
    AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~_erased_io_state_type()
    {
      // i/o still pending is very bad, this should never happen
      assert(!items_to_go);
      if(items_to_go)
      {
        AFIO_LOG_FATAL(parent->native_handle().h, "FATAL: io_state destructed while i/o still in flight, the derived class should never allow this.");
        abort();
      }
    }
  };
  // State for an i/o in progress, but with the per operation typing
  template <class CompletionRoutine, class BuffersType> struct _io_state_type : public _erased_io_state_type
  {
    io_result<BuffersType> result;
    CompletionRoutine completion;
    constexpr _io_state_type(async_file_handle *_parent, operation_t _operation, CompletionRoutine &&f, size_t _items)
        : _erased_io_state_type(_parent, _operation, _items)
        , result(BuffersType())
        , completion(std::forward<CompletionRoutine>(f))
    {
    }
  };
  struct _io_state_deleter
  {
    template <class U> void operator()(U *_ptr) const
    {
      _ptr->~U();
      char *ptr = (char *) _ptr;
      ::free(ptr);
    }
  };

public:
  /*! Smart pointer to state of an i/o in progress. Destroying this before an i/o has completed
  is <b>blocking</b> because the i/o must be cancelled before the destructor can safely exit.
  */
  using erased_io_state_ptr = std::unique_ptr<_erased_io_state_type, _io_state_deleter>;
  /*! Smart pointer to state of an i/o in progress. Destroying this before an i/o has completed
  is <b>blocking</b> because the i/o must be cancelled before the destructor can safely exit.
  */
  template <class CompletionRoutine, class BuffersType> using io_state_ptr = std::unique_ptr<_io_state_type<CompletionRoutine, BuffersType>, _io_state_deleter>;

#if DOXYGEN_SHOULD_SKIP_THIS
private:
#else
protected:
#endif
  template <class CompletionRoutine, class BuffersType, class IORoutine> result<io_state_ptr<CompletionRoutine, BuffersType>> _begin_io(operation_t operation, io_request<BuffersType> reqs, CompletionRoutine &&completion, IORoutine &&ioroutine) noexcept;

public:
  /*! \brief Schedule a read to occur asynchronously.

  \return Either an io_state_ptr to the i/o in progress, or an error code.
  \param reqs A scatter-gather and offset request.
  \param completion A callable to call upon i/o completion. Spec is void(async_file_handle *, io_result<buffers_type> &).
  Note that buffers returned may not be buffers input, see documentation for read().
  \errors As for read(), plus ENOMEM.
  \mallocs One calloc, one free. The allocation is unavoidable due to the need to store a type
  erased completion handler of unknown type.
  */
  AFIO_MAKE_FREE_FUNCTION
  template <class CompletionRoutine> result<io_state_ptr<CompletionRoutine, buffers_type>> async_read(io_request<buffers_type> reqs, CompletionRoutine &&completion) noexcept;

  /*! \brief Schedule a write to occur asynchronously.

  \return Either an io_state_ptr to the i/o in progress, or an error code.
  \param reqs A scatter-gather and offset request.
  \param completion A callable to call upon i/o completion. Spec is void(async_file_handle *, io_result<const_buffers_type> &).
  Note that buffers returned may not be buffers input, see documentation for write().
  \errors As for write(), plus ENOMEM.
  \mallocs One calloc, one free. The allocation is unavoidable due to the need to store a type
  erased completion handler of unknown type.
  */
  AFIO_MAKE_FREE_FUNCTION
  template <class CompletionRoutine> result<io_state_ptr<CompletionRoutine, const_buffers_type>> async_write(io_request<const_buffers_type> reqs, CompletionRoutine &&completion) noexcept;

  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override;
};

// BEGIN make_free_functions.py
/*! Create an async file handle opening access to a file on path
using the given io_service.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_file(io_service &service, const path_handle &base, async_file_handle::path_view_type _path, async_file_handle::mode _mode = async_file_handle::mode::read, async_file_handle::creation _creation = async_file_handle::creation::open_existing,
                                            async_file_handle::caching _caching = async_file_handle::caching::all, async_file_handle::flag flags = async_file_handle::flag::none) noexcept
{
  return async_file_handle::async_file(std::forward<decltype(service)>(service), std::forward<decltype(base)>(base), std::forward<decltype(_path)>(_path), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_creation)>(_creation), std::forward<decltype(_caching)>(_caching),
                                       std::forward<decltype(flags)>(flags));
}
/*! Create an async file handle creating a randomly named file on a path.
The file is opened exclusively with `creation::only_if_not_exist` so it
will never collide with nor overwrite any existing file. Note also
that caching defaults to temporary which hints to the OS to only
flush changes to physical storage as lately as possible.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_random_file(io_service &service, const path_handle &dirpath, async_file_handle::mode _mode = async_file_handle::mode::write, async_file_handle::caching _caching = async_file_handle::caching::temporary, async_file_handle::flag flags = async_file_handle::flag::none) noexcept
{
  return async_file_handle::async_random_file(std::forward<decltype(service)>(service), std::forward<decltype(dirpath)>(dirpath), std::forward<decltype(_mode)>(_mode), std::forward<decltype(_caching)>(_caching), std::forward<decltype(flags)>(flags));
}
/*! Create an async file handle creating the named file on some path which
the OS declares to be suitable for temporary files. Most OSs are
very lazy about flushing changes made to these temporary files.
Note the default flags are to have the newly created file deleted
on first handle close.
Note also that an empty name is equivalent to calling
`async_random_file(temporary_files_directory())` and the creation
parameter is ignored.

\note If the temporary file you are creating is not going to have its
path sent to another process for usage, this is the WRONG function
to use. Use `temp_inode()` instead, it is far more secure.

\errors Any of the values POSIX open() or CreateFile() can return.
*/
inline result<async_file_handle> async_temp_file(io_service &service, async_file_handle::path_view_type name = async_file_handle::path_view_type(), async_file_handle::mode _mode = async_file_handle::mode::write, async_file_handle::creation _creation = async_file_handle::creation::if_needed,
                                                 async_file_handle::caching _caching = async_file_handle::caching::temporary, async_file_handle::flag flags = async_file_handle::flag::unlink_on_close) noexcept
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
inline result<async_file_handle> async_temp_inode(io_service &service, async_file_handle::path_view_type dirpath = temporary_files_directory(), async_file_handle::mode _mode = async_file_handle::mode::write, async_file_handle::flag flags = async_file_handle::flag::none) noexcept
{
  return async_file_handle::async_temp_inode(std::forward<decltype(service)>(service), std::forward<decltype(dirpath)>(dirpath), std::forward<decltype(_mode)>(_mode), std::forward<decltype(flags)>(flags));
}
/*! \brief Schedule a read to occur asynchronously.

\return Either an io_state_ptr to the i/o in progress, or an error code.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\param completion A callable to call upon i/o completion. Spec is void(async_file_handle *, io_result<buffers_type> &).
Note that buffers returned may not be buffers input, see documentation for read().
\errors As for read(), plus ENOMEM.
\mallocs One calloc, one free. The allocation is unavoidable due to the need to store a type
erased completion handler of unknown type.
*/
template <class CompletionRoutine> inline result<async_file_handle::io_state_ptr<CompletionRoutine, async_file_handle::buffers_type>> async_read(async_file_handle &self, async_file_handle::io_request<async_file_handle::buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return self.async_read(std::forward<decltype(reqs)>(reqs), std::forward<decltype(completion)>(completion));
}
/*! \brief Schedule a write to occur asynchronously.

\return Either an io_state_ptr to the i/o in progress, or an error code.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\param completion A callable to call upon i/o completion. Spec is void(async_file_handle *, io_result<const_buffers_type> &).
Note that buffers returned may not be buffers input, see documentation for write().
\errors As for write(), plus ENOMEM.
\mallocs One calloc, one free. The allocation is unavoidable due to the need to store a type
erased completion handler of unknown type.
*/
template <class CompletionRoutine> inline result<async_file_handle::io_state_ptr<CompletionRoutine, async_file_handle::const_buffers_type>> async_write(async_file_handle &self, async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return self.async_write(std::forward<decltype(reqs)>(reqs), std::forward<decltype(completion)>(completion));
}
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
