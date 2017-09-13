/* A handle to something
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#ifndef AFIO_IO_HANDLE_H
#define AFIO_IO_HANDLE_H

#include "handle.hpp"

//! \file io_handle.hpp Provides i/o handle

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

AFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class io_handle
\brief A handle to something capable of scatter-gather i/o.
*/
class AFIO_DECL io_handle : public handle
{
public:
  using path_type = handle::path_type;
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;
  using mode = handle::mode;
  using creation = handle::creation;
  using caching = handle::caching;
  using flag = handle::flag;

  //! The scatter buffer type used by this handle. Guaranteed to be `TrivialType` and `StandardLayoutType`.
  struct buffer_type
  {
    //! Pointer to memory to be filled by a read. Try to make this 64 byte, or ideally, `page_size()` aligned where possible.
    char *data;
    //! The number of bytes to fill into this address. Try to make this a 64 byte multiple, or ideally, a whole multiple of `page_size()`.
    size_t len;
  };
  //! The gather buffer type used by this handle. Guaranteed to be `TrivialType` and `StandardLayoutType`.
  struct const_buffer_type
  {
    //! Pointer to memory to be written. Try to make this 64 byte, or ideally, `page_size()` aligned where possible.
    const char *data;
    //! The number of bytes to write from this address. Try to make this a 64 byte multiple, or ideally, a whole multiple of `page_size()`.
    size_t len;
  };
#ifndef NDEBUG
  static_assert(std::is_trivial<buffer_type>::value, "buffer_type is not a trivial type!");
  static_assert(std::is_trivial<const_buffer_type>::value, "const_buffer_type is not a trivial type!");
  static_assert(std::is_standard_layout<buffer_type>::value, "buffer_type is not a standard layout type!");
  static_assert(std::is_standard_layout<const_buffer_type>::value, "const_buffer_type is not a standard layout type!");
#endif
  //! The scatter buffers type used by this handle. Guaranteed to be `TrivialType` apart from construction, and `StandardLayoutType`.
  using buffers_type = span<buffer_type>;
  //! The gather buffers type used by this handle. Guaranteed to be `TrivialType` apart from construction, and `StandardLayoutType`.
  using const_buffers_type = span<const_buffer_type>;
#ifndef NDEBUG
  // Is trivial in all ways, except default constructibility
  static_assert(std::is_trivially_copyable<buffers_type>::value, "buffers_type is not trivially copyable!");
  static_assert(std::is_trivially_assignable<buffers_type, buffers_type>::value, "buffers_type is not trivially assignable!");
  static_assert(std::is_trivially_destructible<buffers_type>::value, "buffers_type is not trivially destructible!");
  static_assert(std::is_trivially_copy_constructible<buffers_type>::value, "buffers_type is not trivially copy constructible!");
  static_assert(std::is_trivially_move_constructible<buffers_type>::value, "buffers_type is not trivially move constructible!");
  static_assert(std::is_trivially_copy_assignable<buffers_type>::value, "buffers_type is not trivially copy assignable!");
  static_assert(std::is_trivially_move_assignable<buffers_type>::value, "buffers_type is not trivially move assignable!");
  static_assert(std::is_standard_layout<buffers_type>::value, "buffers_type is not a standard layout type!");
#endif
  //! The i/o request type used by this handle. Guaranteed to be `TrivialType` apart from construction, and `StandardLayoutType`.
  template <class T> struct io_request
  {
    T buffers;
    extent_type offset;
    constexpr io_request()
        : buffers()
        , offset(0)
    {
    }
    constexpr io_request(T _buffers, extent_type _offset)
        : buffers(std::move(_buffers))
        , offset(_offset)
    {
    }
  };
#ifndef NDEBUG
  // Is trivial in all ways, except default constructibility
  static_assert(std::is_trivially_copyable<io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially copyable!");
  static_assert(std::is_trivially_assignable<io_request<buffers_type>, io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially assignable!");
  static_assert(std::is_trivially_destructible<io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially destructible!");
  static_assert(std::is_trivially_copy_constructible<io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially copy constructible!");
  static_assert(std::is_trivially_move_constructible<io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially move constructible!");
  static_assert(std::is_trivially_copy_assignable<io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially copy assignable!");
  static_assert(std::is_trivially_move_assignable<io_request<buffers_type>>::value, "io_request<buffers_type> is not trivially move assignable!");
  static_assert(std::is_standard_layout<io_request<buffers_type>>::value, "io_request<buffers_type> is not a standard layout type!");
#endif
  //! The i/o result type used by this handle. Guaranteed to be `TrivialType` apart from construction..
  template <class T> struct io_result : public AFIO_V2_NAMESPACE::result<T>
  {
    using Base = AFIO_V2_NAMESPACE::result<T>;
    size_type _bytes_transferred{static_cast<size_type>(-1)};

#if defined(_MSC_VER) && !defined(__clang__)  // workaround MSVC parsing bug
    constexpr io_result()
        : Base()
    {
    }
    template <class... Args>
    constexpr io_result(Args &&... args)
        : Base(std::forward<Args>(args)...)
    {
    }
#else
    using Base::Base;
    io_result() = default;
#endif
    io_result(const io_result &) = default;
    io_result(io_result &&) = default;
    io_result &operator=(const io_result &) = default;
    io_result &operator=(io_result &&) = default;
    //! Returns bytes transferred
    size_type bytes_transferred() noexcept
    {
      if(_bytes_transferred == (size_type) -1)
      {
        _bytes_transferred = 0;
        for(auto &i : this->value())
          _bytes_transferred += i.second;
      }
      return _bytes_transferred;
    }
  };
#ifndef NDEBUG
  // Is trivial in all ways, except default constructibility
  static_assert(std::is_trivially_copyable<io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially copyable!");
  static_assert(std::is_trivially_assignable<io_result<buffers_type>, io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially assignable!");
  static_assert(std::is_trivially_destructible<io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially destructible!");
  static_assert(std::is_trivially_copy_constructible<io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially copy constructible!");
  static_assert(std::is_trivially_move_constructible<io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially move constructible!");
  static_assert(std::is_trivially_copy_assignable<io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially copy assignable!");
  static_assert(std::is_trivially_move_assignable<io_result<buffers_type>>::value, "io_result<buffers_type> is not trivially move assignable!");
//! \todo Why is io_result<buffers_type> not a standard layout type?
// static_assert(std::is_standard_layout<result<buffers_type>>::value, "result<buffers_type> is not a standard layout type!");
// static_assert(std::is_standard_layout<io_result<buffers_type>>::value, "io_result<buffers_type> is not a standard layout type!");
#endif

public:
  //! Default constructor
  constexpr io_handle() = default;
  //! Construct a handle from a supplied native handle
  constexpr io_handle(native_handle_type h, caching caching = caching::none, flag flags = flag::none)
      : handle(h, caching, flags)
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr io_handle(handle &&o) noexcept : handle(std::move(o)) {}
  //! Move construction permitted
  io_handle(io_handle &&) = default;
  //! Move assignment permitted
  io_handle &operator=(io_handle &&) = default;

  /*! \brief The *maximum* number of buffers which a single read or write syscall can process at a time
  for this specific open handle. On POSIX, this is known as `IOV_MAX`.

  Note that the actual number of buffers accepted for a read or a write may be significantly
  lower than this system-defined limit, depending on available resources. The `read()` or `write()`
  call will return the buffers accepted.

  Note also that some OSs will error out if you supply more than this limit to `read()` or `write()`,
  but other OSs do not. Some OSs guarantee that each i/o syscall has effects atomically visible or not
  to other i/o, other OSs do not.

  Microsoft Windows does not implement scatter-gather file i/o syscalls except for unbuffered i/o.
  Thus this function will always return `1` in that situation.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC size_t max_buffers() const noexcept;

  /*! \brief Read data from the open handle.

  \warning Depending on the implementation backend, **very** different buffers may be returned than you
  supplied. You should **always** use the buffers returned and assume that they point to different
  memory and that each buffer's size will have changed.

  \return The buffers read, which may not be the buffers input. The size of each scatter-gather
  buffer is updated with the number of bytes of that buffer transferred, and the pointer to
  the data may be \em completely different to what was submitted (e.g. it may point into a
  memory map).
  \param reqs A scatter-gather and offset request.
  \param d An optional deadline by which the i/o must complete, else it is cancelled.
  Note function may return significantly after this deadline if the i/o takes long to cancel.
  \errors Any of the values POSIX read() can return, `errc::timed_out`, `errc::operation_canceled`. `errc::not_supported` may be
  returned if deadline i/o is not possible with this particular handle configuration (e.g.
  reading from regular files on POSIX or reading from a non-overlapped HANDLE on Windows).
  \mallocs The default synchronous implementation in file_handle performs no memory allocation.
  The asynchronous implementation in async_file_handle performs one calloc and one free.
  */
  AFIO_MAKE_FREE_FUNCTION
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept;
  //! \overload
  io_result<buffer_type> read(extent_type offset, char *data, size_type bytes, deadline d = deadline()) noexcept
  {
    buffer_type _reqs[1] = {{data, bytes}};
    io_request<buffers_type> reqs(buffers_type(_reqs), offset);
    OUTCOME_TRY(v, read(reqs, d));
    return *v.data();
  }

  /*! \brief Write data to the open handle.

  \warning Depending on the implementation backend, not all of the buffers input may be written and
  the some buffers at the end of the returned buffers may return with zero bytes written.
  For example, with a zeroed deadline, some backends may only consume as many buffers as the system has available write slots
  for, thus for those backends this call is "non-blocking" in the sense that it will return immediately even if it
  could not schedule a single buffer write. Another example is that some implementations will not
  auto-extend the length of a file when a write exceeds the maximum extent, you will need to issue
  a `truncate(newsize)` first.

  \return The buffers written, which may not be the buffers input. The size of each scatter-gather
  buffer is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d An optional deadline by which the i/o must complete, else it is cancelled.
  Note function may return significantly after this deadline if the i/o takes long to cancel.
  \errors Any of the values POSIX write() can return, `errc::timed_out`, `errc::operation_canceled`. `errc::not_supported` may be
  returned if deadline i/o is not possible with this particular handle configuration (e.g.
  writing to regular files on POSIX or writing to a non-overlapped HANDLE on Windows).
  \mallocs The default synchronous implementation in file_handle performs no memory allocation.
  The asynchronous implementation in async_file_handle performs one calloc and one free.
  */
  AFIO_MAKE_FREE_FUNCTION
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept;
  //! \overload
  io_result<const_buffer_type> write(extent_type offset, const char *data, size_type bytes, deadline d = deadline()) noexcept
  {
    const_buffer_type _reqs[1] = {{data, bytes}};
    io_request<const_buffers_type> reqs(const_buffers_type(_reqs), offset);
    OUTCOME_TRY(v, write(reqs, d));
    return *v.data();
  }

  /*! \brief Issue a write reordering barrier such that writes preceding the barrier will reach storage
  before writes after this barrier.

  \warning **Assume that this call is a no-op**. It is not reliably implemented in many common use cases,
  for example if your code is running inside a LXC container, or if the user has mounted the filing
  system with non-default options. Instead open the handle with `caching::reads` which means that all
  writes form a strict sequential order not completing until acknowledged by the storage device.
  Filing system can and do use different algorithms to give much better performance with `caching::reads`,
  some (e.g. ZFS) spectacularly better.

  \warning Let me repeat again: consider this call to be a **hint** to poke the kernel with a stick to
  go start to do some work sooner rather than later. **It may be ignored entirely**.

  \warning For portability, you can only assume that barriers write order for a single handle
  instance. You cannot assume that barriers write order across multiple handles to the same inode, or
  across processes.

  \return The buffers barriered, which may not be the buffers input. The size of each scatter-gather
  buffer is updated with the number of bytes of that buffer barriered.
  \param reqs A scatter-gather and offset request for what range to barrier. May be ignored on some platforms
  which always write barrier the entire file. Supplying a default initialised reqs write barriers the entire file.
  \param wait_for_device True if you want the call to wait until data reaches storage and that storage
  has acknowledged the data is physically written. Slow.
  \param and_metadata True if you want the call to sync the metadata for retrieving the writes before the
  barrier after a sudden power loss event. Slow.
  \param d An optional deadline by which the i/o must complete, else it is cancelled.
  Note function may return significantly after this deadline if the i/o takes long to cancel.
  \errors Any of the values POSIX fdatasync() or Windows NtFlushBuffersFileEx() can return.
  \mallocs None.
  */
  AFIO_MAKE_FREE_FUNCTION
  virtual io_result<const_buffers_type> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false, deadline d = deadline()) noexcept = 0;

  /*! \class extent_guard
  \brief RAII holder a locked extent of bytes in a file.
  */
  class extent_guard
  {
    friend class io_handle;
    io_handle *_h;
    extent_type _offset, _length;
    bool _exclusive;
    constexpr extent_guard(io_handle *h, extent_type offset, extent_type length, bool exclusive)
        : _h(h)
        , _offset(offset)
        , _length(length)
        , _exclusive(exclusive)
    {
    }
    extent_guard(const extent_guard &) = delete;
    extent_guard &operator=(const extent_guard &) = delete;

  public:
    //! Default constructor
    constexpr extent_guard()
        : _h(nullptr)
        , _offset(0)
        , _length(0)
        , _exclusive(false)
    {
    }
    //! Move constructor
    extent_guard(extent_guard &&o) noexcept : _h(o._h), _offset(o._offset), _length(o._length), _exclusive(o._exclusive) { o.release(); }
    //! Move assign
    extent_guard &operator=(extent_guard &&o) noexcept
    {
      unlock();
      _h = o._h;
      _offset = o._offset;
      _length = o._length;
      _exclusive = o._exclusive;
      o.release();
      return *this;
    }
    ~extent_guard()
    {
      if(_h)
        unlock();
    }
    //! True if extent guard is valid
    explicit operator bool() const noexcept { return _h != nullptr; }
    //! True if extent guard is invalid
    bool operator!() const noexcept { return _h == nullptr; }

    //! The io_handle to be unlocked
    io_handle *handle() const noexcept { return _h; }
    //! Sets the io_handle to be unlocked
    void set_handle(io_handle *h) noexcept { _h = h; }
    //! The extent to be unlocked
    std::tuple<extent_type, extent_type, bool> extent() const noexcept { return std::make_tuple(_offset, _length, _exclusive); }

    //! Unlocks the locked extent immediately
    void unlock() noexcept
    {
      if(_h)
      {
        _h->unlock(_offset, _length);
        release();
      }
    }

    //! Detach this RAII unlocker from the locked state
    void release() noexcept
    {
      _h = nullptr;
      _offset = 0;
      _length = 0;
      _exclusive = false;
    }
  };

  /*! \brief Tries to lock the range of bytes specified for shared or exclusive access. Be aware this passes through
  the same semantics as the underlying OS call, including any POSIX insanity present on your platform:

  - Any fd closed on an inode must release all byte range locks on that inode for all
  other fds. If your OS isn't new enough to support the non-insane lock API, `flag::byte_lock_insanity` will be set
  in flags() after the first call to this function.
  - Threads replace each other's locks, indeed locks replace each other's locks.

  You almost cetainly should use your choice of an `algorithm::shared_fs_mutex::*` instead of this
  as those are more portable and performant.

  \warning This is a low-level API which you should not use directly in portable code. Another issue is that
  atomic lock upgrade/downgrade, if your platform implements that (you should assume it does not in
  portable code), means that on POSIX you need to *release* the old `extent_guard` after creating a new one over the
  same byte range, otherwise the old `extent_guard`'s destructor will simply unlock the range entirely. On
  Windows however upgrade/downgrade locks overlay, so on that platform you must *not* release the old
  `extent_guard`. Look into `algorithm::shared_fs_mutex::safe_byte_ranges` for a portable solution.

  \return An extent guard, the destruction of which will call unlock().
  \param offset The offset to lock. Note that on POSIX the top bit is always cleared before use
  as POSIX uses signed transport for offsets. If you want an advisory rather than mandatory lock
  on Windows, one technique is to force top bit set so the region you lock is not the one you will
  i/o - obviously this reduces maximum file size to (2^63)-1.
  \param bytes The number of bytes to lock. Zero means lock the entire file using any more
  efficient alternative algorithm where available on your platform (specifically, on BSD and OS X use
  flock() for non-insane semantics).
  \param exclusive Whether the lock is to be exclusive.
  \param d An optional deadline by which the lock must complete, else it is cancelled.
  \errors Any of the values POSIX fcntl() can return, `errc::timed_out`, `errc::not_supported` may be
  returned if deadline i/o is not possible with this particular handle configuration (e.g.
  non-overlapped HANDLE on Windows).
  \mallocs The default synchronous implementation in file_handle performs no memory allocation.
  The asynchronous implementation in async_file_handle performs one calloc and one free.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_guard> lock(extent_type offset, extent_type bytes, bool exclusive = true, deadline d = deadline()) noexcept;
  //! \overload
  result<extent_guard> try_lock(extent_type offset, extent_type bytes, bool exclusive = true) noexcept { return lock(offset, bytes, exclusive, deadline(std::chrono::seconds(0))); }
  //! \overload Locks for shared access
  result<extent_guard> lock(io_request<buffers_type> reqs, deadline d = deadline()) noexcept
  {
    size_t bytes = 0;
    for(auto &i : reqs.buffers)
    {
      if(bytes + i.len < bytes)
        return std::errc::value_too_large;
      bytes += i.len;
    }
    return lock(reqs.offset, bytes, false, std::move(d));
  }
  //! \overload Locks for exclusive access
  result<extent_guard> lock(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept
  {
    size_t bytes = 0;
    for(auto &i : reqs.buffers)
    {
      if(bytes + i.len < bytes)
        return std::errc::value_too_large;
      bytes += i.len;
    }
    return lock(reqs.offset, bytes, true, std::move(d));
  }

  /*! \brief Unlocks a byte range previously locked.

  \param offset The offset to unlock. This should be an offset previously locked.
  \param bytes The number of bytes to unlock. This should be a byte extent previously locked.
  \errors Any of the values POSIX fcntl() can return.
  \mallocs None.
  */
  AFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock(extent_type offset, extent_type bytes) noexcept;
};


// BEGIN make_free_functions.py
/*! \brief Read data from the open handle.

\warning Depending on the implementation backend, **very** different buffers may be returned than you
supplied. You should **always** use the buffers returned and assume that they point to different
memory and that each buffer's size will have changed.

\return The buffers read, which may not be the buffers input. The size of each scatter-gather
buffer is updated with the number of bytes of that buffer transferred, and the pointer to
the data may be \em completely different to what was submitted (e.g. it may point into a
memory map).
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\param d An optional deadline by which the i/o must complete, else it is cancelled.
Note function may return significantly after this deadline if the i/o takes long to cancel.
\errors Any of the values POSIX read() can return, `errc::timed_out`, `errc::operation_canceled`. `errc::not_supported` may be
returned if deadline i/o is not possible with this particular handle configuration (e.g.
reading from regular files on POSIX or reading from a non-overlapped HANDLE on Windows).
\mallocs The default synchronous implementation in file_handle performs no memory allocation.
The asynchronous implementation in async_file_handle performs one calloc and one free.
*/
inline io_handle::io_result<io_handle::buffers_type> read(io_handle &self, io_handle::io_request<io_handle::buffers_type> reqs, deadline d = deadline()) noexcept
{
  return self.read(std::forward<decltype(reqs)>(reqs), std::forward<decltype(d)>(d));
}
/*! \brief Write data to the open handle.

\warning Depending on the implementation backend, not all of the buffers input may be written and
the some buffers at the end of the returned buffers may return with zero bytes written.
For example, with a zeroed deadline, some backends may only consume as many buffers as the system has available write slots
for, thus for those backends this call is "non-blocking" in the sense that it will return immediately even if it
could not schedule a single buffer write. Another example is that some implementations will not
auto-extend the length of a file when a write exceeds the maximum extent, you will need to issue
a `truncate(newsize)` first.

\return The buffers written, which may not be the buffers input. The size of each scatter-gather
buffer is updated with the number of bytes of that buffer transferred.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request.
\param d An optional deadline by which the i/o must complete, else it is cancelled.
Note function may return significantly after this deadline if the i/o takes long to cancel.
\errors Any of the values POSIX write() can return, `errc::timed_out`, `errc::operation_canceled`. `errc::not_supported` may be
returned if deadline i/o is not possible with this particular handle configuration (e.g.
writing to regular files on POSIX or writing to a non-overlapped HANDLE on Windows).
\mallocs The default synchronous implementation in file_handle performs no memory allocation.
The asynchronous implementation in async_file_handle performs one calloc and one free.
*/
inline io_handle::io_result<io_handle::const_buffers_type> write(io_handle &self, io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d = deadline()) noexcept
{
  return self.write(std::forward<decltype(reqs)>(reqs), std::forward<decltype(d)>(d));
}
/*! \brief Issue a write reordering barrier such that writes preceding the barrier will reach storage
before writes after this barrier.

\warning **Assume that this call is a no-op**. It is not reliably implemented in many common use cases,
for example if your code is running inside a LXC container, or if the user has mounted the filing
system with non-default options. Instead open the handle with `caching::reads` which means that all
writes form a strict sequential order not completing until acknowledged by the storage device.
Filing system can and do use different algorithms to give much better performance with `caching::reads`,
some (e.g. ZFS) spectacularly better.

\warning Let me repeat again: consider this call to be a **hint** to poke the kernel with a stick to
go start to do some work sooner rather than later. **It may be ignored entirely**.

\warning For portability, you can only assume that barriers write order for a single handle
instance. You cannot assume that barriers write order across multiple handles to the same inode, or
across processes.

\return The buffers barriered, which may not be the buffers input. The size of each scatter-gather
buffer is updated with the number of bytes of that buffer barriered.
\param self The object whose member function to call.
\param reqs A scatter-gather and offset request for what range to barrier. May be ignored on some platforms
which always write barrier the entire file. Supplying a default initialised reqs write barriers the entire file.
\param wait_for_device True if you want the call to wait until data reaches storage and that storage
has acknowledged the data is physically written. Slow.
\param and_metadata True if you want the call to sync the metadata for retrieving the writes before the
barrier after a sudden power loss event. Slow.
\param d An optional deadline by which the i/o must complete, else it is cancelled.
Note function may return significantly after this deadline if the i/o takes long to cancel.
\errors Any of the values POSIX fdatasync() or Windows NtFlushBuffersFileEx() can return.
\mallocs None.
*/
inline io_handle::io_result<io_handle::const_buffers_type> barrier(io_handle &self, io_handle::io_request<io_handle::const_buffers_type> reqs = io_handle::io_request<io_handle::const_buffers_type>(), bool wait_for_device = false, bool and_metadata = false, deadline d = deadline()) noexcept
{
  return self.barrier(std::forward<decltype(reqs)>(reqs), std::forward<decltype(wait_for_device)>(wait_for_device), std::forward<decltype(and_metadata)>(and_metadata), std::forward<decltype(d)>(d));
}
// END make_free_functions.py

AFIO_V2_NAMESPACE_END

#if AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/io_handle.ipp"
#else
#include "detail/impl/posix/io_handle.ipp"
#endif
#undef AFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
