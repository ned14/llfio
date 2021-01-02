/* Dynamic thread pool group
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (9 commits)
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

#include "../../dynamic_thread_pool_group.hpp"

#include "../../file_handle.hpp"
#include "../../statfs.hpp"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <iostream>

#ifndef LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
#if LLFIO_FORCE_USE_LIBDISPATCH
#include <dispatch/dispatch.h>
#define LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD 1
#else
#ifdef _WIN32
#include "windows/import.hpp"
#include <threadpoolapiset.h>
#else
#if __has_include(<dispatch/dispatch.h>)
#include <dispatch/dispatch.h>
#define LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD 1
#else
#define LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD 0
#error Right now dynamic_thread_pool_group requires libdispatch to be available on POSIX. It should get auto discovered if installed, which is the default on BSD and Mac OS. Try installing libdispatch-dev if on Linux.
#endif
#endif
#endif
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct global_dynamic_thread_pool_impl
  {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
    using threadh_type = void *;
    using grouph_type = dispatch_group_t;
    static void _gcd_dispatch_callback(void *arg)
    {
      auto *workitem = (dynamic_thread_pool_group::work_item *) arg;
      global_dynamic_thread_pool()._workerthread(workitem, nullptr);
    }
    static void _gcd_timer_callback(void *arg)
    {
      auto *workitem = (dynamic_thread_pool_group::work_item *) arg;
      global_dynamic_thread_pool()._timerthread(workitem, nullptr);
    }
#elif defined(_WIN32)
    using threadh_type = PTP_CALLBACK_INSTANCE;
    using grouph_type = PTP_CALLBACK_ENVIRON;
    static void CALLBACK _win32_worker_thread_callback(threadh_type threadh, PVOID Parameter, PTP_WORK /*unused*/)
    {
      auto *workitem = (dynamic_thread_pool_group::work_item *) Parameter;
      global_dynamic_thread_pool()._workerthread(workitem, threadh);
    }
    static void CALLBACK _win32_timer_thread_callback(threadh_type threadh, PVOID Parameter, PTP_TIMER /*unused*/)
    {
      auto *workitem = (dynamic_thread_pool_group::work_item *) Parameter;
      global_dynamic_thread_pool()._timerthread(workitem, threadh);
    }
#endif

    std::mutex workqueue_lock;
    using _lock_guard = std::unique_lock<std::mutex>;
    struct workqueue_item
    {
      std::unordered_set<dynamic_thread_pool_group_impl *> items;
    };
    std::vector<workqueue_item> workqueue;

    std::mutex io_aware_work_item_handles_lock;
    struct io_aware_work_item_statfs
    {
      size_t refcount{0};
      deadline default_deadline;
      float average_busy{0}, average_queuedepth{0};
      std::chrono::steady_clock::time_point last_updated;
      statfs_t statfs;
    };
    std::unordered_map<fs_handle::unique_id_type, io_aware_work_item_statfs, fs_handle::unique_id_type_hasher> io_aware_work_item_handles;

    global_dynamic_thread_pool_impl()
    {
      workqueue.reserve(4);  // preallocate 4 levels of nesting
    }

    template <class T, class U> static void _append_to_list(T &what, U *v)
    {
      if(what.front == nullptr)
      {
        assert(what.back == nullptr);
        v->_next = v->_prev = nullptr;
        what.front = what.back = v;
      }
      else
      {
        v->_next = nullptr;
        v->_prev = what.back;
        what.back->_next = v;
        what.back = v;
      }
      what.count++;
    }
    template <class T, class U> static void _remove_from_list(T &what, U *v)
    {
      if(v->_prev == nullptr && v->_next == nullptr)
      {
        assert(what.front == v);
        assert(what.back == v);
        what.front = what.back = nullptr;
      }
      else if(v->_prev == nullptr)
      {
        assert(what.front == v);
        v->_next->_prev = nullptr;
        what.front = v->_next;
        v->_next = v->_prev = nullptr;
      }
      else if(v->_next == nullptr)
      {
        assert(what.back == v);
        v->_prev->_next = nullptr;
        what.back = v->_prev;
        v->_next = v->_prev = nullptr;
      }
      else
      {
        v->_next->_prev = v->_prev;
        v->_prev->_next = v->_next;
        v->_next = v->_prev = nullptr;
      }
      what.count--;
    }

    result<void> _prepare_work_item_delay(dynamic_thread_pool_group::work_item *workitem, grouph_type grouph, deadline d)
    {
      if(!d)
      {
        return errc::invalid_argument;
      }
      workitem->_timepoint1 = {};
      workitem->_timepoint2 = {};
      if(workitem->_nextwork == 0 || d.nsecs > 0)
      {
        if(nullptr == workitem->_internaltimerh)
        {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
          (void) grouph;
          workitem->_internaltimerh = (void *) (uintptr_t) -1;
#elif defined(_WIN32)
          workitem->_internaltimerh = CreateThreadpoolTimer(_win32_timer_thread_callback, workitem, grouph);
          if(nullptr == workitem->_internaltimerh)
          {
            return win32_error();
          }
#endif
        }
        if(d.nsecs > 0)
        {
          if(d.steady)
          {
            workitem->_timepoint1 = std::chrono::steady_clock::now() + std::chrono::nanoseconds(d.nsecs);
          }
          else
          {
            workitem->_timepoint2 = d.to_time_point();
          }
        }
      }
      return success();
    }

    inline void _submit_work_item(_lock_guard &g, dynamic_thread_pool_group::work_item *workitem);

    inline result<void> submit(_lock_guard &g, dynamic_thread_pool_group_impl *group, span<dynamic_thread_pool_group::work_item *> work) noexcept;

    inline void _work_item_done(_lock_guard &g, dynamic_thread_pool_group::work_item *i) noexcept;
    inline void _work_item_next(_lock_guard &g, dynamic_thread_pool_group::work_item *i) noexcept;

    inline result<void> stop(_lock_guard &g, dynamic_thread_pool_group_impl *group, result<void> err) noexcept;
    inline result<void> wait(_lock_guard &g, bool reap, const dynamic_thread_pool_group_impl *group, deadline d) noexcept;

    inline void _timerthread(dynamic_thread_pool_group::work_item *workitem, threadh_type selfthreadh);
    inline void _workerthread(dynamic_thread_pool_group::work_item *workitem, threadh_type selfthreadh);
  };
  struct global_dynamic_thread_pool_impl_thread_local_state_t
  {
    dynamic_thread_pool_group::work_item *workitem{nullptr};
    global_dynamic_thread_pool_impl::threadh_type current_callback_instance{nullptr};
    size_t nesting_level{0};
  };
  LLFIO_HEADERS_ONLY_FUNC_SPEC global_dynamic_thread_pool_impl_thread_local_state_t &global_dynamic_thread_pool_thread_local_state() noexcept
  {
    static thread_local global_dynamic_thread_pool_impl_thread_local_state_t tls;
    return tls;
  }

  LLFIO_HEADERS_ONLY_FUNC_SPEC global_dynamic_thread_pool_impl &global_dynamic_thread_pool() noexcept
  {
    static global_dynamic_thread_pool_impl impl;
    return impl;
  }
}  // namespace detail


class dynamic_thread_pool_group_impl final : public dynamic_thread_pool_group
{
  friend struct detail::global_dynamic_thread_pool_impl;

  mutable std::mutex _lock;
  using _lock_guard = detail::global_dynamic_thread_pool_impl::_lock_guard;
  size_t _nesting_level{0};
  struct workitems_t
  {
    size_t count{0};
    dynamic_thread_pool_group::work_item *front{nullptr}, *back{nullptr};
  } _work_items_active, _work_items_done, _work_items_delayed;
  std::atomic<bool> _stopping{false}, _stopped{true}, _completing{false};
  result<void> _abnormal_completion_cause{success()};  // The cause of any abnormal group completion

#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
  dispatch_group_t _grouph;
#elif defined(_WIN32)
  TP_CALLBACK_ENVIRON _callbackenviron;
  PTP_CALLBACK_ENVIRON _grouph{&_callbackenviron};
#endif

public:
  result<void> init()
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    try
    {
      auto &impl = detail::global_dynamic_thread_pool();
      _nesting_level = detail::global_dynamic_thread_pool_thread_local_state().nesting_level;
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      _grouph = dispatch_group_create();
      if(_grouph == nullptr)
      {
        return errc::not_enough_memory;
      }
#elif defined(_WIN32)
      InitializeThreadpoolEnvironment(_grouph);
#endif
      detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.workqueue_lock);
      // Append this group to the global work queue at its nesting level
      if(_nesting_level >= impl.workqueue.size())
      {
        impl.workqueue.resize(_nesting_level + 1);
      }
      impl.workqueue[_nesting_level].items.insert(this);
      return success();
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

  virtual ~dynamic_thread_pool_group_impl()
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    (void) wait();
    auto &impl = detail::global_dynamic_thread_pool();
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
    if(nullptr != _grouph)
    {
      dispatch_release(_grouph);
      _grouph = nullptr;
    }
#elif defined(_WIN32)
    if(nullptr != _grouph)
    {
      DestroyThreadpoolEnvironment(_grouph);
      _grouph = nullptr;
    }
#endif
    detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.workqueue_lock);
    assert(impl.workqueue.size() > _nesting_level);
    impl.workqueue[_nesting_level].items.erase(this);
    while(!impl.workqueue.empty() && impl.workqueue.back().items.empty())
    {
      impl.workqueue.pop_back();
    }
  }

  virtual result<void> submit(span<work_item *> work) noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_stopping.load(std::memory_order_relaxed))
    {
      return errc::operation_canceled;
    }
    _stopped.store(false, std::memory_order_release);
    if(_completing.load(std::memory_order_relaxed))
    {
      for(auto *i : work)
      {
        i->_parent.store(this, std::memory_order_release);
        detail::global_dynamic_thread_pool_impl::_append_to_list(_work_items_delayed, i);
      }
      return success();
    }
    auto &impl = detail::global_dynamic_thread_pool();
    _lock_guard g(_lock);
    if(_work_items_active.count == 0 && _work_items_done.count == 0)
    {
      _abnormal_completion_cause = success();
    }
    OUTCOME_TRY(impl.submit(g, this, work));
    if(_work_items_active.count == 0)
    {
      _stopped.store(true, std::memory_order_release);
    }
    return success();
  }

  virtual result<void> stop() noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_stopped.load(std::memory_order_relaxed))
    {
      return success();
    }
    auto &impl = detail::global_dynamic_thread_pool();
    _lock_guard g(_lock);
    return impl.stop(g, this, errc::operation_canceled);
  }

  virtual bool stopping() const noexcept override { return _stopping.load(std::memory_order_relaxed); }

  virtual bool stopped() const noexcept override { return _stopped.load(std::memory_order_relaxed); }

  virtual result<void> wait(deadline d = {}) const noexcept override
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(_stopped.load(std::memory_order_relaxed))
    {
      return success();
    }
    auto &impl = detail::global_dynamic_thread_pool();
    _lock_guard g(_lock);
    return impl.wait(g, true, this, d);
  }
};

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t dynamic_thread_pool_group::current_nesting_level() noexcept
{
  return detail::global_dynamic_thread_pool_thread_local_state().nesting_level;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC dynamic_thread_pool_group::work_item *dynamic_thread_pool_group::current_work_item() noexcept
{
  return detail::global_dynamic_thread_pool_thread_local_state().workitem;
}

LLFIO_HEADERS_ONLY_FUNC_SPEC result<dynamic_thread_pool_group_ptr> make_dynamic_thread_pool_group() noexcept
{
  try
  {
    auto ret = std::make_unique<dynamic_thread_pool_group_impl>();
    OUTCOME_TRY(ret->init());
    return dynamic_thread_pool_group_ptr(std::move(ret));
  }
  catch(...)
  {
    return error_from_exception();
  }
}

namespace detail
{
  inline void global_dynamic_thread_pool_impl::_submit_work_item(_lock_guard &g, dynamic_thread_pool_group::work_item *workitem)
  {
    (void) g;
    if(workitem->_nextwork != -1)
    {
      // If no work item for now, or there is a delay, schedule a timer
      if(workitem->_nextwork == 0 || workitem->_timepoint1 != std::chrono::steady_clock::time_point() ||
         workitem->_timepoint2 != std::chrono::system_clock::time_point())
      {
        assert(workitem->_internaltimerh != nullptr);
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
        dispatch_time_t when;
        if(workitem->_timepoint1 != std::chrono::steady_clock::time_point())
        {
          auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(workitem->_timepoint1 - std::chrono::steady_clock::now()).count();
          if(duration > 1000000000LL)
          {
            // Because GCD has no way of cancelling timers, nor assigning them to a group,
            // we clamp the timer to 1 second. Then if cancellation is ever done to the group,
            // the worst possible wait is 1 second. _timerthread will reschedule the timer
            // if it gets called short.
            duration = 1000000000LL;
          }
          when = dispatch_time(DISPATCH_TIME_NOW, duration);
        }
        else if(workitem->_timepoint2 != std::chrono::system_clock::time_point())
        {
          deadline d(workitem->_timepoint2);
          auto now = std::chrono::system_clock::now();
          if(workitem->_timepoint2 - now > std::chrono::seconds(1))
          {
            d = now + std::chrono::seconds(1);
          }
          when = dispatch_walltime(&d.utc, 0);
        }
        else
        {
          when = dispatch_time(DISPATCH_TIME_NOW, 1);  // smallest possible non immediate duration from now
        }
        // std::cout << "*** timer " << workitem << std::endl;
        dispatch_after_f(when, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), workitem, _gcd_timer_callback);
#elif defined(_WIN32)
        LARGE_INTEGER li;
        DWORD slop = 1000;
        if(workitem->_timepoint1 != std::chrono::steady_clock::time_point())
        {
          li.QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(workitem->_timepoint1 - std::chrono::steady_clock::now()).count() / 100;
          if(li.QuadPart < 0)
          {
            li.QuadPart = 0;
          }
          if(li.QuadPart / 8 < (int64_t) slop)
          {
            slop = (DWORD)(li.QuadPart / 8);
          }
          li.QuadPart = -li.QuadPart;  // negative is relative
        }
        else if(workitem->_timepoint2 != std::chrono::system_clock::time_point())
        {
          li = windows_nt_kernel::from_timepoint(workitem->_timepoint2);
        }
        else
        {
          li.QuadPart = -1;  // smallest possible non immediate duration from now
        }
        FILETIME ft;
        ft.dwHighDateTime = (DWORD) li.HighPart;
        ft.dwLowDateTime = li.LowPart;
        // std::cout << "*** timer " << workitem << std::endl;
        SetThreadpoolTimer((PTP_TIMER) workitem->_internaltimerh, &ft, 0, slop);
#endif
      }
      else
      {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
        intptr_t priority = DISPATCH_QUEUE_PRIORITY_LOW;
        if(workqueue.size() - workitem->_parent.load(std::memory_order_relaxed)->_nesting_level == 1)
        {
          priority = DISPATCH_QUEUE_PRIORITY_HIGH;
        }
        else if(workqueue.size() - workitem->_parent.load(std::memory_order_relaxed)->_nesting_level == 2)
        {
          priority = DISPATCH_QUEUE_PRIORITY_DEFAULT;
        }
        // std::cout << "*** submit " << workitem << std::endl;
        dispatch_group_async_f(workitem->_parent.load(std::memory_order_relaxed)->_grouph, dispatch_get_global_queue(priority, 0), workitem,
                               _gcd_dispatch_callback);
#elif defined(_WIN32)
        // Set the priority of the group according to distance from the top
        TP_CALLBACK_PRIORITY priority = TP_CALLBACK_PRIORITY_LOW;
        if(workqueue.size() - workitem->_parent.load(std::memory_order_relaxed)->_nesting_level == 1)
        {
          priority = TP_CALLBACK_PRIORITY_HIGH;
        }
        else if(workqueue.size() - workitem->_parent.load(std::memory_order_relaxed)->_nesting_level == 2)
        {
          priority = TP_CALLBACK_PRIORITY_NORMAL;
        }
        SetThreadpoolCallbackPriority(workitem->_parent.load(std::memory_order_relaxed)->_grouph, priority);
        // std::cout << "*** submit " << workitem << std::endl;
        SubmitThreadpoolWork((PTP_WORK) workitem->_internalworkh);
#endif
      }
    }
  }

  inline result<void> global_dynamic_thread_pool_impl::submit(_lock_guard &g, dynamic_thread_pool_group_impl *group,
                                                              span<dynamic_thread_pool_group::work_item *> work) noexcept
  {
    try
    {
      if(work.empty())
      {
        return success();
      }
      for(auto *i : work)
      {
        if(i->_parent.load(std::memory_order_relaxed) != nullptr)
        {
          return errc::address_in_use;
        }
      }
      auto uninit = make_scope_exit([&]() noexcept {
        for(auto *i : work)
        {
          _remove_from_list(group->_work_items_active, i);
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
          i->_internalworkh = nullptr;
          i->_internaltimerh = nullptr;
#elif defined(_WIN32)
          if(nullptr != i->_internaltimerh)
          {
            CloseThreadpoolTimer((PTP_TIMER) i->_internaltimerh);
            i->_internaltimerh = nullptr;
          }
          if(nullptr != i->_internalworkh)
          {
            CloseThreadpoolWork((PTP_WORK) i->_internalworkh);
            i->_internalworkh = nullptr;
          }
#endif
          i->_parent.store(nullptr, std::memory_order_release);
        }
      });
      for(auto *i : work)
      {
        deadline d(std::chrono::seconds(0));
        i->_parent.store(group, std::memory_order_release);
        i->_nextwork = i->next(d);
        if(-1 == i->_nextwork)
        {
          _append_to_list(group->_work_items_done, i);
        }
        else
        {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
          i->_internalworkh = (void *) (uintptr_t) -1;
#elif defined(_WIN32)
          i->_internalworkh = CreateThreadpoolWork(_win32_worker_thread_callback, i, group->_grouph);
          if(nullptr == i->_internalworkh)
          {
            return win32_error();
          }
#endif
          OUTCOME_TRY(_prepare_work_item_delay(i, group->_grouph, d));
          _append_to_list(group->_work_items_active, i);
        }
      }
      uninit.release();
      {
        for(auto *i : work)
        {
          _submit_work_item(g, i);
        }
      }
      return success();
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

  inline void global_dynamic_thread_pool_impl::_work_item_done(_lock_guard &g, dynamic_thread_pool_group::work_item *i) noexcept
  {
    (void) g;
    // std::cout << "*** _work_item_done " << i << std::endl;
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
    i->_internaltimerh = nullptr;
    i->_internalworkh = nullptr;
#elif defined(_WIN32)
    if(i->_internalworkh_inuse > 0)
    {
      i->_internalworkh_inuse = 2;
    }
    else
    {
      if(i->_internaltimerh != nullptr)
      {
        CloseThreadpoolTimer((PTP_TIMER) i->_internaltimerh);
        i->_internaltimerh = nullptr;
      }
      if(i->_internalworkh != nullptr)
      {
        CloseThreadpoolWork((PTP_WORK) i->_internalworkh);
        i->_internalworkh = nullptr;
      }
    }
#endif
    _remove_from_list(i->_parent.load(std::memory_order_relaxed)->_work_items_active, i);
    _append_to_list(i->_parent.load(std::memory_order_relaxed)->_work_items_done, i);
    if(i->_parent.load(std::memory_order_relaxed)->_work_items_active.count == 0)
    {
      auto *parent = i->_parent.load(std::memory_order_relaxed);
      i = nullptr;
      auto *v = parent->_work_items_done.front, *n = v;
      for(; v != nullptr; v = n)
      {
        v->_parent.store(nullptr, std::memory_order_release);
        v->_nextwork = -1;
        n = v->_next;
      }
      n = v = parent->_work_items_done.front;
      parent->_work_items_done.front = parent->_work_items_done.back = nullptr;
      parent->_work_items_done.count = 0;
      parent->_stopping.store(false, std::memory_order_release);
      parent->_stopped.store(true, std::memory_order_release);
      parent->_completing.store(true, std::memory_order_release);
      for(; v != nullptr; v = n)
      {
        n = v->_next;
        v->group_complete(parent->_abnormal_completion_cause);
      }
      parent->_completing.store(false, std::memory_order_release);
      // Did a least one group_complete() submit more work to myself?
      while(parent->_work_items_delayed.count > 0)
      {
        i = parent->_work_items_delayed.front;
        _remove_from_list(parent->_work_items_delayed, i);
        auto r = submit(g, parent, {&i, 1});
        if(!r)
        {
          parent->_work_items_delayed = {};
          (void) stop(g, parent, std::move(r));
          break;
        }
      }
    }
  }
  inline void global_dynamic_thread_pool_impl::_work_item_next(_lock_guard &g, dynamic_thread_pool_group::work_item *workitem) noexcept
  {
    assert(workitem->_nextwork != -1);
    if(workitem->_nextwork == 0)
    {
      deadline d(std::chrono::seconds(0));
      workitem->_nextwork = workitem->next(d);
      auto r = _prepare_work_item_delay(workitem, workitem->_parent.load(std::memory_order_relaxed)->_grouph, d);
      if(!r)
      {
        (void) stop(g, workitem->_parent.load(std::memory_order_relaxed), std::move(r));
        _work_item_done(g, workitem);
        return;
      }
    }
    if(-1 == workitem->_nextwork)
    {
      _work_item_done(g, workitem);
      return;
    }
    _submit_work_item(g, workitem);
  }

  inline result<void> global_dynamic_thread_pool_impl::stop(_lock_guard &g, dynamic_thread_pool_group_impl *group, result<void> err) noexcept
  {
    (void) g;
    if(group->_abnormal_completion_cause)
    {
      group->_abnormal_completion_cause = std::move(err);
    }
    group->_stopping.store(true, std::memory_order_release);
    return success();
  }

  inline result<void> global_dynamic_thread_pool_impl::wait(_lock_guard &g, bool reap, const dynamic_thread_pool_group_impl *group, deadline d) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    if(!d || d.nsecs > 0)
    {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      while(group->_work_items_active.count > 0)
      {
        LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
        dispatch_time_t timeout = DISPATCH_TIME_FOREVER;
        if(d)
        {
          std::chrono::nanoseconds duration;
          LLFIO_DEADLINE_TO_PARTIAL_TIMEOUT(duration, d);
          timeout = dispatch_time(DISPATCH_TIME_NOW, duration.count());
        }
        g.unlock();
        dispatch_group_wait(group->_grouph, timeout);
        g.lock();
        // if(1 == group->_work_items_active.count)
        //{
        //  std::cout << "*** wait item remaining is " << group->_work_items_active.front << std::endl;
        //  std::this_thread::sleep_for(std::chrono::seconds(1));
        //}
      }
#elif defined(_WIN32)
      auto &tls = detail::global_dynamic_thread_pool_thread_local_state();
      if(tls.current_callback_instance != nullptr)
      {
        // I am being called from within a thread worker. Tell
        // the thread pool that I am not going to exit promptly.
        CallbackMayRunLong(tls.current_callback_instance);
      }
      // Is this a cancellation?
      if(group->_stopping.load(std::memory_order_relaxed))
      {
        while(group->_work_items_active.count > 0)
        {
          auto *i = group->_work_items_active.front;
          if(nullptr != i->_internalworkh)
          {
            if(0 == i->_internalworkh_inuse)
            {
              i->_internalworkh_inuse = 1;
            }
            g.unlock();
            WaitForThreadpoolWorkCallbacks((PTP_WORK) i->_internalworkh, true);
            g.lock();
            if(i->_internalworkh_inuse == 2)
            {
              if(nullptr != i->_internalworkh)
              {
                CloseThreadpoolWork((PTP_WORK) i->_internalworkh);
                i->_internalworkh = nullptr;
              }
              if(nullptr != i->_internaltimerh)
              {
                CloseThreadpoolTimer((PTP_TIMER) i->_internaltimerh);
                i->_internaltimerh = nullptr;
              }
            }
            i->_internalworkh_inuse = 0;
          }
          if(nullptr != i->_internaltimerh)
          {
            if(0 == i->_internalworkh_inuse)
            {
              i->_internalworkh_inuse = 1;
            }
            g.unlock();
            WaitForThreadpoolTimerCallbacks((PTP_TIMER) i->_internaltimerh, true);
            g.lock();
            if(i->_internalworkh_inuse == 2)
            {
              if(nullptr != i->_internalworkh)
              {
                CloseThreadpoolWork((PTP_WORK) i->_internalworkh);
                i->_internalworkh = nullptr;
              }
              if(nullptr != i->_internaltimerh)
              {
                CloseThreadpoolTimer((PTP_TIMER) i->_internaltimerh);
                i->_internaltimerh = nullptr;
              }
            }
            i->_internalworkh_inuse = 0;
          }
          if(group->_work_items_active.count > 0 && group->_work_items_active.front == i)
          {
            // This item got cancelled before it started
            _work_item_done(g, group->_work_items_active.front);
          }
        }
        assert(!group->_stopping.load(std::memory_order_relaxed));
      }
      else if(!d)
      {
        while(group->_work_items_active.count > 0)
        {
          auto *i = group->_work_items_active.front;
          if(nullptr != i->_internalworkh)
          {
            if(0 == i->_internalworkh_inuse)
            {
              i->_internalworkh_inuse = 1;
            }
            g.unlock();
            WaitForThreadpoolWorkCallbacks((PTP_WORK) i->_internalworkh, false);
            g.lock();
            if(i->_internalworkh_inuse == 2)
            {
              if(nullptr != i->_internalworkh)
              {
                CloseThreadpoolWork((PTP_WORK) i->_internalworkh);
                i->_internalworkh = nullptr;
              }
              if(nullptr != i->_internaltimerh)
              {
                CloseThreadpoolTimer((PTP_TIMER) i->_internaltimerh);
                i->_internaltimerh = nullptr;
              }
            }
            i->_internalworkh_inuse = 0;
          }
          if(nullptr != i->_internaltimerh)
          {
            if(0 == i->_internalworkh_inuse)
            {
              i->_internalworkh_inuse = 1;
            }
            g.unlock();
            WaitForThreadpoolTimerCallbacks((PTP_TIMER) i->_internaltimerh, false);
            g.lock();
            if(i->_internalworkh_inuse == 2)
            {
              if(nullptr != i->_internalworkh)
              {
                CloseThreadpoolWork((PTP_WORK) i->_internalworkh);
                i->_internalworkh = nullptr;
              }
              if(nullptr != i->_internaltimerh)
              {
                CloseThreadpoolTimer((PTP_TIMER) i->_internaltimerh);
                i->_internaltimerh = nullptr;
              }
            }
            i->_internalworkh_inuse = 0;
          }
        }
      }
      else
      {
        while(group->_work_items_active.count > 0)
        {
          LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
          g.unlock();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          g.lock();
        }
      }
#endif
    }
    if(group->_work_items_active.count > 0)
    {
      return errc::timed_out;
    }
    if(reap)
    {
      return std::move(group->_abnormal_completion_cause);
    }
    return success();
  }

  inline void global_dynamic_thread_pool_impl::_timerthread(dynamic_thread_pool_group::work_item *workitem, threadh_type /*unused*/)
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    assert(workitem->_nextwork != -1);
    _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);
    // std::cout << "*** _timerthread " << workitem << std::endl;
    if(workitem->_parent.load(std::memory_order_relaxed)->_stopping.load(std::memory_order_relaxed))
    {
      _work_item_done(g, workitem);
      return;
    }
    if(workitem->_timepoint1 != std::chrono::steady_clock::time_point())
    {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      auto now = std::chrono::steady_clock::now();
      if(workitem->_timepoint1 - now > std::chrono::seconds(0))
      {
        // Timer fired short, so schedule it again
        _submit_work_item(g, workitem);
        return;
      }
#endif
      workitem->_timepoint1 = {};
    }
    if(workitem->_timepoint2 != std::chrono::system_clock::time_point())
    {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      auto now = std::chrono::system_clock::now();
      if(workitem->_timepoint2 - now > std::chrono::seconds(0))
      {
        // Timer fired short, so schedule it again
        _submit_work_item(g, workitem);
        return;
      }
#endif
      workitem->_timepoint2 = {};
    }
    _work_item_next(g, workitem);
  }

  // Worker thread entry point
  inline void global_dynamic_thread_pool_impl::_workerthread(dynamic_thread_pool_group::work_item *workitem, threadh_type selfthreadh)
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    //{
    //  _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);
    //  std::cout << "*** _workerthread " << workitem << " begins with work " << workitem->_nextwork << std::endl;
    //}
    assert(workitem->_nextwork != -1);
    assert(workitem->_nextwork != 0);
    if(workitem->_parent.load(std::memory_order_relaxed)->_stopping.load(std::memory_order_relaxed))
    {
      _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);
      _work_item_done(g, workitem);
      return;
    }
    auto &tls = detail::global_dynamic_thread_pool_thread_local_state();
    auto old_thread_local_state = tls;
    tls.workitem = workitem;
    tls.current_callback_instance = selfthreadh;
    tls.nesting_level = workitem->_parent.load(std::memory_order_relaxed)->_nesting_level + 1;
    auto r = (*workitem)(workitem->_nextwork);
    workitem->_nextwork = 0;  // call next() next time
    tls = old_thread_local_state;
    _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);
    // std::cout << "*** _workerthread " << workitem << " ends with work " << workitem->_nextwork << std::endl;
    if(!r)
    {
      (void) stop(g, workitem->_parent.load(std::memory_order_relaxed), std::move(r));
      _work_item_done(g, workitem);
      workitem = nullptr;
    }
    else if(workitem->_parent.load(std::memory_order_relaxed)->_stopping.load(std::memory_order_relaxed))
    {
      _work_item_done(g, workitem);
    }
    else
    {
      _work_item_next(g, workitem);
    }
  }
}  // namespace detail


/****************************************** io_aware_work_item *********************************************/

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC dynamic_thread_pool_group::io_aware_work_item::io_aware_work_item(span<io_handle_awareness> hs)
    : _handles([](span<io_handle_awareness> hs) -> span<io_handle_awareness> {
      float all = 0;
      for(auto &i : hs)
      {
        all += i.reads + i.writes + i.barriers;
      }
      for(auto &i : hs)
      {
        if(all == 0.0f)
        {
          i.reads = i.writes = 0.5f;
          i.barriers = 0.0f;
        }
        else
        {
          i.reads /= all;
          i.writes /= all;
          i.barriers /= all;
        }
      }
      auto &impl = detail::global_dynamic_thread_pool();
      detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.io_aware_work_item_handles_lock);
      for(auto &h : hs)
      {
        if(!h.h->is_seekable())
        {
          throw std::runtime_error("Supplied handle is not seekable");
        }
        auto *fh = static_cast<file_handle *>(h.h);
        auto unique_id = fh->unique_id();
        auto it = impl.io_aware_work_item_handles.find(unique_id);
        if(it == impl.io_aware_work_item_handles.end())
        {
          it = impl.io_aware_work_item_handles.emplace(unique_id, detail::global_dynamic_thread_pool_impl::io_aware_work_item_statfs{}).first;
          auto r = it->second.statfs.fill(*fh, statfs_t::want::iosinprogress | statfs_t::want::iosbusytime);
          if(!r || it->second.statfs.f_iosinprogress == (uint32_t) -1)
          {
            impl.io_aware_work_item_handles.erase(it);
            if(!r)
            {
              r.value();
            }
            throw std::runtime_error("statfs::f_iosinprogress unavailable for supplied handle");
          }
          it->second.last_updated = std::chrono::steady_clock::now();
        }
        it->second.refcount++;
        h._internal = &*it;
      }
      return hs;
    }(hs))
{
  LLFIO_LOG_FUNCTION_CALL(this);
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC dynamic_thread_pool_group::io_aware_work_item::~io_aware_work_item()
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto &impl = detail::global_dynamic_thread_pool();
  detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.io_aware_work_item_handles_lock);
  using value_type = decltype(impl.io_aware_work_item_handles)::value_type;
  for(auto &h : _handles)
  {
    auto *i = (value_type *) h._internal;
    if(0 == --i->second.refcount)
    {
      impl.io_aware_work_item_handles.erase(i->first);
    }
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC intptr_t dynamic_thread_pool_group::io_aware_work_item::next(deadline &d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  {
    auto &impl = detail::global_dynamic_thread_pool();
    auto now = std::chrono::steady_clock::now();
    detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.io_aware_work_item_handles_lock);
    using value_type = decltype(impl.io_aware_work_item_handles)::value_type;
    for(auto &h : _handles)
    {
      auto *i = (value_type *) h._internal;
      if(std::chrono::duration_cast<std::chrono::milliseconds>(now - i->second.last_updated) >= std::chrono::milliseconds(100))
      {
        // auto old_iosinprogress = i->second.statfs.f_iosinprogress;
        auto elapsed = now - i->second.last_updated;
        (void) i->second.statfs.fill(*h.h, statfs_t::want::iosinprogress | statfs_t::want::iosbusytime);
        i->second.last_updated = now;

        if(elapsed > std::chrono::seconds(5))
        {
          i->second.average_busy = i->second.statfs.f_iosbusytime;
          i->second.average_queuedepth = (float) i->second.statfs.f_iosinprogress;
        }
        else
        {
          i->second.average_busy = (i->second.average_busy * 0.9f) + (i->second.statfs.f_iosbusytime * 0.1f);
          i->second.average_queuedepth = (i->second.average_queuedepth * 0.9f) + (i->second.statfs.f_iosinprogress * 0.1f);
        }
        if(i->second.average_busy < this->max_iosbusytime && i->second.average_queuedepth < this->min_iosinprogress)
        {
          i->second.default_deadline = std::chrono::seconds(0);  // remove pacing
        }
        else if(i->second.average_queuedepth > this->max_iosinprogress)
        {
          if(0 == i->second.default_deadline.nsecs)
          {
            i->second.default_deadline = std::chrono::milliseconds(1);  // start with 1ms, it'll reduce from there if needed
          }
          else if((i->second.default_deadline.nsecs >> 4) > 0)
          {
            i->second.default_deadline.nsecs += i->second.default_deadline.nsecs >> 4;
          }
          else
          {
            i->second.default_deadline.nsecs++;
          }
        }
        else if(i->second.average_queuedepth < this->min_iosinprogress)
        {
          if(i->second.default_deadline.nsecs > (i->second.default_deadline.nsecs >> 4) && (i->second.default_deadline.nsecs >> 4) > 0)
          {
            i->second.default_deadline.nsecs -= i->second.default_deadline.nsecs >> 4;
          }
          else if(i->second.default_deadline.nsecs > 1)
          {
            i->second.default_deadline.nsecs--;
          }
        }
      }
      if(d.nsecs < i->second.default_deadline.nsecs)
      {
        d = i->second.default_deadline;
      }
    }
  }
  return io_aware_next(d);
}


LLFIO_V2_NAMESPACE_END
