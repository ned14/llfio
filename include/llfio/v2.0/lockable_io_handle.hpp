/* A lockable i/o handle to something
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

#ifndef LLFIO_LOCKABLE_IO_HANDLE_H
#define LLFIO_LOCKABLE_IO_HANDLE_H

#include "io_handle.hpp"

#include <cassert>

//! \file lockable_io_handle.hpp Provides a lockable i/o handle

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

//! The kinds of concurrent user exclusion which can be performed.
enum class lock_kind
{
  unlocked,  //!< Exclude none.
  shared,    //!< Exclude only those requesting an exclusive lock on the same inode.
  exclusive  //!< Exclude those requesting any kind of lock on the same inode.
};

/*! \class lockable_io_handle
\brief A handle to something capable of scatter-gather i/o and which can exclude other concurrent users.
Models `SharedMutex`, though note that the locks are per-handle, not per-thread.

\note On Microsoft Windows, we lock the maximum possible byte `(2^64-1)` as the closest available
emulation of advisory whole-file locking. This causes byte range locks to work (probably) independently
of these locks.
*/
class LLFIO_DECL lockable_io_handle : public io_handle
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

public:
  //! Default constructor
  constexpr lockable_io_handle() {}  // NOLINT
  ~lockable_io_handle() = default;
  //! Construct a handle from a supplied native handle
  constexpr explicit lockable_io_handle(native_handle_type h, caching caching, flag flags, io_multiplexer *ctx)
      : io_handle(h, caching, flags, ctx)
  {
  }
  //! Explicit conversion from `handle` permitted
  explicit constexpr lockable_io_handle(handle &&o, io_multiplexer *ctx) noexcept
      : io_handle(std::move(o), ctx)
  {
  }
  //! Explicit conversion from `io_handle` permitted
  explicit constexpr lockable_io_handle(io_handle &&o) noexcept
      : io_handle(std::move(o))
  {
  }
  //! Move construction permitted
  lockable_io_handle(lockable_io_handle &&) = default;
  //! No copy construction (use `clone()`)
  lockable_io_handle(const lockable_io_handle &) = delete;
  //! Move assignment permitted
  lockable_io_handle &operator=(lockable_io_handle &&) = default;
  //! No copy assignment
  lockable_io_handle &operator=(const lockable_io_handle &) = delete;

  /*! \brief Locks the inode referred to by the open handle for exclusive access.

  Note that this may, or may not, interact with the byte range lock extensions. See `unique_file_lock`
  for a RAII locker.
  \errors Any of the values POSIX `flock()` can return.
  \mallocs The default synchronous implementation in `file_handle` performs no memory allocation.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> lock_file() noexcept;
  /*! \brief Tries to lock the inode referred to by the open handle for exclusive access,
  returning `false` if lock is currently unavailable.

  Note that this may, or may not, interact with the byte range lock extensions. See `unique_file_lock`
  for a RAII locker.
  \errors Any of the values POSIX `flock()` can return.
  \mallocs The default synchronous implementation in `file_handle` performs no memory allocation.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC bool try_lock_file() noexcept;
  /*! \brief Unlocks a previously acquired exclusive lock.
   */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock_file() noexcept;

  /*! \brief Locks the inode referred to by the open handle for shared access.

  Note that this may, or may not, interact with the byte range lock extensions. See `unique_file_lock`
  for a RAII locker.
  \errors Any of the values POSIX `flock()` can return.
  \mallocs The default synchronous implementation in `file_handle` performs no memory allocation.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> lock_file_shared() noexcept;
  /*! \brief Tries to lock the inode referred to by the open handle for shared access,
  returning `false` if lock is currently unavailable.

  Note that this may, or may not, interact with the byte range lock extensions. See `unique_file_lock`
  for a RAII locker.
  \errors Any of the values POSIX `flock()` can return.
  \mallocs The default synchronous implementation in `file_handle` performs no memory allocation.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC bool try_lock_file_shared() noexcept;
  /*! \brief Unlocks a previously acquired shared lock.
   */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock_file_shared() noexcept;

public:
  /*! \class extent_guard
  \brief EXTENSION: RAII holder a locked extent of bytes in a file.
  */
  class extent_guard
  {
    friend class lockable_io_handle;
    lockable_io_handle *_h{nullptr};
    extent_type _offset{0}, _length{0};
    lock_kind _kind{lock_kind::unlocked};

  protected:
    constexpr extent_guard(lockable_io_handle *h, extent_type offset, extent_type length, lock_kind kind)
        : _h(h)
        , _offset(offset)
        , _length(length)
        , _kind(kind)
    {
    }

  public:
    extent_guard(const extent_guard &) = delete;
    extent_guard &operator=(const extent_guard &) = delete;

    //! Default constructor
    constexpr extent_guard() {}  // NOLINT
    //! Move constructor
    extent_guard(extent_guard &&o) noexcept
        : _h(o._h)
        , _offset(o._offset)
        , _length(o._length)
        , _kind(o._kind)
    {
      o.release();
    }
    //! Move assign
    extent_guard &operator=(extent_guard &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      unlock();
      _h = o._h;
      _offset = o._offset;
      _length = o._length;
      _kind = o._kind;
      o.release();
      return *this;
    }
    ~extent_guard()
    {
      if(_h != nullptr)
      {
        unlock();
      }
    }
    //! True if extent guard is valid
    explicit operator bool() const noexcept { return _h != nullptr; }

    //! The `lockable_io_handle` to be unlocked
    lockable_io_handle *handle() const noexcept { return _h; }
    //! Sets the `lockable_io_handle` to be unlocked
    void set_handle(lockable_io_handle *h) noexcept { _h = h; }
    //! The extent to be unlocked
    std::tuple<extent_type, extent_type, lock_kind> extent() const noexcept { return std::make_tuple(_offset, _length, _kind); }

    //! Unlocks the locked extent immediately
    void unlock() noexcept
    {
      if(_h != nullptr)
      {
        _h->unlock_file_range(_offset, _length);
        release();
      }
    }

    //! Detach this RAII unlocker from the locked state
    void release() noexcept
    {
      _h = nullptr;
      _offset = 0;
      _length = 0;
      _kind = lock_kind::unlocked;
    }
  };

  /*! \brief EXTENSION: Tries to lock the range of bytes specified for shared or exclusive access.
  Note that this may, or MAY NOT, observe whole file locks placed with `lock()`, `lock_shared()` etc.
  
  Be aware this passes through the same semantics as the underlying OS call, including any POSIX
  insanity present on your platform:

  - Any fd closed on an inode must release all byte range locks on that inode for all
  other fds. If your OS isn't new enough to support the non-insane lock API,
  `flag::byte_lock_insanity` will be set in flags() after the first call to this function.
  - Threads replace each other's locks, indeed locks replace each other's locks.

  You almost cetainly should use your choice of an `algorithm::shared_fs_mutex::*` instead of this
  as those are more portable and performant, or use the `SharedMutex` modelling member functions
  which lock the whole inode for exclusive or shared access.

  \warning This is a low-level API which you should not use directly in portable code. Another
  issue is that atomic lock upgrade/downgrade, if your platform implements that (you should assume
  it does not in portable code), means that on POSIX you need to *release* the old `extent_guard`
  after creating a new one over the same byte range, otherwise the old `extent_guard`'s destructor
  will simply unlock the range entirely. On Windows however upgrade/downgrade locks overlay, so on
  that platform you must *not* release the old `extent_guard`. Look into
  `algorithm::shared_fs_mutex::safe_byte_ranges` for a portable solution.

  \return An extent guard, the destruction of which will call unlock().
  \param offset The offset to lock. Note that on POSIX the top bit is always cleared before use
  as POSIX uses signed transport for offsets. If you want an advisory rather than mandatory lock
  on Windows, one technique is to force top bit set so the region you lock is not the one you will
  i/o - obviously this reduces maximum file size to (2^63)-1.
  \param bytes The number of bytes to lock. Setting this and the offset to zero causes the whole
  file to be locked.
  \param kind Whether the lock is to be shared or exclusive.
  \param d An optional deadline by which the lock must complete, else it is cancelled.
  \errors Any of the values POSIX fcntl() can return, `errc::timed_out`, `errc::not_supported` may be
  returned if deadline i/o is not possible with this particular handle configuration (e.g.
  non-overlapped HANDLE on Windows).
  \mallocs The default synchronous implementation in file_handle performs no memory allocation.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_guard> lock_file_range(extent_type offset, extent_type bytes, lock_kind kind, deadline d = deadline()) noexcept;
  //! \overload EXTENSION: Locks for shared access
  result<extent_guard> lock_file_range(io_request<buffers_type> reqs, deadline d = deadline()) noexcept
  {
    size_t bytes = 0;
    for(auto &i : reqs.buffers)
    {
      if(bytes + i.size() < bytes)
      {
        return errc::value_too_large;
      }
      bytes += i.size();
    }
    return lock_file_range(reqs.offset, bytes, lock_kind::shared, d);
  }
  //! \overload EXTENSION: Locks for exclusive access
  result<extent_guard> lock_file_range(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept
  {
    size_t bytes = 0;
    for(auto &i : reqs.buffers)
    {
      if(bytes + i.size() < bytes)
      {
        return errc::value_too_large;
      }
      bytes += i.size();
    }
    return lock_file_range(reqs.offset, bytes, lock_kind::exclusive, d);
  }

  LLFIO_DEADLINE_TRY_FOR_UNTIL(lock_file_range)

  /*! \brief EXTENSION: Unlocks a byte range previously locked.

  \param offset The offset to unlock. This should be an offset previously locked.
  \param bytes The number of bytes to unlock. This should be a byte extent previously locked.
  \errors Any of the values POSIX fcntl() can return.
  \mallocs None.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock_file_range(extent_type offset, extent_type bytes) noexcept;
};

/*! \brief RAII locker matching `std::unique_lock` for `lockable_io_handle`, but untemplated.
 */
class unique_file_lock
{
  lockable_io_handle *_h{nullptr};
  lock_kind _lock_state{lock_kind::unlocked};

public:
  unique_file_lock() = default;
  explicit unique_file_lock(lockable_io_handle &h, lock_kind kind)
      : _h(&h)
  {
    if(kind == lock_kind::exclusive)
    {
      lock().value();
    }
    else if(kind == lock_kind::shared)
    {
      lock_shared().value();
    }
  }
  unique_file_lock(unique_file_lock &&o) noexcept
      : _h(o._h)
      , _lock_state(o._lock_state)
  {
    o._h = nullptr;
    o._lock_state = lock_kind::unlocked;
  }
  unique_file_lock(const unique_file_lock &) = delete;
  unique_file_lock &operator=(unique_file_lock &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~unique_file_lock();
    new(this) unique_file_lock(std::move(o));
    return *this;
  }
  unique_file_lock &operator=(const unique_file_lock &) = delete;
  ~unique_file_lock()
  {
    if(_h != nullptr)
    {
      if(_lock_state == lock_kind::exclusive)
      {
        _h->unlock_file();
      }
      else if(_lock_state == lock_kind::shared)
      {
        _h->unlock_file_shared();
      }
      _h = nullptr;
    }
  }

  //! Returns the associated mutex
  lockable_io_handle *mutex() const noexcept { return _h; }
  //! True if the associated mutex is owned by this lock
  bool owns_lock() const noexcept { return (_h != nullptr) && _lock_state != lock_kind::unlocked; }
  //! True if the associated mutex is owned by this lock
  explicit operator bool() const noexcept { return owns_lock(); }

  //! Releases the mutex from management
  lockable_io_handle *release() noexcept
  {
    lockable_io_handle *ret = _h;
    _h = nullptr;
    return ret;
  }

  //! Lock the mutex for exclusive access
  result<void> lock() noexcept
  {
    if(_h == nullptr)
    {
      return errc::operation_not_permitted;
    }
    if(_lock_state != lock_kind::unlocked)
    {
      return errc::resource_deadlock_would_occur;
    }
    OUTCOME_TRY(_h->lock_file());
    _lock_state = lock_kind::exclusive;
    return success();
  }
  //! Try to lock the mutex for exclusive access
  bool try_lock() noexcept
  {
    if(_h == nullptr)
    {
      return false;
    }
    if(_lock_state != lock_kind::unlocked)
    {
      return false;
    }
    if(_h->try_lock_file())
    {
      _lock_state = lock_kind::exclusive;
      return true;
    }
    return false;
  }
  //! Unlock the mutex from exclusive access
  void unlock() noexcept
  {
    assert(_h != nullptr);
    assert(_lock_state == lock_kind::exclusive);
    if(_h == nullptr || _lock_state != lock_kind::exclusive)
    {
      return;
    }
    _h->unlock_file();
    _lock_state = lock_kind::unlocked;
  }

  //! Lock the mutex for shared access
  result<void> lock_shared() noexcept
  {
    if(_h == nullptr)
    {
      return errc::operation_not_permitted;
    }
    if(_lock_state != lock_kind::unlocked)
    {
      return errc::resource_deadlock_would_occur;
    }
    OUTCOME_TRY(_h->lock_file_shared());
    _lock_state = lock_kind::shared;
    return success();
  }
  //! Try to lock the mutex for shared access
  bool try_lock_shared() noexcept
  {
    if(_h == nullptr)
    {
      return false;
    }
    if(_lock_state != lock_kind::unlocked)
    {
      return false;
    }
    if(_h->try_lock_file_shared())
    {
      _lock_state = lock_kind::shared;
      return true;
    }
    return false;
  }
  //! Unlock the mutex from shared access
  void unlock_shared() noexcept
  {
    assert(_h != nullptr);
    assert(_lock_state == lock_kind::shared);
    if(_h == nullptr || _lock_state != lock_kind::shared)
    {
      return;
    }
    _h->unlock_file_shared();
    _lock_state = lock_kind::unlocked;
  }
};


// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/lockable_io_handle.ipp"
#else
#include "detail/impl/posix/lockable_io_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
