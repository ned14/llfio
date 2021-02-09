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
#endif
#endif
#endif
#endif
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
#if !defined(__linux__)
#error dynamic_thread_pool_group requires Grand Central Dispatch (libdispatch) on non-Linux POSIX.
#endif
#include <dirent.h> /* Defines DT_* constants */
#include <sys/syscall.h>

#include <condition_variable>
#include <thread>
#endif

#define LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING 0

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  struct global_dynamic_thread_pool_impl
  {
    std::mutex workqueue_lock;
    using _lock_guard = std::unique_lock<std::mutex>;
    struct workqueue_item
    {
      std::unordered_set<dynamic_thread_pool_group_impl *> items;
      std::unordered_set<dynamic_thread_pool_group_impl *>::iterator currentgroup{items.begin()};
      dynamic_thread_pool_group_impl *_currentgroup{nullptr};
    };
    std::vector<workqueue_item> workqueue;
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
#else
    using threadh_type = void *;
    using grouph_type = void *;
    struct thread_t
    {
      thread_t *_prev{nullptr}, *_next{nullptr};
      std::thread thread;
      std::condition_variable cond;
      std::chrono::steady_clock::time_point last_did_work;
      int state{0};  // <0 = dead, 0 = sleeping/please die, 1 = busy
    };
    struct threads_t
    {
      size_t count{0};
      thread_t *front{nullptr}, *back{nullptr};
    } threadpool_active, threadpool_sleeping;
    std::atomic<size_t> total_submitted_workitems{0}, threadpool_threads{0}, threadpool_sleeping_count{0};
    std::atomic<uint32_t> ms_sleep_for_more_work{60000};

    std::mutex threadmetrics_lock;
    struct threadmetrics_threadid
    {
      char text[12];  // enough for a UINT32_MAX in decimal
      constexpr threadmetrics_threadid()
          : text{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
      {
      }
      threadmetrics_threadid(string_view sv)
      {
        memset(text, '0', sizeof(text));
        assert(sv.size() <= sizeof(text));
        if(sv.size() > sizeof(text))
        {
          abort();
        }
        memcpy(text + sizeof(text) - sv.size(), sv.data(), sv.size());
      }
      int compare(const threadmetrics_threadid &o) const noexcept { return memcmp(text, o.text, sizeof(text)); }
      bool operator<(const threadmetrics_threadid &o) const noexcept { return compare(o) < 0; }
      bool operator==(const threadmetrics_threadid &o) const noexcept { return compare(o) == 0; }
    };
    struct threadmetrics_item
    {
      threadmetrics_item *_prev{nullptr}, *_next{nullptr};
      std::chrono::steady_clock::time_point last_updated, blocked_since;  // latter set if thread seen no time
      threadmetrics_threadid threadid;
      uint32_t diskfaults{(uint32_t) -1}, utime{(uint32_t) -1}, stime{(uint32_t) -1};  // culmulative ticks spent in user and system for this thread

      explicit threadmetrics_item(threadmetrics_threadid v)
          : threadid(v)
      {
      }
      string_view threadid_name() const noexcept { return string_view(threadid.text, sizeof(threadid.text)); }
    };
    struct threadmetrics_t
    {
      size_t count{0};
      threadmetrics_item *front{nullptr}, *back{nullptr};
      uint32_t blocked{0}, running{0};
    } threadmetrics_queue;                                   // items at front are least recently updated
    std::vector<threadmetrics_item *> threadmetrics_sorted;  // sorted by threadid
    std::chrono::steady_clock::time_point threadmetrics_last_updated;
    std::atomic<bool> update_threadmetrics_reentrancy{false};
#ifdef __linux__
    std::mutex proc_self_task_fd_lock;
    int proc_self_task_fd{-1};
#endif
#endif

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
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
      populate_threadmetrics(std::chrono::steady_clock::now());
#endif
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
    template <class T, class U> static void _prepend_to_list(T &what, U *v)
    {
      if(what.front == nullptr)
      {
        assert(what.back == nullptr);
        v->_next = v->_prev = nullptr;
        what.front = what.back = v;
      }
      else
      {
        v->_prev = nullptr;
        v->_next = what.front;
        what.front->_prev = v;
        what.front = v;
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

#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
    inline void _execute_work(thread_t *self);

    void _add_thread(_lock_guard & /*unused*/)
    {
      thread_t *p = nullptr;
      try
      {
        p = new thread_t;
        _append_to_list(threadpool_active, p);
        p->thread = std::thread([this, p] { _execute_work(p); });
      }
      catch(...)
      {
        if(p != nullptr)
        {
          _remove_from_list(threadpool_active, p);
        }
        // drop failure
      }
    }

    bool _remove_thread(_lock_guard &g, threads_t &which)
    {
      if(which.count == 0)
      {
        return false;
      }
      // Threads which went to sleep the longest ago are at the front
      auto *t = which.front;
      if(t->state < 0)
      {
        // He's already exiting
        return false;
      }
      assert(t->state == 0);
      t->state--;
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
      std::cout << "*** DTP " << t << " is told to quit" << std::endl;
#endif
      do
      {
        g.unlock();
        t->cond.notify_one();
        g.lock();
      } while(t->state >= -1);
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
      std::cout << "*** DTP " << t << " has quit, deleting" << std::endl;
#endif
      _remove_from_list(threadpool_active, t);
      t->thread.join();
      delete t;
      return true;
    }

    ~global_dynamic_thread_pool_impl()
    {
      {
        _lock_guard g(workqueue_lock);  // lock global
        while(threadpool_active.count > 0 || threadpool_sleeping.count > 0)
        {
          while(threadpool_sleeping.count > 0)
          {
            auto removed = _remove_thread(g, threadpool_sleeping);
            assert(removed);
            (void) removed;
          }
          if(threadpool_active.count > 0)
          {
            auto removed = _remove_thread(g, threadpool_active);
            assert(removed);
            (void) removed;
          }
        }
      }
      _lock_guard g(threadmetrics_lock);
      for(auto *p : threadmetrics_sorted)
      {
        delete p;
      }
      threadmetrics_sorted.clear();
      threadmetrics_queue = {};
#ifdef __linux__
      if(proc_self_task_fd > 0)
      {
        ::close(proc_self_task_fd);
        proc_self_task_fd = -1;
      }
#endif
    }

#ifdef __linux__
    bool update_threadmetrics(_lock_guard &&g, std::chrono::steady_clock::time_point now, threadmetrics_item *new_items)
    {
      auto update_item = [&](threadmetrics_item *item) {
        char path[64] = "/proc/self/task/", *pend = path + 16, *tend = item->threadid.text;
        while(*tend == '0' && (tend - item->threadid.text) < (ssize_t) sizeof(item->threadid.text))
        {
          ++tend;
        }
        while((tend - item->threadid.text) < (ssize_t) sizeof(item->threadid.text))
        {
          *pend++ = *tend++;
        }
        memcpy(pend, "/stat", 6);
        int fd = ::open(path, O_RDONLY);
        if(-1 == fd)
        {
          // Thread may have exited since we last populated
          if(item->blocked_since == std::chrono::steady_clock::time_point())
          {
            threadmetrics_queue.running--;
            threadmetrics_queue.blocked++;
          }
          item->blocked_since = now;
          item->last_updated = now;
          return;
        }
        char buffer[1024];
        auto bytesread = ::read(fd, buffer, sizeof(buffer));
        ::close(fd);
        buffer[std::max((size_t) bytesread, sizeof(buffer) - 1)] = 0;
        char state = 0;
        unsigned long majflt = 0, utime = 0, stime = 0;
        sscanf(buffer, "%*d %*s %c %*d %*d %*d %*d %*d %*u %*u %*u %lu %*u %lu %lu", &state, &majflt, &utime, &stime);
        if(item->utime != (uint32_t) -1 || item->stime != (uint32_t) -1)
        {
          if(item->utime == (uint32_t) utime && item->stime == (uint32_t) stime && state != 'R')
          {
            // This thread made no progress since last time
            if(item->blocked_since == std::chrono::steady_clock::time_point())
            {
              threadmetrics_queue.running--;
              threadmetrics_queue.blocked++;
              item->blocked_since = now;
            }
          }
          else
          {
            if(item->blocked_since != std::chrono::steady_clock::time_point())
            {
              threadmetrics_queue.running++;
              threadmetrics_queue.blocked--;
              item->blocked_since = std::chrono::steady_clock::time_point();
            }
          }
        }
        // std::cout << "Threadmetrics " << path << " " << state << " " << majflt << " " << utime << " " << stime << ". Previously " << item->diskfaults << " "
        //          << item->utime << " " << item->stime << std::endl;
        item->diskfaults = (uint32_t) majflt;
        item->utime = (uint32_t) utime;
        item->stime = (uint32_t) stime;
        item->last_updated = now;
      };
      if(new_items != nullptr)
      {
        for(; new_items != nullptr; new_items = new_items->_next)
        {
          update_item(new_items);
        }
        return false;
      }
      if(threadmetrics_queue.count == 0)
      {
        return false;
      }
      size_t updated = 0;
      while(now - threadmetrics_queue.front->last_updated >= std::chrono::milliseconds(100) && updated++ < 10)
      {
        auto *p = threadmetrics_queue.front;
        update_item(p);
        _remove_from_list(threadmetrics_queue, p);
        _append_to_list(threadmetrics_queue, p);
      }
      if(updated > 0)
      {
        static const auto min_hardware_concurrency = std::thread::hardware_concurrency();
        static const auto max_hardware_concurrency = min_hardware_concurrency + (min_hardware_concurrency >> 1);
        auto toadd = std::max((ssize_t) 0, std::min((ssize_t) min_hardware_concurrency - (ssize_t) threadmetrics_queue.running,
                                                    (ssize_t) total_submitted_workitems.load(std::memory_order_relaxed) -
                                                    (ssize_t) threadpool_threads.load(std::memory_order_relaxed)));
        auto toremove = std::max((ssize_t) 0, (ssize_t) threadmetrics_queue.running - (ssize_t) max_hardware_concurrency);
        // std::cout << "Threadmetrics toadd = " << (toadd - (ssize_t) threadpool_sleeping.count) << " toremove = " << toremove
        //          << " running = " << threadmetrics_queue.running << " blocked = " << threadmetrics_queue.blocked << " total = " << threadmetrics_queue.count
        //          << ". Actual active = " << threadpool_active.count << " sleeping = " << threadpool_sleeping.count
        //          << ". Current working threads = " << threadpool_threads.load(std::memory_order_relaxed)
        //          << ". Current submitted work items = " << total_submitted_workitems.load(std::memory_order_relaxed) << std::endl;
        if(toadd > 0 || toremove > 0)
        {
          if(update_threadmetrics_reentrancy.exchange(true, std::memory_order_relaxed))
          {
            return false;
          }
          auto unupdate_threadmetrics_reentrancy =
          make_scope_exit([this]() noexcept { update_threadmetrics_reentrancy.store(false, std::memory_order_relaxed); });
          g.unlock();
          _lock_guard gg(workqueue_lock);
          toadd -= (ssize_t) threadpool_sleeping.count;
          std::cout << "total active = " << threadpool_active.count << " total idle = " << threadpool_sleeping.count << " toadd = " << toadd
                    << " toremove = " << toremove << std::endl;
          for(; toadd > 0; toadd--)
          {
            _add_thread(gg);
          }
          for(; toremove > 0 && threadpool_sleeping.count > 0; toremove--)
          {
            if(!_remove_thread(gg, threadpool_sleeping))
            {
              break;
            }
          }
          if(toremove > 0 && threadpool_active.count > 1)
          {
            // Kill myself, but not if I'm the final thread who needs to run timers
            return true;
          }
          return false;
        }
      }
      return false;
    }
    // Returns true if calling thread is to exit
    bool populate_threadmetrics(std::chrono::steady_clock::time_point now)
    {
      static thread_local std::vector<char> kernelbuffer(1024);
      static thread_local std::vector<threadmetrics_threadid> threadidsbuffer(1024 / sizeof(dirent));
      using getdents64_t = int (*)(int, char *, unsigned int);
      static auto getdents = static_cast<getdents64_t>([](int fd, char *buf, unsigned count) -> int { return syscall(SYS_getdents64, fd, buf, count); });
      using dirent = dirent64;
      size_t bytes = 0;
      {
        _lock_guard g(threadmetrics_lock);
        if(now - threadmetrics_last_updated < std::chrono::milliseconds(100) &&
           threadmetrics_queue.running + threadmetrics_queue.blocked >= threadpool_threads.load(std::memory_order_relaxed))
        {
          return update_threadmetrics(std::move(g), now, nullptr);
        }
        threadmetrics_last_updated = now;
        if(proc_self_task_fd < 0)
        {
          proc_self_task_fd = ::open("/proc/self/task", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
          if(proc_self_task_fd < 0)
          {
            posix_error().throw_exception();
          }
        }
      }
      {
        _lock_guard g(proc_self_task_fd_lock);
        /* It turns out that /proc/self/task is quite racy in the Linux kernel, so keep
        looping this until it stops telling obvious lies.
        */
        for(auto done = false; !done;)
        {
          if(-1 == ::lseek64(proc_self_task_fd, 0, SEEK_SET))
          {
            posix_error().throw_exception();
          }
          auto _bytes = getdents(proc_self_task_fd, kernelbuffer.data(), kernelbuffer.size());
          // std::cout << "getdents(" << (kernelbuffer.size()-bytes) << ") returns " << _bytes << std::endl;
          if(_bytes < 0 && errno != EINVAL)
          {
            posix_error().throw_exception();
          }
          if(_bytes >= 0 && kernelbuffer.size() - (size_t) _bytes >= sizeof(dirent) + 16)
          {
            bytes = (size_t) _bytes;
            threadidsbuffer.clear();
            for(auto *dent = (dirent *) kernelbuffer.data();; dent = reinterpret_cast<dirent *>(reinterpret_cast<uintptr_t>(dent) + dent->d_reclen))
            {
              if(dent->d_ino != 0u && dent->d_type == DT_DIR && dent->d_name[0] != '.')
              {
                size_t length = strchr(dent->d_name, 0) - dent->d_name;
                threadidsbuffer.push_back(string_view(dent->d_name, length));
              }
              if((bytes -= dent->d_reclen) <= 0)
              {
                break;
              }
            }
            auto mythreadcount = threadpool_threads.load(std::memory_order_relaxed);
            if(threadidsbuffer.size() >= mythreadcount)
            {
              // std::cout << "Parsed from /proc " << threadidsbuffer.size() << " entries, should be at least " << mythreadcount << std::endl;
              std::sort(threadidsbuffer.begin(), threadidsbuffer.end());
              done = true;
              break;
            }
#ifndef NDEBUG
            std::cout << "NOTE: /proc returned " << threadidsbuffer.size() << " items when we know for a fact at least " << mythreadcount
                      << " threads exist, retrying!" << std::endl;
#endif
            continue;
          }
          kernelbuffer.resize(kernelbuffer.size() << 1);
        }
      }
      threadmetrics_item *firstnewitem = nullptr;
      _lock_guard g(threadmetrics_lock);
#if 0
      {
        std::stringstream s;
        s << "Parsed from /proc " << threadidsbuffer.size() << " entries (should be at least " << threadpool_threads.load(std::memory_order_relaxed) << "):";
        for(auto &i : threadidsbuffer)
        {
          s << " " << string_view(i.text, 12);
        }
        std::cout << s.str() << std::endl;
      }
#endif
#if 0
      {
        auto d_it = threadmetrics_sorted.begin();
        auto s_it = threadidsbuffer.begin();
        for(; d_it != threadmetrics_sorted.end() && s_it != threadidsbuffer.end(); ++d_it, ++s_it)
        {
          std::cout << (*d_it)->threadid_name() << "   " << string_view(s_it->text, 12) << "\n";
        }
        for(; d_it != threadmetrics_sorted.end(); ++d_it)
        {
          std::cout << (*d_it)->threadid_name() << "   XXXXXXXXXXXX\n";
        }
        for(; s_it != threadidsbuffer.end(); ++s_it)
        {
          std::cout << "XXXXXXXXXXXX   " << string_view(s_it->text, 12) << "\n";
        }
        std::cout << std::flush;
      }
#endif
      auto d_it = threadmetrics_sorted.begin();
      auto s_it = threadidsbuffer.begin();
      auto remove_item = [&] {
        // std::cout << "Removing thread metrics for " << (*d_it)->threadid_name() << std::endl;
        if((*d_it)->blocked_since != std::chrono::steady_clock::time_point())
        {
          threadmetrics_queue.blocked--;
        }
        else
        {
          threadmetrics_queue.running--;
        }
        _remove_from_list(threadmetrics_queue, *d_it);
        d_it = threadmetrics_sorted.erase(d_it);
      };
      auto add_item = [&] {
        auto p = std::make_unique<threadmetrics_item>(*s_it);
        d_it = threadmetrics_sorted.insert(d_it, p.get());
        _append_to_list(threadmetrics_queue, p.get());
        // std::cout << "Adding thread metrics for " << p->threadid_name() << std::endl;
        if(firstnewitem == nullptr)
        {
          firstnewitem = p.get();
        }
        p.release();
        threadmetrics_queue.running++;
      };
      // std::cout << "Compare" << std::endl;
      for(; d_it != threadmetrics_sorted.end() && s_it != threadidsbuffer.end();)
      {
        auto c = (*d_it)->threadid.compare(*s_it);
        // std::cout << "Comparing " << (*d_it)->threadid_name() << " with " << string_view(s_it->text, 12) << " = " << c << std::endl;
        if(0 == c)
        {
          ++d_it;
          ++s_it;
          continue;
        }
        if(c < 0)
        {
          // d_it has gone away
          remove_item();
        }
        if(c > 0)
        {
          // s_it is a new entry
          add_item();
        }
      }
      // std::cout << "Tail dest" << std::endl;
      while(d_it != threadmetrics_sorted.end())
      {
        remove_item();
      }
      // std::cout << "Tail source" << std::endl;
      while(s_it != threadidsbuffer.end())
      {
        add_item();
        ++d_it;
        ++s_it;
      }
      assert(threadmetrics_sorted.size() == threadidsbuffer.size());
#if 1
      if(!std::is_sorted(threadmetrics_sorted.begin(), threadmetrics_sorted.end(),
                         [](threadmetrics_item *a, threadmetrics_item *b) { return a->threadid < b->threadid; }))
      {
        std::cout << "Threadmetrics:";
        for(auto *p : threadmetrics_sorted)
        {
          std::cout << "\n   " << p->threadid_name();
        }
        std::cout << std::endl;
        abort();
      }
#endif
      assert(threadmetrics_queue.running + threadmetrics_queue.blocked == threadidsbuffer.size());
      return update_threadmetrics(std::move(g), now, firstnewitem);
    }
#endif
#endif

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
#else
          (void) grouph;
          workitem->_internaltimerh = (void *) (uintptr_t) -1;
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
        else
        {
          workitem->_timepoint1 = std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(1));
        }
      }
      return success();
    }

    inline void _submit_work_item(_lock_guard &g, dynamic_thread_pool_group::work_item *workitem, bool defer_pool_wake);

    inline result<void> submit(_lock_guard &g, dynamic_thread_pool_group_impl *group, span<dynamic_thread_pool_group::work_item *> work) noexcept;

    inline void _work_item_done(_lock_guard &g, dynamic_thread_pool_group::work_item *i) noexcept;
    inline void _work_item_next(_lock_guard &g, dynamic_thread_pool_group::work_item *i) noexcept;

    inline result<void> stop(_lock_guard &g, dynamic_thread_pool_group_impl *group, result<void> err) noexcept;
    inline result<void> wait(_lock_guard &g, bool reap, dynamic_thread_pool_group_impl *group, deadline d) noexcept;

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
  } _work_items_timer, _work_items_active, _work_items_done, _work_items_delayed;
  std::atomic<bool> _stopping{false}, _stopped{true}, _completing{false};
  std::atomic<int> _waits{0};
  result<void> _abnormal_completion_cause{success()};  // The cause of any abnormal group completion

#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
  dispatch_group_t _grouph;
#elif defined(_WIN32)
  TP_CALLBACK_ENVIRON _callbackenviron;
  PTP_CALLBACK_ENVIRON _grouph{&_callbackenviron};
#else
  void *_grouph{nullptr};
  size_t _newly_added_active_work_items{0};
  size_t _timer_work_items_remaining{0};
  size_t _active_work_items_remaining{0};
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
      detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.workqueue_lock);  // lock global
      // Append this group to the global work queue at its nesting level
      if(_nesting_level >= impl.workqueue.size())
      {
        for(auto &i : impl.workqueue)
        {
          i._currentgroup = (i.currentgroup != i.items.end()) ? *i.currentgroup : nullptr;
          i.currentgroup = {};
        }
        impl.workqueue.resize(_nesting_level + 1);
        for(auto &i : impl.workqueue)
        {
          if(i._currentgroup != nullptr)
          {
            i.currentgroup = i.items.find(i._currentgroup);
          }
          else
          {
            i.currentgroup = i.items.end();
          }
          i._currentgroup = nullptr;
        }
      }
      auto &wq = impl.workqueue[_nesting_level];
      wq.items.insert(this);
      if(wq.items.size() == 1)
      {
        wq.currentgroup = wq.items.begin();
      }
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
    detail::global_dynamic_thread_pool_impl::_lock_guard g(impl.workqueue_lock);  // lock global
    assert(impl.workqueue.size() > _nesting_level);
    auto &wq = impl.workqueue[_nesting_level];
    if(wq.items.end() != wq.currentgroup && *wq.currentgroup == this)
    {
      if(wq.items.end() == ++wq.currentgroup)
      {
        wq.currentgroup = wq.items.begin();
      }
    }
    wq.items.erase(this);
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
    if(_completing.load(std::memory_order_relaxed))
    {
      for(auto *i : work)
      {
        i->_parent.store(this, std::memory_order_release);
        detail::global_dynamic_thread_pool_impl::_append_to_list(_work_items_delayed, i);
      }
      return success();
    }
    _stopped.store(false, std::memory_order_release);
    auto &impl = detail::global_dynamic_thread_pool();
    _lock_guard g(_lock);  // lock group
    if(_work_items_timer.count == 0 && _work_items_active.count == 0 && _work_items_done.count == 0)
    {
      _abnormal_completion_cause = success();
    }
    OUTCOME_TRY(impl.submit(g, this, work));
    if(_work_items_timer.count == 0 && _work_items_active.count == 0)
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
    _lock_guard g(_lock);  // lock group
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
    _lock_guard g(_lock);  // lock group
    return impl.wait(g, true, const_cast<dynamic_thread_pool_group_impl *>(this), d);
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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint32_t dynamic_thread_pool_group::ms_sleep_for_more_work() noexcept
{
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
  return detail::global_dynamic_thread_pool().ms_sleep_for_more_work.load(std::memory_order_relaxed);
#else
  return 0;
#endif
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC uint32_t dynamic_thread_pool_group::ms_sleep_for_more_work(uint32_t v) noexcept
{
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
  if(0 == v)
  {
    v = 1;
  }
  detail::global_dynamic_thread_pool().ms_sleep_for_more_work.store(v, std::memory_order_relaxed);
  return v;
#else
  (void) v;
  return 0;
#endif
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
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
  inline void global_dynamic_thread_pool_impl::_execute_work(thread_t *self)
  {
    pthread_setname_np(pthread_self(), "LLFIO DYN TPG");
    self->last_did_work = std::chrono::steady_clock::now();
    _lock_guard g(workqueue_lock);  // lock global
    self->state++;                  // busy
    threadpool_threads.fetch_add(1, std::memory_order_release);
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
    std::cout << "*** DTP " << self << " begins." << std::endl;
#endif
    size_t workqueue_depth = workqueue.size() - 1;
    while(self->state > 0)
    {
    restart:
      dynamic_thread_pool_group::work_item *workitem = nullptr;
      std::chrono::steady_clock::time_point earliest_duration;
      std::chrono::system_clock::time_point earliest_absolute;
      if(!workqueue.empty())
      {
        auto wq_it = --workqueue.end();
        bool in_highest_priority_queue = true, started_from_top = false;
        if(workqueue.size() > 1 && workqueue_depth < workqueue.size() - 1)
        {
          wq_it = workqueue.begin() + workqueue_depth;
          in_highest_priority_queue = false;
        }
        auto *wq = &(*wq_it);
        if(in_highest_priority_queue)
        {
          if(wq->currentgroup == wq->items.end())
          {
            wq->currentgroup = wq->items.begin();
            started_from_top = true;
          }
          else if(wq->currentgroup == wq->items.begin())
          {
            started_from_top = true;
          }
        }
        for(;;)
        {
          dynamic_thread_pool_group_impl *tpg = *wq->currentgroup;
          _lock_guard gg(tpg->_lock);  // lock group
          if(started_from_top)
          {
            if(tpg->_timer_work_items_remaining == 0 && tpg->_active_work_items_remaining == 0)
            {
              tpg->_timer_work_items_remaining = (size_t) -1;
              tpg->_active_work_items_remaining = (size_t) -1;
            }
            else
            {
              started_from_top = false;
            }
          }
          std::cout << "*** DTP " << self << " sees in group " << tpg << " in_highest_priority_queue = " << in_highest_priority_queue
                    << " work items = " << tpg->_work_items_active.count << " timer items = " << tpg->_work_items_timer.count << std::endl;
          if(tpg->_work_items_timer.count > 0 && tpg->_work_items_timer.front->_internalworkh != nullptr)
          {
            tpg->_timer_work_items_remaining = 0;  // this group is being fully executed right now
          }
          else if(tpg->_timer_work_items_remaining > tpg->_work_items_timer.count)
          {
            tpg->_timer_work_items_remaining = tpg->_work_items_timer.count;
          }
          // If we are not in the highest priority group, and there are no newly added
          // work items, don't execute work.
          if(tpg->_work_items_active.count > 0 &&
             (tpg->_work_items_active.front->_internalworkh != nullptr || (!in_highest_priority_queue && tpg->_newly_added_active_work_items == 0)))
          {
            tpg->_active_work_items_remaining = 0;  // this group is being fully executed right now
          }
          else if(tpg->_active_work_items_remaining > tpg->_work_items_active.count)
          {
            tpg->_active_work_items_remaining = tpg->_work_items_active.count;
          }
          std::cout << "*** DTP " << self << " sees in group " << tpg << " _active_work_items_remaining = " << tpg->_active_work_items_remaining
                    << " _timer_work_items_remaining = " << tpg->_timer_work_items_remaining << std::endl;
          if(tpg->_timer_work_items_remaining > 0)
          {
            auto *wi = tpg->_work_items_timer.front;
            assert(wi->_has_timer_set());
            _remove_from_list(tpg->_work_items_timer, wi);
            _append_to_list(tpg->_work_items_timer, wi);
            tpg->_timer_work_items_remaining--;

            bool invoketimer = false;
            if(wi->_has_timer_set_relative())
            {
              // Special constant for immediately rescheduled work items
              if(wi->_timepoint1 == std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(1)))
              {
                invoketimer = true;
              }
              else if(earliest_duration == std::chrono::steady_clock::time_point() || wi->_timepoint1 < earliest_duration)
              {
                earliest_duration = wi->_timepoint1;
                if(wi->_timepoint1 <= std::chrono::steady_clock::now())
                {
                  invoketimer = true;
                }
              }
            }
            if(wi->_has_timer_set_absolute() && (earliest_absolute == std::chrono::system_clock::time_point() || wi->_timepoint2 < earliest_absolute))
            {
              earliest_absolute = wi->_timepoint2;
              if(wi->_timepoint2 <= std::chrono::system_clock::now())
              {
                invoketimer = true;
              }
            }
            // If this work item's timer is due, execute immediately
            if(invoketimer)
            {
              wi->_internalworkh = self;
              wi->_internaltimerh = nullptr;
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
              std::cout << "*** DTP " << self << " executes timer item " << wi << std::endl;
#endif
              gg.unlock();
              g.unlock();
              _timerthread(wi, nullptr);
              g.lock();
              // wi->_internalworkh should be null, however wi may also no longer exist
              goto restart;
            }
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
            std::cout << "*** DTP " << self << " timer item " << wi << " timer is not ready yet " << std::endl;
#endif
            // Process all timers before considering work
            continue;
          }
          if(tpg->_active_work_items_remaining > 0)
          {
            auto *wi = tpg->_work_items_active.front;
            assert(!wi->_has_timer_set());
            _remove_from_list(tpg->_work_items_active, wi);
            _append_to_list(tpg->_work_items_active, wi);
            tpg->_active_work_items_remaining--;

            if(tpg->_newly_added_active_work_items > 0)
            {
              tpg->_newly_added_active_work_items--;
            }
            workitem = wi;
            workitem->_internalworkh = self;
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
            std::cout << "*** DTP " << self << " chooses work item " << workitem << " from group " << tpg << " distance from top "
                      << (workqueue.end() - wq_it - 1) << std::endl;
#endif
            // Execute this workitem
            break;
          }

          // Move to next group, which may be in a lower priority work queue
          assert(tpg->_active_work_items_remaining == 0);
          if(++wq->currentgroup == wq->items.end())
          {
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
            std::cout << "*** DTP " << self << " reaches end of current group, started_from_top = " << started_from_top << std::endl;
#endif
            gg.unlock();  // unlock group
            if(!started_from_top)
            {
              workqueue_depth = (size_t) -1;
              goto restart;
            }
            // Nothing for me to do in this workqueue
            if(wq_it == workqueue.begin())
            {
              // Reset all wq->currentgroup to begin() to ensure timers
              // etc in lower priority groups do get seen
              for(auto &i : workqueue)
              {
                if(i.currentgroup == i.items.end())
                {
                  i.currentgroup = i.items.begin();
                }
              }
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
              std::cout << "*** DTP " << self << " finds work queue empty, going to sleep." << std::endl;
#endif
              assert(workitem == nullptr);
              goto workqueue_empty;
            }
            --wq_it;
            workqueue_depth = wq_it - workqueue.begin();
            wq = &(*wq_it);
            tpg = *wq->currentgroup;
            gg = _lock_guard(tpg->_lock);  // lock group
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
            std::cout << "*** DTP " << self << " moves work search up to distance from top = " << (workqueue.end() - wq_it - 1) << " examining " << tpg
                      << " finds _work_items_active.count = " << tpg->_work_items_active.count << "." << std::endl;
#endif
          }
          tpg = *wq->currentgroup;
          tpg->_active_work_items_remaining = tpg->_work_items_active.count;
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
          std::cout << "*** DTP " << self << " choose new group " << tpg << std::endl;
#endif
        }
      }
    workqueue_empty:
      auto now = std::chrono::steady_clock::now();
      if(workitem == nullptr)
      {
        std::chrono::steady_clock::duration duration(std::chrono::minutes(1));
        if(earliest_duration != std::chrono::steady_clock::time_point())
        {
          if(now - earliest_duration < duration)
          {
            duration = now - earliest_duration;
          }
        }
        else if(earliest_absolute != std::chrono::system_clock::time_point())
        {
          auto diff = std::chrono::system_clock::now() - earliest_absolute;
          if(diff > duration)
          {
            earliest_absolute = {};
          }
        }
        else if(now - self->last_did_work >= std::chrono::milliseconds(ms_sleep_for_more_work.load(std::memory_order_relaxed)))
        {
          _remove_from_list(threadpool_active, self);
          threadpool_threads.fetch_sub(1, std::memory_order_release);
          self->thread.detach();
          delete self;
          return;
        }
        self->last_did_work = now;
        _remove_from_list(threadpool_active, self);
        _append_to_list(threadpool_sleeping, self);
        self->state--;
        threadpool_sleeping_count.fetch_add(1, std::memory_order_release);
        if(earliest_absolute != std::chrono::system_clock::time_point())
        {
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
          std::cout << "*** DTP " << self << " goes to sleep absolute" << std::endl;
#endif
          self->cond.wait_until(g, earliest_absolute);
        }
        else
        {
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
          std::cout << "*** DTP " << self << " goes to sleep for " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << std::endl;
#endif
          self->cond.wait_for(g, duration);
        }
        self->state++;
        _remove_from_list(threadpool_sleeping, self);
        _append_to_list(threadpool_active, self);
        threadpool_sleeping_count.fetch_sub(1, std::memory_order_release);
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
        std::cout << "*** DTP " << self << " wakes, state = " << self->state << std::endl;
#endif
        g.unlock();
        try
        {
          populate_threadmetrics(now);
        }
        catch(...)
        {
        }
        g.lock();
        continue;
      }
      self->last_did_work = now;
#if 1  // LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
      std::cout << "*** DTP " << self << " executes work item " << workitem << std::endl;
#endif
      total_submitted_workitems.fetch_sub(1, std::memory_order_relaxed);
      g.unlock();
      _workerthread(workitem, nullptr);
      // workitem->_internalworkh should be null, however workitem may also no longer exist
      try
      {
        if(populate_threadmetrics(now))
        {
          _remove_from_list(threadpool_active, self);
          threadpool_threads.fetch_sub(1, std::memory_order_release);
          self->thread.detach();
          delete self;
          return;
        }
      }
      catch(...)
      {
      }
      g.lock();
    }
    self->state -= 2;  // dead
    threadpool_threads.fetch_sub(1, std::memory_order_release);
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
    std::cout << "*** DTP " << self << " exits, state = " << self->state << std::endl;
#endif
  }
#endif

  inline void global_dynamic_thread_pool_impl::_submit_work_item(_lock_guard &g, dynamic_thread_pool_group::work_item *workitem, bool defer_pool_wake)
  {
    (void) g;
    (void) defer_pool_wake;
    if(workitem->_nextwork != -1)
    {
      // If no work item for now, or there is a delay, schedule a timer
      if(workitem->_nextwork == 0 || workitem->_has_timer_set())
      {
        assert(workitem->_internaltimerh != nullptr);
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
        dispatch_time_t when;
        if(workitem->_has_timer_set_relative())
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
        else if(workitem->_has_timer_set_absolute())
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
        if(workitem->_has_timer_set_relative())
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
        else if(workitem->_has_timer_set_absolute())
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
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
      // Indicate that I can be executed again
      workitem->_internalworkh = nullptr;
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
      std::cout << "*** DTP submits work item " << workitem << std::endl;
#endif
      const auto active_work_items = total_submitted_workitems.fetch_add(1, std::memory_order_relaxed) + 1;
      if(!defer_pool_wake)
      {
        const auto sleeping_count = threadpool_sleeping_count.load(std::memory_order_relaxed);
        const auto threads = threadpool_threads.load(std::memory_order_relaxed);
        if(sleeping_count > 0 || threads == 0)
        {
          g.unlock();  // unlock group
          {
            _lock_guard gg(workqueue_lock);  // lock global
            if(threadpool_active.count == 0 && threadpool_sleeping.count == 0)
            {
              _add_thread(gg);
            }
            else if(threadpool_sleeping.count > 0 && active_work_items > threadpool_active.count)
            {
              // Try to wake the most recently slept first
              auto *t = threadpool_sleeping.back;
              auto now = std::chrono::steady_clock::now();
              for(size_t n = std::min(active_work_items - threadpool_active.count, threadpool_sleeping.count); n > 0; n--)
              {
                t->last_did_work = now;  // prevent reap
                t->cond.notify_one();
                t = t->_prev;
              }
            }
          }
          g.lock();  // lock group
        }
      }
#endif
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
          _remove_from_list(!i->_has_timer_set() ? group->_work_items_active : group->_work_items_timer, i);
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
          if(!i->_has_timer_set())
          {
            _prepend_to_list(group->_work_items_active, i);
          }
          else
          {
            _prepend_to_list(group->_work_items_timer, i);
          }
        }
      }
      uninit.release();
      {
        for(auto *i : work)
        {
#if !LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD && !defined(_WIN32)
          group->_newly_added_active_work_items++;
          group->_active_work_items_remaining++;
#endif
          _submit_work_item(g, i, i != work.back());
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
    auto *parent = i->_parent.load(std::memory_order_relaxed);
    _remove_from_list(!i->_has_timer_set() ? parent->_work_items_active : parent->_work_items_timer, i);
    _append_to_list(parent->_work_items_done, i);
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
#else
    i->_internaltimerh = nullptr;
    i->_internalworkh = nullptr;
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
    std::cout << "*** DTP sets done work item " << i << std::endl;
#endif
#endif
    if(parent->_work_items_timer.count == 0 && parent->_work_items_active.count == 0)
    {
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
      parent->_completing.store(true, std::memory_order_release);  // cause submissions to enter _work_items_delayed
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
      std::cout << "*** DTP executes group_complete for group " << parent << std::endl;
#endif
      for(; v != nullptr; v = n)
      {
        n = v->_next;
        v->group_complete(parent->_abnormal_completion_cause);
      }
      parent->_completing.store(false, std::memory_order_release);  // cease submitting to _work_items_delayed
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
      std::cout << "*** DTP group_complete done for group " << parent << ". _work_items_delayed.count = " << parent->_work_items_delayed.count << std::endl;
#endif
      if(parent->_work_items_delayed.count > 0)
      {
        /* If there are waits on this group to complete, forward progress those now.
         */
        while(parent->_waits.load(std::memory_order_relaxed) > 0)
        {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
          std::cout << "*** DTP group_complete blocks on waits for group " << parent << std::endl;
#endif
          g.unlock();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          g.lock();
        }
        // Now submit all delayed work
        while(parent->_work_items_delayed.count > 0)
        {
          i = parent->_work_items_delayed.front;
          _remove_from_list(parent->_work_items_delayed, i);
          auto r = submit(g, parent, {&i, 1});
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_PRINTING
          std::cout << "*** DTP group_complete submits delayed work item " << i << " for group " << parent << " which saw error ";
          if(r)
          {
            std::cout << "none" << std::endl;
          }
          else
          {
            std::cout << r.error().message() << std::endl;
          }
#endif
          if(!r)
          {
            parent->_work_items_delayed = {};
            (void) stop(g, parent, std::move(r));
            break;
          }
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
    _submit_work_item(g, workitem, false);
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


  inline result<void> global_dynamic_thread_pool_impl::wait(_lock_guard &g, bool reap, dynamic_thread_pool_group_impl *group, deadline d) noexcept
  {
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    if(!d || d.nsecs > 0)
    {
      /* To ensure forward progress, we need to gate new waits during delayed work submission.
      Otherwise waits may never exit if the window where _work_items_active.count == 0 is
      missed.
      */
      while(group->_work_items_delayed.count > 0)
      {
        g.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g.lock();
      }
      group->_waits.fetch_add(1, std::memory_order_release);
      auto unwaitcount = make_scope_exit([&]() noexcept { group->_waits.fetch_sub(1, std::memory_order_release); });
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      while(group->_work_items_timer.count > 0 || group->_work_items_active.count > 0)
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
        while(group->_work_items_timer.count > 0 || group->_work_items_active.count > 0)
        {
          auto *i = (group->_work_items_active.front != nullptr) ? group->_work_items_active.front : group->_work_items_timer.front;
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
          if(group->_work_items_timer.count > 0 && group->_work_items_timer.front == i)
          {
            // This item got cancelled before it started
            _work_item_done(g, group->_work_items_timer.front);
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
        while(group->_work_items_timer.count > 0 || group->_work_items_active.count > 0)
        {
          auto *i = (group->_work_items_active.front != nullptr) ? group->_work_items_active.front : group->_work_items_timer.front;
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
        while(group->_work_items_timer.count > 0 || group->_work_items_active.count > 0)
        {
          LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
          g.unlock();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          g.lock();
        }
      }
#else
#if 0
      if(group->_stopping.load(std::memory_order_relaxed))
      {
        // Kill all work items not currently being executed immediately
        for(bool done = false; !done;)
        {
          done = true;
          for(auto *p = group->_work_items_timer.front; p != nullptr; p = p->_next)
          {
            if(p->_internalworkh == nullptr)
            {
              _remove_from_list(group->_work_items_timer, p);
              _append_to_list(group->_work_items_done, p);
              done = false;
              break;
            }
          }
          for(auto *p = group->_work_items_active.front; p != nullptr; p = p->_next)
          {
            if(p->_internalworkh == nullptr)
            {
              _remove_from_list(group->_work_items_active, p);
              _append_to_list(group->_work_items_done, p);
              done = false;
              break;
            }
          }
        }
      }
#endif
      while(group->_work_items_timer.count > 0 || group->_work_items_active.count > 0)
      {
        LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
        g.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        g.lock();
      }
#endif
    }
    if(group->_work_items_timer.count > 0 || group->_work_items_active.count > 0)
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
    _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);  // lock group
    // std::cout << "*** _timerthread " << workitem << std::endl;
    if(workitem->_parent.load(std::memory_order_relaxed)->_stopping.load(std::memory_order_relaxed))
    {
      _work_item_done(g, workitem);
      return;
    }
    if(workitem->_has_timer_set_relative())
    {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      auto now = std::chrono::steady_clock::now();
      if(workitem->_timepoint1 - now > std::chrono::seconds(0))
      {
        // Timer fired short, so schedule it again
        _submit_work_item(g, workitem, false);
        return;
      }
#endif
      workitem->_timepoint1 = {};
    }
    if(workitem->_has_timer_set_absolute())
    {
#if LLFIO_DYNAMIC_THREAD_POOL_GROUP_USING_GCD
      auto now = std::chrono::system_clock::now();
      if(workitem->_timepoint2 - now > std::chrono::seconds(0))
      {
        // Timer fired short, so schedule it again
        _submit_work_item(g, workitem, false);
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
      _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);  // lock group
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
    _lock_guard g(workitem->_parent.load(std::memory_order_relaxed)->_lock);  // lock group
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
