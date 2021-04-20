/* A handle to something
(C) 2015-2020 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#ifndef LLFIO_IO_HANDLE_H
#define LLFIO_IO_HANDLE_H

#include "io_multiplexer.hpp"

//! \file io_handle.hpp Provides a byte-orientated i/o handle

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class io_handle
\brief A handle to something capable of scatter-gather byte i/o.
*/
class LLFIO_DECL io_handle : public handle
{
  friend class io_multiplexer;

public:
  using path_type = handle::path_type;
  using extent_type = handle::extent_type;
  using size_type = handle::size_type;
  using mode = handle::mode;
  using creation = handle::creation;
  using caching = handle::caching;
  using flag = handle::flag;
  using barrier_kind = io_multiplexer::barrier_kind;
  using buffer_type = io_multiplexer::buffer_type;
  using const_buffer_type = io_multiplexer::const_buffer_type;
  using buffers_type = io_multiplexer::buffers_type;
  using const_buffers_type = io_multiplexer::const_buffers_type;
  using registered_buffer_type = io_multiplexer::registered_buffer_type;
  template <class T> using io_request = io_multiplexer::io_request<T>;
  template <class T> using io_result = io_multiplexer::io_result<T>;
  template <class T> using awaitable = io_multiplexer::awaitable<T>;

protected:
  io_multiplexer *_ctx{nullptr};  // +4 or +8 bytes

public:
  //! Default constructor
  constexpr io_handle() {}  // NOLINT
  ~io_handle() = default;
  //! Construct a handle from a supplied native handle
  constexpr explicit io_handle(native_handle_type h, caching caching, flag flags, io_multiplexer *ctx)
      : handle(h, caching, flags)
      , _ctx(ctx)
  {
  }
  //! Explicit conversion from handle permitted
  explicit constexpr io_handle(handle &&o, io_multiplexer *ctx) noexcept
      : handle(std::move(o))
      , _ctx(ctx)
  {
  }
  //! Move construction permitted
  io_handle(io_handle &&) = default;
  //! No copy construction (use `clone()`)
  io_handle(const io_handle &) = delete;
  //! Move assignment permitted
  io_handle &operator=(io_handle &&) = default;
  //! No copy assignment
  io_handle &operator=(const io_handle &) = delete;

  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
  {
    if(_ctx != nullptr)
    {
      OUTCOME_TRY(set_multiplexer(nullptr));
    }
    return handle::close();
  }

  /*! \brief The i/o multiplexer this handle will use to multiplex i/o. If this returns null,
  then this handle has not been registered with an i/o multiplexer yet.
  */
  io_multiplexer *multiplexer() const noexcept { return _ctx; }

  /*! \brief Sets the i/o multiplexer this handle will use to implement `read()`, `write()` and `barrier()`.

  Note that this call deregisters this handle from any existing i/o multiplexer, and registers
  it with the new i/o multiplexer. You must therefore not call it if any i/o is currently
  outstanding on this handle. You should also be aware that multiple dynamic memory
  allocations and deallocations may occur, as well as multiple syscalls (i.e. this is
  an expensive call, try to do it from cold code).

  If the handle was not created as multiplexable, this call always fails.

  \mallocs Multiple dynamic memory allocations and deallocations.
  */
  virtual result<void> set_multiplexer(io_multiplexer *c = this_thread::multiplexer()) noexcept;  // implementation is below

protected:
  //! The virtualised implementation of `max_buffers()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC size_t _do_max_buffers() const noexcept;
  //! The virtualised implementation of `allocate_registered_buffer()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<registered_buffer_type> _do_allocate_registered_buffer(size_t &bytes) noexcept;  // default implementation is in map_handle.hpp
  //! The virtualised implementation of `read()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_read(io_request<buffers_type> reqs, deadline d) noexcept;
  //! The virtualised implementation of `read()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_read(registered_buffer_type base, io_request<buffers_type> reqs, deadline d) noexcept
  {
    (void) base;
    return _do_read(reqs, d);
  }
  //! The virtualised implementation of `write()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, deadline d) noexcept;
  //! The virtualised implementation of `write()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_write(registered_buffer_type base, io_request<const_buffers_type> reqs, deadline d) noexcept
  {
    (void) base;
    return _do_write(reqs, d);
  }
  //! The virtualised implementation of `barrier()` used if no multiplexer has been set.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_barrier(io_request<const_buffers_type> reqs, barrier_kind kind, deadline d) noexcept;

  io_result<buffers_type> _do_multiplexer_read(registered_buffer_type &&base, io_request<buffers_type> reqs, deadline d) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    const auto state_reqs = _ctx->io_state_requirements();
    auto *storage = (byte *) alloca(state_reqs.first + state_reqs.second);
    const auto diff = (uintptr_t) storage & (state_reqs.second - 1);
    storage += state_reqs.second - diff;
    auto *state = _ctx->construct_and_init_io_operation({storage, state_reqs.first}, this, nullptr, std::move(base), d, std::move(reqs));
    OUTCOME_TRY(_ctx->flush_inited_io_operations());
    while(!is_finished(_ctx->check_io_operation(state)))
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(_ctx->check_for_any_completed_io(nd));
    }
    io_result<buffers_type> ret = std::move(*state).get_completed_read();
    state->~io_operation_state();
    return ret;
  }
  io_result<const_buffers_type> _do_multiplexer_write(registered_buffer_type &&base, io_request<const_buffers_type> reqs, deadline d) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    const auto state_reqs = _ctx->io_state_requirements();
    auto *storage = (byte *) alloca(state_reqs.first + state_reqs.second);
    const auto diff = (uintptr_t) storage & (state_reqs.second - 1);
    storage += state_reqs.second - diff;
    auto *state = _ctx->construct_and_init_io_operation({storage, state_reqs.first}, this, nullptr, std::move(base), d, std::move(reqs));
    OUTCOME_TRY(_ctx->flush_inited_io_operations());
    while(!is_finished(_ctx->check_io_operation(state)))
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(_ctx->check_for_any_completed_io(nd));
    }
    io_result<const_buffers_type> ret = std::move(*state).get_completed_write_or_barrier();
    state->~io_operation_state();
    return ret;
  }
  io_result<const_buffers_type> _do_multiplexer_barrier(registered_buffer_type &&base, io_request<const_buffers_type> reqs, barrier_kind kind, deadline d) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    const auto state_reqs = _ctx->io_state_requirements();
    auto *storage = (byte *) alloca(state_reqs.first + state_reqs.second);
    const auto diff = (uintptr_t) storage & (state_reqs.second - 1);
    storage += state_reqs.second - diff;
    auto *state = _ctx->construct_and_init_io_operation({storage, state_reqs.first}, this, nullptr, std::move(base), d, std::move(reqs), kind);
    OUTCOME_TRY(_ctx->flush_inited_io_operations());
    while(!is_finished(_ctx->check_io_operation(state)))
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(_ctx->check_for_any_completed_io(nd));
    }
    io_result<const_buffers_type> ret = std::move(*state).get_completed_write_or_barrier();
    state->~io_operation_state();
    return ret;
  }

public:
  /*! \brief The *maximum* number of buffers which a single read or write syscall can (atomically)
  process at a time for this specific open handle. On POSIX, this is known as `IOV_MAX`.
  Preferentially uses any i/o multiplexer set over the virtually overridable per-class implementation.

  Note that the actual number of buffers accepted for a read or a write may be significantly
  lower than this system-defined limit, depending on available resources. The `read()` or `write()`
  call will return the buffers accepted at the time of invoking the syscall.

  Note also that some OSs will error out if you supply more than this limit to `read()` or `write()`,
  but other OSs do not. Some OSs guarantee that each i/o syscall has effects atomically visible or not
  to other i/o, other OSs do not.

  OS X does not implement scatter-gather file i/o syscalls. Thus this function will always return
  `1` in that situation.

  Microsoft Windows *may* implement scatter-gather i/o under certain handle configurations.
  Most of the time for non-socket handles this function will return `1`.

  For handles which implement i/o entirely in user space, and thus syscalls are not involved,
  this function will return `0`.
  */
  size_t max_buffers() const noexcept
  {
    if(_ctx == nullptr)
    {
      return _do_max_buffers();
    }
    return _ctx->do_io_handle_max_buffers(this);
  }

  /*! \brief Request the allocation of a new registered i/o buffer with the system suitable
  for maximum performance i/o, preferentially using any i/o multiplexer set over the
  virtually overridable per-class implementation.

  \return A shared pointer to the i/o buffer. Note that the pointer returned is not
  the resource under management, using shared ptr's aliasing feature.
  \param bytes The size of the i/o buffer requested. This may be rounded (considerably)
  upwards, you should always use the value returned.

  Some i/o multiplexer implementations have the ability to allocate i/o buffers in special
  memory shared between the i/o hardware and user space processes. Using registered i/o
  buffers can entirely eliminate all kernel transitions and memory copying during i/o, and
  can saturate very high end hardware from a single kernel thread.

  If no multiplexer is set, the default implementation uses `map_handle` to allocate raw
  memory pages from the OS kernel. If the requested buffer size is a multiple of one of
  the larger page sizes from `utils::page_sizes()`, an attempt to satisfy the request
  using the larger page size will be attempted first.
  */
  LLFIO_MAKE_FREE_FUNCTION
  result<registered_buffer_type> allocate_registered_buffer(size_t &bytes) noexcept
  {
    if(_ctx == nullptr)
    {
      return _do_allocate_registered_buffer(bytes);
    }
    return _ctx->do_io_handle_allocate_registered_buffer(this, bytes);
  }

  /*! \brief Read data from the open handle, preferentially using any i/o multiplexer set over the
  virtually overridable per-class implementation.

  \warning Depending on the implementation backend, **very** different buffers may be returned than you
  supplied. You should **always** use the buffers returned and assume that they point to different
  memory and that each buffer's size will have changed.

  \return The buffers read, which may not be the buffers input. The size of each scatter-gather
  buffer returned is updated with the number of bytes of that buffer transferred, and the pointer
  to the data may be \em completely different to what was submitted (e.g. it may point into a
  memory map).
  \param reqs A scatter-gather and offset request.
  \param d An optional deadline by which the i/o must complete, else it is cancelled.
  Note function may return significantly after this deadline if the i/o takes long to cancel.
  \errors Any of the values POSIX read() can return, `errc::timed_out`, `errc::operation_canceled`. `errc::not_supported` may be
  returned if deadline i/o is not possible with this particular handle configuration (e.g.
  reading from regular files on POSIX or reading from a non-overlapped HANDLE on Windows).
  \mallocs The default synchronous implementation in file_handle performs no memory allocation.
  */
  LLFIO_MAKE_FREE_FUNCTION
  io_result<buffers_type> read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept { return (_ctx == nullptr) ? _do_read(reqs, d) : _do_multiplexer_read({}, reqs, d); }
  //! \overload Registered buffer overload, scatter list **must** be wholly within the registered buffer
  LLFIO_MAKE_FREE_FUNCTION
  io_result<buffers_type> read(registered_buffer_type base, io_request<buffers_type> reqs, deadline d = deadline()) noexcept { return (_ctx == nullptr) ? _do_read(std::move(base), reqs, d) : _do_multiplexer_read(std::move(base), reqs, d); }
  //! \overload Convenience initialiser list based overload for `read()`
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

  /*! \brief Write data to the open handle, preferentially using any i/o multiplexer set over the
  virtually overridable per-class implementation.

  \warning Depending on the implementation backend, not all of the buffers input may be written.
  For example, with a zeroed deadline, some backends may only consume as many buffers as the system has available write slots
  for, thus for those backends this call is "non-blocking" in the sense that it will return immediately even if it
  could not schedule a single buffer write. Another example is that some implementations will not
  auto-extend the length of a file when a write exceeds the maximum extent, you will need to issue
  a `truncate(newsize)` first.

  \return The buffers written, which may not be the buffers input. The size of each scatter-gather
  buffer returned is updated with the number of bytes of that buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d An optional deadline by which the i/o must complete, else it is cancelled.
  Note function may return significantly after this deadline if the i/o takes long to cancel.
  \errors Any of the values POSIX write() can return, `errc::timed_out`, `errc::operation_canceled`. `errc::not_supported` may be
  returned if deadline i/o is not possible with this particular handle configuration (e.g.
  writing to regular files on POSIX or writing to a non-overlapped HANDLE on Windows).
  \mallocs The default synchronous implementation in file_handle performs no memory allocation.
  */
  LLFIO_MAKE_FREE_FUNCTION
  io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept { return (_ctx == nullptr) ? _do_write(reqs, d) : _do_multiplexer_write({}, std::move(reqs), d); }
  //! \overload Registered buffer overload, gather list **must** be wholly within the registered buffer
  LLFIO_MAKE_FREE_FUNCTION
  io_result<const_buffers_type> write(registered_buffer_type base, io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept { return (_ctx == nullptr) ? _do_write(std::move(base), reqs, d) : _do_multiplexer_write(std::move(base), std::move(reqs), d); }
  //! \overload Convenience initialiser list based overload for `write()`
  LLFIO_MAKE_FREE_FUNCTION
  io_result<size_type> write(extent_type offset, std::initializer_list<const_buffer_type> lst, deadline d = deadline()) noexcept
  {
    const_buffer_type *_reqs = reinterpret_cast<const_buffer_type *>(alloca(sizeof(const_buffer_type) * lst.size()));
    memcpy(_reqs, lst.begin(), sizeof(const_buffer_type) * lst.size());
    io_request<const_buffers_type> reqs(const_buffers_type(_reqs, lst.size()), offset);
    auto ret = write(reqs, d);
    if(ret)
    {
      return ret.bytes_transferred();
    }
    return std::move(ret).error();
  }

  LLFIO_DEADLINE_TRY_FOR_UNTIL(write)

  /*! \brief Issue a write reordering barrier such that writes preceding the barrier will reach
  storage before writes after this barrier, preferentially using any i/o multiplexer set over the
  virtually overridable per-class implementation.

  \warning **Assume that this call is a no-op**. It is not reliably implemented in many common
  use cases, for example if your code is running inside a LXC container, or if the user has mounted
  the filing system with non-default options. Instead open the handle with `caching::reads` which
  means that all writes form a strict sequential order not completing until acknowledged by the
  storage device. Filing system can and do use different algorithms to give much better performance
  with `caching::reads`, some (e.g. ZFS) spectacularly better.

  \warning Let me repeat again: consider this call to be a **hint** to poke the kernel with a stick
  to go start to do some work sooner rather than later. **It may be ignored entirely**.

  \warning For portability, you can only assume that barriers write order for a single handle
  instance. You cannot assume that barriers write order across multiple handles to the same inode,
  or across processes.

  \return The buffers barriered, which may not be the buffers input. The size of each scatter-gather
  buffer is updated with the number of bytes of that buffer barriered.
  \param reqs A scatter-gather and offset request for what range to barrier. May be ignored on
  some platforms which always write barrier the entire file. Supplying a default initialised reqs
  write barriers the entire file.
  \param kind Which kind of write reordering barrier to perform.
  \param d An optional deadline by which the i/o must complete, else it is cancelled.
  Note function may return significantly after this deadline if the i/o takes long to cancel.
  \errors Any of the values POSIX fdatasync() or Windows NtFlushBuffersFileEx() can return.
  \mallocs None.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), barrier_kind kind = barrier_kind::nowait_data_only, deadline d = deadline()) noexcept
  {
    return (_ctx == nullptr) ? _do_barrier(reqs, kind, d) : _do_multiplexer_barrier({}, std::move(reqs), kind, d);
  }
  //! \overload Convenience overload
  LLFIO_MAKE_FREE_FUNCTION
  io_result<const_buffers_type> barrier(barrier_kind kind, deadline d = deadline()) noexcept { return barrier(io_request<const_buffers_type>(), kind, d); }

  LLFIO_DEADLINE_TRY_FOR_UNTIL(barrier)

public:
  /*! \brief A coroutinised equivalent to `.read()` which suspends the coroutine until
  the i/o finishes. **Blocks execution** i.e is equivalent to `.read()` if no i/o multiplexer
  has been set on this handle!

  The awaitable returned is **eager** i.e. it immediately begins the i/o. If the i/o completes
  and finishes immediately, no coroutine suspension occurs.
  */
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<buffers_type>> co_read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<buffers_type>>(read(std::move(reqs), d));
    }
    awaitable<io_result<buffers_type>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, {}, d, std::move(reqs)));
    return ret;
  }
  //! \overload Registered buffer overload, scatter list **must** be wholly within the registered buffer
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<buffers_type>> co_read(registered_buffer_type base, io_request<buffers_type> reqs, deadline d = deadline()) noexcept
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<buffers_type>>(read(std::move(base), std::move(reqs), d));
    }
    awaitable<io_result<buffers_type>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, std::move(base), d, std::move(reqs)));
    return ret;
  }

  /*! \brief A coroutinised equivalent to `.write()` which suspends the coroutine until
  the i/o finishes. **Blocks execution** i.e is equivalent to `.write()` if no i/o multiplexer
  has been set on this handle!

  The awaitable returned is **eager** i.e. it immediately begins the i/o. If the i/o completes
  and finishes immediately, no coroutine suspension occurs.
  */
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<const_buffers_type>> co_write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<const_buffers_type>>(write(std::move(reqs), d));
    }
    awaitable<io_result<const_buffers_type>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, {}, d, std::move(reqs)));
    return ret;
  }
  //! \overload Registered buffer overload, gather list **must** be wholly within the registered buffer
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<const_buffers_type>> co_write(registered_buffer_type base, io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<const_buffers_type>>(write(std::move(base), std::move(reqs), d));
    }
    awaitable<io_result<const_buffers_type>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, std::move(base), d, std::move(reqs)));
    return ret;
  }

  /*! \brief A coroutinised equivalent to `.barrier()` which suspends the coroutine until
  the i/o finishes. **Blocks execution** i.e is equivalent to `.barrier()` if no i/o multiplexer
  has been set on this handle!

  The awaitable returned is **eager** i.e. it immediately begins the i/o. If the i/o completes
  and finishes immediately, no coroutine suspension occurs.
  */
  LLFIO_MAKE_FREE_FUNCTION
  awaitable<io_result<const_buffers_type>> co_barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), barrier_kind kind = barrier_kind::nowait_data_only, deadline d = deadline()) noexcept
  {
    if(_ctx == nullptr)
    {
      return awaitable<io_result<const_buffers_type>>(barrier(std::move(reqs), kind, d));
    }
    awaitable<io_result<const_buffers_type>> ret;
    ret.set_state(_ctx->construct(ret._state_storage, this, nullptr, {}, d, std::move(reqs), kind));
    return ret;
  }
};
static_assert((sizeof(void *) == 4 && sizeof(io_handle) == 20) || (sizeof(void *) == 8 && sizeof(io_handle) == 32), "io_handle is not 20 or 32 bytes in size!");

// Out of line definition purely to work around a bug in GCC where if marked inline,
// its visibility is hidden and links fail
inline result<void> io_handle::set_multiplexer(io_multiplexer *c) noexcept
{
  if(!is_multiplexable())
  {
    return errc::operation_not_supported;
  }
  if(c == _ctx)
  {
    return success();
  }
  if(_ctx != nullptr)
  {
    OUTCOME_TRY(_ctx->do_io_handle_deregister(this));
    _ctx = nullptr;
  }
  if(c != nullptr)
  {
    OUTCOME_TRY(auto &&state, c->do_io_handle_register(this));
    _v.behaviour = (_v.behaviour & ~(native_handle_type::disposition::_multiplexer_state_bit0 | native_handle_type::disposition::_multiplexer_state_bit1));
    if((state & 1) != 0)
    {
      _v.behaviour |= native_handle_type::disposition::_multiplexer_state_bit0;
    }
    if((state & 2) != 0)
    {
      _v.behaviour |= native_handle_type::disposition::_multiplexer_state_bit1;
    }
  }
  _ctx = c;
  return success();
}

inline size_t io_multiplexer::do_io_handle_max_buffers(const io_handle *h) const noexcept
{
  return h->_do_max_buffers();
}
inline result<io_multiplexer::registered_buffer_type> io_multiplexer::do_io_handle_allocate_registered_buffer(io_handle *h, size_t &bytes) noexcept
{
  return h->_do_allocate_registered_buffer(bytes);
}
template <class T> inline bool io_multiplexer::awaitable<T>::await_ready() noexcept
{
  auto state = _state->current_state();
  if(is_initialised(state))
  {
    // Begin the i/o
    state = _state->h->multiplexer()->init_io_operation(_state);
    // std::cout << "Coroutine " << _state << " begins i/o, state on return was " << (int) state << std::endl;
  }
  return is_finished(state);
}
template <class T> inline io_multiplexer::awaitable<T>::~awaitable()
{
  if(_state != nullptr)
  {
    auto state = _state->current_state();
    if(state != io_operation_state_type::unknown && !is_initialised(state) && !is_finished(state))
    {
#if LLFIO_ENABLE_COROUTINES
      // Resumption of the coroutine at this point causes recursion into this destructor, so prevent that
      _coro = {};
#endif
      state = _state->h->multiplexer()->check_io_operation(_state);
      if(!is_completed(state))
      {
        // Cancel the i/o
        (void) _state->h->multiplexer()->cancel_io_operation(_state);
      }
      while(!is_finished(state))
      {
        // Block forever until I finish
        (void) _state->h->multiplexer()->check_for_any_completed_io({});
        state = _state->current_state();
      }
    }
    _state->~io_operation_state();
  }
}

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
*/
inline io_handle::io_result<io_handle::const_buffers_type> write(io_handle &self, io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d = deadline()) noexcept
{
  return self.write(std::forward<decltype(reqs)>(reqs), std::forward<decltype(d)>(d));
}
//! \overload
inline io_handle::io_result<io_handle::size_type> write(io_handle &self, io_handle::extent_type offset, std::initializer_list<io_handle::const_buffer_type> lst, deadline d = deadline()) noexcept
{
  return self.write(std::forward<decltype(offset)>(offset), std::forward<decltype(lst)>(lst), std::forward<decltype(d)>(d));
}
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
#include "detail/impl/test/null_multiplexer.ipp"
#endif

#ifdef _WIN32
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
#include "detail/impl/windows/test/iocp_multiplexer.ipp"
#else
#include "detail/impl/windows/io_handle.ipp"
#endif
#else
#if LLFIO_ENABLE_TEST_IO_MULTIPLEXERS
//#include "detail/impl/posix/test/io_uring_multiplexer.ipp"
#else
#endif
#include "detail/impl/posix/io_handle.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
