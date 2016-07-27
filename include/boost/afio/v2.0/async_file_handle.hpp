/* async_file_handle.hpp
An async handle to a file
(C) 2015 Niall Douglas http://www.nedprod.com/
File Created: Dec 2015


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#include "file_handle.hpp"
#include "io_service.hpp"

#ifndef BOOST_AFIO_ASYNC_FILE_HANDLE_H
#define BOOST_AFIO_ASYNC_FILE_HANDLE_H

BOOST_AFIO_V2_NAMESPACE_EXPORT_BEGIN

//! A handle to an open something
class BOOST_AFIO_DECL async_file_handle : public file_handle
{
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

protected:
  io_service *_service;

public:
  //! Default constructor
  async_file_handle()
      : file_handle()
      , _service(nullptr)
  {
  }

  //! Construct a handle from a supplied native handle
  async_file_handle(io_service *service, path_type path, native_handle_type h, caching caching = caching::none, flag flags = flag::none)
      : file_handle(std::move(path), std::move(h), std::move(caching), std::move(flags))
      , _service(service)
  {
  }
  //! Implicit move construction of async_file_handle permitted
  async_file_handle(async_file_handle &&o) noexcept : file_handle(std::move(o)), _service(o._service) { o._service = nullptr; }
  //! Explicit conversion from file_handle permitted
  explicit async_file_handle(file_handle &&o) noexcept : file_handle(std::move(o)) {}
  //! Explicit conversion from handle and io_handle permitted
  explicit async_file_handle(handle &&o, io_service *service, path_type path) noexcept : file_handle(std::move(o), std::move(path)), _service(service) {}
  using file_handle::really_copy;
  //! Copy the handle. Tag enabled because copying handles is expensive (fd duplication).
  explicit async_file_handle(const async_file_handle &o, really_copy _)
      : file_handle(o, _)
  {
  }
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
  //[[bindlib::make_free]]
  static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<async_file_handle> async_file(io_service &service, path_type _path, mode _mode = mode::read, creation _creation = creation::open_existing, caching _caching = caching::all, flag flags = flag::none) noexcept;

  /*! Clone this handle to a different io_service (copy constructor is disabled to avoid accidental copying)

  \errors Any of the values POSIX dup() or DuplicateHandle() can return.
  */
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<async_file_handle> clone(io_service &service) const noexcept;
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
    write
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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~_erased_io_state_type()
    {
      // i/o still pending is very bad, this should never happen
      assert(!items_to_go);
      if(items_to_go)
      {
        BOOST_AFIO_LOG_FATAL(parent->native_handle().h, "FATAL: io_state destructed while i/o still in flight, the derived class should never allow this.");
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
        , result(make_result(BuffersType()))
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
  //[[bindlib::make_free]]
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
  //[[bindlib::make_free]]
  template <class CompletionRoutine> result<io_state_ptr<CompletionRoutine, const_buffers_type>> async_write(io_request<const_buffers_type> reqs, CompletionRoutine &&completion) noexcept;

  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override;
};

BOOST_AFIO_V2_NAMESPACE_END

#if BOOST_AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define BOOST_AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/async_file_handle.ipp"
#else
#include "detail/impl/posix/async_file_handle.ipp"
#endif
#undef BOOST_AFIO_INCLUDED_BY_HEADER
#endif

#endif