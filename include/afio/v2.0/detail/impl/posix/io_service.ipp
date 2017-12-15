/* Multiplex file i/o
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (4 commits)
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

#include "../../../async_file_handle.hpp"

#include <pthread.h>
#if AFIO_USE_POSIX_AIO
#include <aio.h>
#include <sys/mman.h>
#if AFIO_COMPILE_KQUEUES
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#endif
#endif

AFIO_V2_NAMESPACE_BEGIN

static int interrupt_signal;
static struct sigaction interrupt_signal_handler_old_action;
struct ucontext;
static inline void interrupt_signal_handler(int /*unused*/, siginfo_t * /*unused*/, void * /*unused*/)
{
  // We do nothing, and aio_suspend should exit with EINTR
}

int io_service::interruption_signal() noexcept
{
  return interrupt_signal;
}

int io_service::set_interruption_signal(int signo)
{
  int ret = interrupt_signal;
  if(interrupt_signal != 0)
  {
    if(sigaction(interrupt_signal, &interrupt_signal_handler_old_action, nullptr) < 0)
    {
      throw std::system_error(errno, std::system_category());
    }
    interrupt_signal = 0;
  }
  if(signo != 0)
  {
#if AFIO_HAVE_REALTIME_SIGNALS
    if(-1 == signo)
    {
      for(signo = SIGRTMIN; signo < SIGRTMAX; signo++)
      {
        struct sigaction sigact
        {
        };
        memset(&sigact, 0, sizeof(sigact));
        if(sigaction(signo, nullptr, &sigact) >= 0)
        {
          if(sigact.sa_handler == SIG_DFL)
          {
            break;
          }
        }
      }
    }
#endif
    // Install process wide signal handler for signal
    struct sigaction sigact
    {
    };
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = &interrupt_signal_handler;
    sigact.sa_flags = SA_SIGINFO;
    sigemptyset(&sigact.sa_mask);
    if(sigaction(signo, &sigact, &interrupt_signal_handler_old_action) < 0)
    {
      throw std::system_error(errno, std::system_category());
    }
    interrupt_signal = signo;
  }
  return ret;
}

void io_service::_block_interruption() noexcept
{
  if(_use_kqueues)
  {
    return;
  }
  assert(!_blocked_interrupt_signal);
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, interrupt_signal);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);
  _blocked_interrupt_signal = interrupt_signal;
  _need_signal = false;
}

void io_service::_unblock_interruption() noexcept
{
  if(_use_kqueues)
  {
    return;
  }
  assert(_blocked_interrupt_signal);
  if(_blocked_interrupt_signal != 0)
  {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, _blocked_interrupt_signal);
    pthread_sigmask(SIG_UNBLOCK, &set, nullptr);
    _blocked_interrupt_signal = 0;
    _need_signal = true;
  }
}

io_service::io_service()
    : _work_queued(0)
{
  _threadh = pthread_self();
#if AFIO_USE_POSIX_AIO
  _use_kqueues = true;
  _blocked_interrupt_signal = 0;
#if AFIO_COMPILE_KQUEUES
  _kqueueh = 0;
#error todo
#else
  disable_kqueues();
#endif
#else
#error todo
#endif
}

io_service::~io_service()
{
  if(_work_queued != 0u)
  {
#ifndef NDEBUG
    fprintf(stderr, "WARNING: ~io_service() sees work still queued, blocking until no work queued\n");
#endif
    while(_work_queued != 0u)
    {
      std::this_thread::yield();
    }
  }
#if AFIO_USE_POSIX_AIO
#if AFIO_COMPILE_KQUEUES
  if(_kqueueh)
    ::close(_kqueueh);
#endif
  _aiocbsv.clear();
  if(pthread_self() == _threadh)
  {
    _unblock_interruption();
  }
#else
#error todo
#endif
}

#if AFIO_USE_POSIX_AIO
void io_service::disable_kqueues()
{
  if(_use_kqueues)
  {
    if(_work_queued != 0u)
    {
      throw std::runtime_error("Cannot disable kqueues if work is pending");
    }
    if(pthread_self() != _threadh)
    {
      throw std::runtime_error("Cannot disable kqueues except from owning thread");
    }
    // Is the global signal handler set yet?
    if(interrupt_signal == 0)
    {
      set_interruption_signal();
    }
    _use_kqueues = false;
    // Block interruption on this thread
    _block_interruption();
// Prepare for aio_suspend
#ifdef AIO_LISTIO_MAX
    _aiocbsv.reserve(AIO_LISTIO_MAX);
#else
    _aiocbsv.reserve(16);
#endif
  }
}
#endif

result<bool> io_service::run_until(deadline d) noexcept
{
  if(_work_queued == 0u)
  {
    return false;
  }
  if(pthread_self() != _threadh)
  {
    return std::errc::operation_not_supported;
  }
  std::chrono::steady_clock::time_point began_steady;
  std::chrono::system_clock::time_point end_utc;
  if(d)
  {
    if(d.steady)
    {
      began_steady = std::chrono::steady_clock::now();
    }
    else
    {
      end_utc = d.to_time_point();
    }
  }
  struct timespec *ts = nullptr, _ts{};
  memset(&_ts, 0, sizeof(_ts));
  bool done = false;
  do
  {
    if(d)
    {
      std::chrono::nanoseconds ns;
      if(d.steady)
      {
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds(d.nsecs)) - std::chrono::steady_clock::now());
      }
      else
      {
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_utc - std::chrono::system_clock::now());
      }
      ts = &_ts;
      if(ns.count() <= 0)
      {
        ts->tv_sec = 0;
        ts->tv_nsec = 0;
      }
      else
      {
        ts->tv_sec = ns.count() / 1000000000ULL;
        ts->tv_nsec = ns.count() % 1000000000ULL;
      }
    }
    bool timedout = false;
    // Unblock the interruption signal
    _unblock_interruption();
    // Execute any pending posts
    {
      std::unique_lock<decltype(_posts_lock)> g(_posts_lock);
      if(!_posts.empty())
      {
        post_info *pi = &_posts.front();
        g.unlock();
        pi->f(this);
        _post_done(pi);
        // We did work, so exit
        // Block the interruption signal
        _block_interruption();
        return _work_queued != 0;
      }
    }
#if AFIO_USE_POSIX_AIO
    int errcode = 0;
    if(_use_kqueues)
    {
#if AFIO_COMPILE_KQUEUES
#error todo
#endif
    }
    else
    {
      if(aio_suspend(_aiocbsv.data(), _aiocbsv.size(), ts) < 0)
      {
        errcode = errno;
      }
    }
    // Block the interruption signal
    _block_interruption();
    if(errcode != 0)
    {
      switch(errcode)
      {
      case EAGAIN:
        if(d)
        {
          timedout = true;
        }
        break;
      case EINTR:
        // Let him loop, recalculate any timeout and check for posts to be executed
        break;
      default:
        return {errcode, std::system_category()};
      }
    }
    else
    {
      // Poll the outstanding aiocbs to see which are ready
      for(auto &aiocb : _aiocbsv)
      {
        int ioerr = aio_error(aiocb);
        if(EINPROGRESS == ioerr)
        {
          continue;
        }
        if(0 == ioerr)
        {
          // Scavenge the aio
          int ioret = aio_return(aiocb);
          if(ioret < 0)
          {
            return { errno, std::system_category() };
          }
          // std::cout << "aiocb " << aiocb << " sees succesful return " << ioret << std::endl;
          // The aiocb aio_sigevent.sigev_value.sival_ptr field will point to a file_handle::_io_state_type
          auto io_state = static_cast<async_file_handle::_erased_io_state_type *>(aiocb->aio_sigevent.sigev_value.sival_ptr);
          assert(io_state);
          io_state->_system_io_completion(0, ioret, &aiocb);
        }
        else
        {
          // Either cancelled or errored out
          // std::cout << "aiocb " << aiocb << " sees failed return " << ioerr << std::endl;
          // The aiocb aio_sigevent.sigev_value.sival_ptr field will point to a file_handle::_io_state_type
          auto io_state = static_cast<async_file_handle::_erased_io_state_type *>(aiocb->aio_sigevent.sigev_value.sival_ptr);
          assert(io_state);
          io_state->_system_io_completion(ioerr, 0, &aiocb);
        }
      }
      // Eliminate any empty holes in the quick aiocbs vector
      _aiocbsv.erase(std::remove(_aiocbsv.begin(), _aiocbsv.end(), nullptr), _aiocbsv.end());
      done = true;
    }
#else
#error todo
#endif
    if(timedout)
    {
      if(d.steady)
      {
        if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds(d.nsecs)))
        {
          return std::errc::timed_out;
        }
      }
      else
      {
        if(std::chrono::system_clock::now() >= end_utc)
        {
          return std::errc::timed_out;
        }
      }
    }
  } while(!done);
  return _work_queued != 0;
}

void io_service::_post(detail::function_ptr<void(io_service *)> &&f)
{
  {
    post_info pi(this, std::move(f));
    std::lock_guard<decltype(_posts_lock)> g(_posts_lock);
    _posts.push_back(std::move(pi));
  }
  _work_enqueued();
#if AFIO_USE_POSIX_AIO
  if(_use_kqueues)
  {
#if AFIO_COMPILE_KQUEUES
#error todo
#endif
  }
  else
  {
    // If run_until() is exactly between the unblock of the signal and the beginning
    // of the aio_suspend(), we need to pump this until run_until() notices
    while(_need_signal)
    {
      //#  if AFIO_HAVE_REALTIME_SIGNALS
      //    sigval val = { 0 };
      //    pthread_sigqueue(_threadh, interrupt_signal, val);
      //#else
      pthread_kill(_threadh, interrupt_signal);
      //#  endif
    }
  }
#else
#error todo
#endif
}

AFIO_V2_NAMESPACE_END
