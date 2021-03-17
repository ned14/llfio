/* Dynamic thread pool group
(C) 2020-2021 Niall Douglas <http://www.nedproductions.biz/> (9 commits)
File Created: Dec 2020


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

#ifndef LLFIO_DYNAMIC_THREAD_POOL_GROUP_H
#define LLFIO_DYNAMIC_THREAD_POOL_GROUP_H

#include "deadline.h"

#include <memory>  // for unique_ptr and shared_ptr

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#pragma warning(disable : 4275)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class dynamic_thread_pool_group_impl;
class io_handle;

namespace detail
{
  struct global_dynamic_thread_pool_impl;
  struct global_dynamic_thread_pool_impl_workqueue_item;
  LLFIO_HEADERS_ONLY_FUNC_SPEC global_dynamic_thread_pool_impl &global_dynamic_thread_pool() noexcept;
}  // namespace detail

/*! \class dynamic_thread_pool_group
\brief Work group within the global dynamic thread pool.

Some operating systems provide a per-process global kernel thread pool capable of
dynamically adjusting its kernel thread count to how many of the threads in
the pool are currently blocked. The platform will choose the exact strategy used,
but as an example of a strategy, one might keep creating new kernel threads
so long as the total threads currently running and not blocked on page faults,
i/o or syscalls, is below the hardware concurrency. Similarly, if more threads
are running and not blocked than hardware concurrency, one might remove kernel
threads from executing work. Such a strategy would dynamically increase
concurrency until all CPUs are busy, but reduce concurrency if more work is
being done than CPUs available.

Such dynamic kernel thread pools are excellent for CPU bound processing, you
simply fire and forget work into them. However, for i/o bound processing, you
must be careful as there are gotchas. For non-seekable i/o, it is very possible
that there could be 100k handles upon which we do i/o. Doing i/o on
100k handles using a dynamic thread pool would in theory cause the creation
of 100k kernel threads, which would not be wise. A much better solution is
to use an `io_multiplexer` to await changes in large sets of i/o handles.

For seekable i/o, the same problem applies, but worse again: an i/o bound problem
would cause a rapid increase in the number of kernel threads, which by
definition makes i/o even more congested. Basically the system runs off
into pathological performance loss. You must therefore never naively do
i/o bound work (e.g. with memory mapped files) from within a dynamic thread
pool without employing some mechanism to force concurrency downwards if
the backing storage is congested.

## Work groups

Instances of this class contain zero or more work items. Each work item
is asked for its next item of work, and if an item of work is available,
that item of work is executed by the global kernel thread pool at a time
of its choosing. It is NEVER possible that any one work item is concurrently
executed at a time, each work item is always sequentially executed with
respect to itself. The only concurrency possible is *across* work items.
Therefore, if you want to execute the same piece of code concurrently,
you need to submit a separate work item for each possible amount of
concurrency (e.g. `std::thread::hardware_concurrency()`).

You can have as many or as few items of work as you like. You can
dynamically submit additional work items at any time, except when a group
is currently in the process of being stopped. The group of work items can
be waited upon to complete, after which the work group becomes reset as
if back to freshly constructed. You can also stop executing all the work
items in the group, even if they have not fully completed. If any work
item returns a failure, this equals a `stop()`, and the next `wait()` will
return that error.

Work items may create sub work groups as part of their
operation. If they do so, the work items from such nested work groups are
scheduled preferentially. This ensures good forward progress, so if you
have 100 work items each of which do another 100 work items, you don't get
10,000 slowly progressing work. Rather, the work items in the first set
progress slowly, whereas the work items in the second set progress quickly.

`work_item::next()` may optionally set a deadline to delay when that work
item ought to be processed again. Deadlines can be relative or absolute.

## C++ 23 Executors

As with elsewhere in LLFIO, as a low level facility, we don't implement
https://wg21.link/P0443 Executors, but it is trivially easy to implement
a dynamic equivalent to `std::static_thread_pool` using this class.

## Implementation notes

### Microsoft Windows

On Microsoft Windows, the Win32 thread pool API is used (https://docs.microsoft.com/en-us/windows/win32/procthread/thread-pool-api).
This is an IOCP-aware thread pool which will dynamically increase the number
of kernel threads until none are blocked. If more kernel threads
are running than twice the number of CPUs in the system, the number of kernel
threads is dynamically reduced. The maximum number of kernel threads which
will run simultaneously is 500. Note that the Win32 thread pool is shared
across the process by multiple Windows facilities.

Note that the Win32 thread pool has built in support for IOCP, so if you
have a custom i/o multiplexer, you can use the global Win32 thread pool
to execute i/o completions handling. See `CreateThreadpoolIo()` for more.

No dynamic memory allocation is performed by this implementation outside
of the initial `make_dynamic_thread_pool_group()`. The Win32 thread pool
API may perform dynamic memory allocation internally, but that is outside
our control.

Overhead of LLFIO above the Win32 thread pool API is very low, statistically
unmeasurable.

### POSIX

If not on Linux, you will need libdispatch which is detected by LLFIO cmake
during configuration. libdispatch is better known as
Grand Central Dispatch, originally a Mac OS technology but since ported
to a high quality kernel based implementation on recent FreeBSDs, and to
a lower quality userspace based implementation on Linux. Generally
libdispatch should get automatically found on Mac OS without additional
effort; on FreeBSD it may need installing from ports; on Linux you would
need to explicitly install `libdispatch-dev` or the equivalent. You can
force the use in cmake of libdispatch by setting the cmake variable
`LLFIO_USE_LIBDISPATCH` to On.

Overhead of LLFIO above the libdispatch API is very low, statistically
unmeasurable.

### Linux

On Linux only, we have a custom userspace implementation with superior performance.
A similar strategy to Microsoft Windows' approach is used. We
dynamically increase the number of kernel threads until none are sleeping
awaiting i/o. If more kernel threads are running than three more than the number of
CPUs in the system, the number of kernel threads is dynamically reduced.
Note that **all** the kernel threads for the current process are considered,
not just the kernel threads created by this thread pool implementation.
Therefore, if you have alternative thread pool implementations (e.g. OpenMP,
`std::async`), those are also included in the dynamic adjustment.

As this is wholly implemented by this library, dynamic memory allocation
occurs in the initial `make_dynamic_thread_pool_group()` and per thread
creation, but otherwise the implementation does not perform dynamic memory
allocations.

After multiple rewrites, eventually I got this custom userspace implementation
to have superior performance to both ASIO and libdispatch. For larger work
items the difference is meaningless between all three, however for smaller
work items I benchmarked this custom userspace implementation as beating
(non-dynamic) ASIO by approx 29% and Linux libdispatch by approx 52% (note
that Linux libdispatch appears to have a scale up bug when work items are
small and few, it is often less than half the performance of LLFIO's custom
implementation).
*/
class LLFIO_DECL dynamic_thread_pool_group
{
  friend class dynamic_thread_pool_group_impl;
public:
  //! An individual item of work within the work group.
  class work_item
  {
    friend struct detail::global_dynamic_thread_pool_impl;
    friend struct detail::global_dynamic_thread_pool_impl_workqueue_item;
    friend class dynamic_thread_pool_group_impl;
    std::atomic<dynamic_thread_pool_group_impl *> _parent{nullptr};
    void *_internalworkh{nullptr};
    void *_internaltimerh{nullptr};  // lazily created if next() ever returns a deadline
    work_item *_prev{nullptr}, *_next{nullptr}, *_next_scheduled{nullptr};
    std::atomic<intptr_t> _nextwork{-1};
    std::chrono::steady_clock::time_point _timepoint1;
    std::chrono::system_clock::time_point _timepoint2;
    int _internalworkh_inuse{0};

  protected:
    constexpr bool _has_timer_set_relative() const noexcept { return _timepoint1 != std::chrono::steady_clock::time_point(); }
    constexpr bool _has_timer_set_absolute() const noexcept { return _timepoint2 != std::chrono::system_clock::time_point(); }
    constexpr bool _has_timer_set() const noexcept { return _has_timer_set_relative() || _has_timer_set_absolute(); }

    constexpr work_item() {}
    work_item(const work_item &o) = delete;
    work_item(work_item &&o) noexcept
        : _parent(o._parent.load(std::memory_order_relaxed))
        , _internalworkh(o._internalworkh)
        , _internaltimerh(o._internaltimerh)
        , _prev(o._prev)
        , _next(o._next)
        , _next_scheduled(o._next_scheduled)
        , _nextwork(o._nextwork.load(std::memory_order_relaxed))
        , _timepoint1(o._timepoint1)
        , _timepoint2(o._timepoint2)
        , _internalworkh_inuse(o._internalworkh_inuse)
    {
      assert(o._parent.load(std::memory_order_relaxed) == nullptr);
      assert(o._internalworkh == nullptr);
      assert(o._internaltimerh == nullptr);
      if(o._parent.load(std::memory_order_relaxed) != nullptr || o._internalworkh != nullptr)
      {
        LLFIO_LOG_FATAL(this, "FATAL: dynamic_thread_pool_group::work_item was relocated in memory during use!");
        abort();
      }
      o._prev = o._next = o._next_scheduled = nullptr;
      o._nextwork.store(-1, std::memory_order_relaxed);
      o._internalworkh_inuse = 0;
    }
    work_item &operator=(const work_item &) = delete;
    work_item &operator=(work_item &&) = delete;

  public:
    virtual ~work_item()
    {
      assert(_nextwork.load(std::memory_order_relaxed) == -1);
      if(_nextwork.load(std::memory_order_relaxed) != -1)
      {
        LLFIO_LOG_FATAL(this, "FATAL: dynamic_thread_pool_group::work_item destroyed before all work was done!");
        abort();
      }
      assert(_internalworkh == nullptr);
      assert(_internaltimerh == nullptr);
      assert(_parent == nullptr);
      if(_internalworkh != nullptr || _parent != nullptr)
      {
        LLFIO_LOG_FATAL(this, "FATAL: dynamic_thread_pool_group::work_item destroyed before group_complete() was executed!");
        abort();
      }
    }

    //! Returns the parent work group between successful submission and just before `group_complete()`.
    dynamic_thread_pool_group *parent() const noexcept { return reinterpret_cast<dynamic_thread_pool_group *>(_parent.load(std::memory_order_relaxed)); }

    /*! Invoked by the i/o thread pool to determine if this work item
    has more work to do.

    \return If there is no work _currently_ available to do, but there
    might be some later, you should return zero. You will be called again
    later after other work has been done. If you return -1, you are
    saying that no further work will be done, and the group need never
    call you again. If you have more work you want to do, return any
    other value.
    \param d Optional delay before the next item of work ought to be
    executed (return != 0), or `next()` ought to be called again to
    determine the next item (return == 0). On entry `d` is set to no
    delay, so if you don't modify it, the next item of work occurs
    as soon as possible.

    Note that this function is called from multiple kernel threads.
    You must NOT do any significant work in this function.
    In particular do NOT call any dynamic thread pool group function,
    as you will experience deadlock.

    `dynamic_thread_pool_group::current_work_item()` may have any
    value during this call.
    */
    virtual intptr_t next(deadline &d) noexcept = 0;

    /*! Invoked by the i/o thread pool to perform the next item of work.

    \return Any failure causes all remaining work in this group to
    be cancelled as soon as possible.
    \param work The value returned by `next()`.

    Note that this function is called from multiple kernel threads,
    and may not be the kernel thread from which `next()`
    was called.

    `dynamic_thread_pool_group::current_work_item()` will always be
    `this` during this call.
    */
    virtual result<void> operator()(intptr_t work) noexcept = 0;

    /*! Invoked by the i/o thread pool when all work in this thread
    pool group is complete.

    `cancelled` indicates if this is an abnormal completion. If its
    error compares equal to `errc::operation_cancelled`, then `stop()`
    was called.

    Just before this is called for all work items submitted, the group
    becomes reset to fresh, and `parent()` becomes null. You can resubmit
    this work item, but do not submit other work items until their
    `group_complete()` has been invoked.

    Note that this function is called from multiple kernel threads.

    `dynamic_thread_pool_group::current_work_item()` may have any
    value during this call.
    */
    virtual void group_complete(const result<void> &cancelled) noexcept { (void) cancelled; }
  };

  /*! \class io_aware_work_item
  \brief A work item which paces when it next executes according to i/o congestion.

  Currently there is only a working implementation of this for the Microsoft Windows
  and Linux platforms, due to lack of working `statfs_t::f_iosinprogress` on other
  platforms. If retrieving that for a seekable handle does not work, the constructor
  throws an exception.

  For seekable handles, currently `reads`, `writes` and `barriers` are ignored. We
  simply retrieve, periodically, `statfs_t::f_iosinprogress` and `statfs_t::f_iosbusytime`
  for the storage devices backing the seekable handle. If the recent averaged i/o wait time exceeds
  `max_iosbusytime` and the i/o in progress > `max_iosinprogress`, `next()` will
  start setting the default deadline passed to
  `io_aware_next()`. Thereafter, every 1/10th of a second, if `statfs_t::f_iosinprogress`
  is above `max_iosinprogress`, it will increase the deadline by 1/16th, whereas if it is
  below `min_iosinprogress`, it will decrease the deadline by 1/16th. The default deadline
  chosen is always the worst of all the
  storage devices of all the handles. This will reduce concurrency within the kernel thread pool
  in order to reduce congestion on the storage devices. If at any point `statfs_t::f_iosbusytime`
  drops below `max_iosbusytime` as averaged across one second, and `statfs_t::f_iosinprogress` drops
  below `min_iosinprogress`, the additional
  throttling is completely removed. `io_aware_next()` can ignore the default deadline
  passed into it, and can set any other deadline.

  For non-seekable handles, the handle must have an i/o multiplexer set upon it, and on
  Microsoft Windows, that i/o multiplexer must be utilising the IOCP instance of the
  global Win32 thread pool. For each `reads`, `writes` and `barriers` which is non-zero,
  a corresponding zero length i/o is constructed and initiated. When the i/o completes,
  and all readable handles in the work item's set have data waiting to be read, and all
  writable handles in the work item's set have space to allow writes, only then is the
  work item invoked with the next piece of work.

  \note Non-seekable handle support is not implemented yet.
  */
  class LLFIO_DECL io_aware_work_item : public work_item
  {
  public:
    //! Maximum i/o busyness above which throttling is to begin.
    float max_iosbusytime{0.95f};
    //! Minimum i/o in progress to target if `iosbusytime` exceeded. The default of 16 suits SSDs, you want around 4 for spinning rust or NV-RAM.
    uint32_t min_iosinprogress{16};
    //! Maximum i/o in progress to target if `iosbusytime` exceeded. The default of 32 suits SSDs, you want around 8 for spinning rust or NV-RAM.
#ifdef _WIN32
    uint32_t max_iosinprogress{1};  // windows appears to do a lot of i/o coalescing
#else
    uint32_t max_iosinprogress{32};
#endif
    //! Information about an i/o handle this work item will use
    struct io_handle_awareness
    {
      //! An i/o handle this work item will use
      io_handle *h{nullptr};
      //! The relative amount of reading done by this work item from the handle.
      float reads{0};
      //! The relative amount of writing done by this work item to the handle.
      float writes{0};
      //! The relative amount of write barriering done by this work item to the handle.
      float barriers{0};

      void *_internal{nullptr};
    };

  private:
    const span<io_handle_awareness> _handles;

    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC intptr_t next(deadline &d) noexcept override final;

  public:
    constexpr io_aware_work_item() {}
    /*! \brief Constructs a work item aware of i/o done to the handles in `hs`.

    Note that the `reads`, `writes` and `barriers` are normalised to proportions
    out of `1.0` by this constructor, so if for example you had `reads/writes/barriers = 200/100/0`,
    after normalisation those become `0.66/0.33/0.0` such that the total is `1.0`.
    If `reads/writes/barriers = 0/0/0` on entry, they are replaced with `0.5/0.5/0.0`.

    Note that normalisation is across *all* i/o handles in the set, so three handles
    each with `reads/writes/barriers = 200/100/0` on entry would have `0.22/0.11/0.0`
    each after construction.
    */
    explicit LLFIO_HEADERS_ONLY_MEMFUNC_SPEC io_aware_work_item(span<io_handle_awareness> hs);
    io_aware_work_item(io_aware_work_item &&o) noexcept
        : work_item(std::move(o))
        , _handles(o._handles)
    {
    }
    LLFIO_HEADERS_ONLY_MEMFUNC_SPEC ~io_aware_work_item();

    //! The handles originally registered during construction.
    span<io_handle_awareness> handles() const noexcept { return _handles; }

    /*! \brief As for `work_item::next()`, but deadline may be extended to
    reduce i/o congestion on the hardware devices to which the handles
    refer.
    */
    virtual intptr_t io_aware_next(deadline &d) noexcept = 0;
  };

  virtual ~dynamic_thread_pool_group() {}

  /*! \brief A textual description of the underlying implementation of
  this dynamic thread pool group.

  The current possible underlying implementations are:

  - "Grand Central Dispatch" (Mac OS, FreeBSD, Linux)
  - "Linux native" (Linux)
  - "Win32 thread pool (Vista+)" (Windows)

  Which one is chosen depends on what was detected at cmake configure time,
  and possibly what the host OS running the program binary supports.
  */
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const char *implementation_description() noexcept;

  /*! \brief Threadsafe. Submit one or more work items for execution. Note that you can submit more later.

  Note that if the group is currently stopping, you cannot submit more
  work until the group has stopped. An error code comparing equal to
  `errc::operation_canceled` is returned if you try.
  */
  virtual result<void> submit(span<work_item *> work) noexcept = 0;
  //! \overload
  result<void> submit(work_item *wi) noexcept { return submit(span<work_item *>(&wi, 1)); }
  //! \overload
  LLFIO_TEMPLATE(class T)
  LLFIO_TREQUIRES(LLFIO_TPRED(!std::is_pointer<T>::value), LLFIO_TPRED(std::is_base_of<work_item, T>::value))
  result<void> submit(span<T> wi) noexcept
  {
    auto *wis = (T **) alloca(sizeof(T *) * wi.size());
    for(size_t n = 0; n < wi.size(); n++)
    {
      wis[n] = &wi[n];
    }
    return submit(span<work_item *>((work_item **) wis, wi.size()));
  }

  //! Threadsafe. Cancel any remaining work previously submitted, but without blocking (use `wait()` to block).
  virtual result<void> stop() noexcept = 0;

  /*! \brief Threadsafe. True if a work item reported an error, or
  `stop()` was called, but work items are still running.
  */
  virtual bool stopping() const noexcept = 0;

  //! Threadsafe. True if all the work previously submitted is complete.
  virtual bool stopped() const noexcept = 0;

  //! Threadsafe. Wait for work previously submitted to complete, returning any failures by any work item.
  virtual result<void> wait(deadline d = {}) const noexcept = 0;
  //! \overload
  template <class Rep, class Period> result<bool> wait_for(const std::chrono::duration<Rep, Period> &duration) const noexcept
  {
    auto r = wait(duration);
    if(!r && r.error() == errc::timed_out)
    {
      return false;
    }
    OUTCOME_TRY(std::move(r));
    return true;
  }
  //! \overload
  template <class Clock, class Duration> result<bool> wait_until(const std::chrono::time_point<Clock, Duration> &timeout) const noexcept
  {
    auto r = wait(timeout);
    if(!r && r.error() == errc::timed_out)
    {
      return false;
    }
    OUTCOME_TRY(std::move(r));
    return true;
  }

  //! Returns the work item nesting level which would be used if a new dynamic thread pool group were created within the current work item.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t current_nesting_level() noexcept;
  //! Returns the work item the calling thread is running within, if any.
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC work_item *current_work_item() noexcept;
  /*! \brief Returns the number of milliseconds that a thread is without work before it is shut down.
  Note that this will be zero on all but on Linux if using our local thread pool
  implementation, because the system controls this value on Windows, Grand Central
  Dispatch etc.
  */
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint32_t ms_sleep_for_more_work() noexcept;
  /*! \brief Sets the number of milliseconds that a thread is without work before it is shut down,
  returning the value actually set.

  Note that this will have no effect (and thus return zero) on all but on Linux if
  using our local thread pool implementation, because the system controls this value
  on Windows, Grand Central Dispatch etc.
  */
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint32_t ms_sleep_for_more_work(uint32_t v) noexcept;
};
//! A unique ptr to a work group within the global dynamic thread pool.
using dynamic_thread_pool_group_ptr = std::unique_ptr<dynamic_thread_pool_group>;

//! Creates a new work group within the global dynamic thread pool.
LLFIO_HEADERS_ONLY_FUNC_SPEC result<dynamic_thread_pool_group_ptr> make_dynamic_thread_pool_group() noexcept;

// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/dynamic_thread_pool_group.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif
