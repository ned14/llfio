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

#include <atomic>
#include <mutex>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#include "windows/import.hpp"
#include <threadpoolapiset.h>
#else
#include <pthread>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct global_dynamic_thread_pool_impl
  {
#ifdef _WIN32
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
#else
#endif

    std::mutex workqueue_lock;
    using _lock_guard = std::unique_lock<std::mutex>;
    struct workqueue_item
    {
      std::unordered_set<dynamic_thread_pool_group_impl *> items;
    };
    std::vector<workqueue_item> workqueue;

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
      if(workitem->_nextwork == 0 || d.nsecs > 0)
      {
        if(nullptr == workitem->_internaltimerh)
        {
          workitem->_internaltimerh = CreateThreadpoolTimer(_win32_timer_thread_callback, workitem, grouph);
          if(nullptr == workitem->_internaltimerh)
          {
            return win32_error();
          }
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

#ifdef _WIN32
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
#ifdef _WIN32
      InitializeThreadpoolEnvironment(_grouph);
#else
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
#ifdef _WIN32
    if(nullptr != _grouph)
    {
      DestroyThreadpoolEnvironment(_grouph);
      _grouph = nullptr;
    }
#else
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
        i->_parent = this;
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
    if(workitem->_nextwork != -1 && !workitem->_parent->_stopping.load(std::memory_order_relaxed))
    {
      // If no work item for now, or there is a delay, schedule a timer
      if(workitem->_nextwork == 0 || workitem->_timepoint1 != std::chrono::steady_clock::time_point() ||
         workitem->_timepoint2 != std::chrono::system_clock::time_point())
      {
        assert(workitem->_internaltimerh != nullptr);
#ifdef _WIN32
        LARGE_INTEGER li;
        if(workitem->_timepoint1 != std::chrono::steady_clock::time_point())
        {
          li.QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(workitem->_timepoint1 - std::chrono::steady_clock::now()).count() / 100;
          if(li.QuadPart < 0)
          {
            li.QuadPart = 0;
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
        SetThreadpoolTimer((PTP_TIMER) workitem->_internaltimerh, &ft, 0, 1000);
#else
#endif
      }
      else
      {
#ifdef _WIN32
        // Set the priority of the group according to distance from the top
        TP_CALLBACK_PRIORITY priority = TP_CALLBACK_PRIORITY_LOW;
        if(workqueue.size() - workitem->_parent->_nesting_level == 1)
        {
          priority = TP_CALLBACK_PRIORITY_HIGH;
        }
        else if(workqueue.size() - workitem->_parent->_nesting_level == 2)
        {
          priority = TP_CALLBACK_PRIORITY_NORMAL;
        }
        SetThreadpoolCallbackPriority(workitem->_parent->_grouph, priority);
        // std::cout << "*** submit " << workitem << std::endl;
        SubmitThreadpoolWork((PTP_WORK) workitem->_internalworkh);
#else
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
        if(i->_parent != nullptr)
        {
          return errc::address_in_use;
        }
      }
      auto uninit = make_scope_exit([&]() noexcept {
        for(auto *i : work)
        {
          _remove_from_list(group->_work_items_active, i);
#ifdef _WIN32
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
#else
#endif
          i->_parent = nullptr;
        }
      });
      for(auto *i : work)
      {
        deadline d(std::chrono::seconds(0));
        i->_parent = group;
        i->_nextwork = i->next(d);
        if(-1 == i->_nextwork)
        {
          _append_to_list(group->_work_items_done, i);
        }
        else
        {
#ifdef _WIN32
          i->_internalworkh = CreateThreadpoolWork(_win32_worker_thread_callback, i, group->_grouph);
          if(nullptr == i->_internalworkh)
          {
            return win32_error();
          }
#else
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
#ifdef _WIN32
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
#else
#endif
    _remove_from_list(i->_parent->_work_items_active, i);
    _append_to_list(i->_parent->_work_items_done, i);
    if(i->_parent->_work_items_active.count == 0)
    {
      auto *parent = i->_parent;
      i = nullptr;
      auto *v = parent->_work_items_done.front, *n = v;
      for(; v != nullptr; v = n)
      {
        v->_parent = nullptr;
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
      auto r = _prepare_work_item_delay(workitem, workitem->_parent->_grouph, d);
      if(!r)
      {
        (void) stop(g, workitem->_parent, std::move(r));
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
      auto &tls = detail::global_dynamic_thread_pool_thread_local_state();
#ifdef _WIN32
      if(tls.current_callback_instance != nullptr)
      {
        // I am being called from within a thread worker. Tell
        // the thread pool that I am not going to exit promptly.
        CallbackMayRunLong(tls.current_callback_instance);
      }
#endif
      // Is this a cancellation?
      if(group->_stopping.load(std::memory_order_relaxed))
      {
        while(group->_work_items_active.count > 0)
        {
#ifdef _WIN32
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
#else
#endif
        }
        assert(!group->_stopping.load(std::memory_order_relaxed));
      }
      else if(!d)
      {
        while(group->_work_items_active.count > 0)
        {
#ifdef _WIN32
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
#else
#endif
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
    // std::cout << "*** _timerthread " << workitem << std::endl;
    _lock_guard g(workitem->_parent->_lock);
    _work_item_next(g, workitem);
  }

  // Worker thread entry point
  inline void global_dynamic_thread_pool_impl::_workerthread(dynamic_thread_pool_group::work_item *workitem, threadh_type selfthreadh)
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    // std::cout << "*** _workerthread " << workitem << " with work " << workitem->_nextwork << std::endl;
    assert(workitem->_nextwork != -1);
    assert(workitem->_nextwork != 0);
    if(workitem->_parent->_stopped.load(std::memory_order_relaxed))
    {
      _lock_guard g(workitem->_parent->_lock);
      _work_item_done(g, workitem);
      return;
    }
    auto &tls = detail::global_dynamic_thread_pool_thread_local_state();
    auto old_thread_local_state = tls;
    tls.workitem = workitem;
    tls.current_callback_instance = selfthreadh;
    tls.nesting_level = workitem->_parent->_nesting_level + 1;
    auto r = (*workitem)(workitem->_nextwork);
    workitem->_nextwork = 0;  // call next() next time
    tls = old_thread_local_state;
    _lock_guard g(workitem->_parent->_lock);
    if(!r)
    {
      (void) stop(g, workitem->_parent, std::move(r));
      _work_item_done(g, workitem);
      workitem = nullptr;
    }
    else
    {
      _work_item_next(g, workitem);
    }
  }
}  // namespace detail

LLFIO_V2_NAMESPACE_END
