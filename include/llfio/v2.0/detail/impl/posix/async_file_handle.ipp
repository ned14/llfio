/* A handle to something
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (5 commits)
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

#include "../../../handle.hpp"

#include <fcntl.h>
#include <unistd.h>
#if LLFIO_USE_POSIX_AIO
#include <aio.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::barrier(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  optional<io_result<const_buffers_type>> ret;
  OUTCOME_TRY(io_state, async_barrier(reqs, [&ret](async_file_handle *, io_result<const_buffers_type> &result) { ret = result; }, wait_for_device, and_metadata));
  (void) io_state;

  // While i/o is not done pump i/o completion
  while(!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
    {
      return t.error();
    }
#ifndef NDEBUG
    if(!ret && t && !t.value())
    {
      LLFIO_LOG_FATAL(_v.fd, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}

template <class BuffersType, class IORoutine> result<async_file_handle::io_state_ptr> async_file_handle::_begin_io(span<char> mem, async_file_handle::operation_t operation, async_file_handle::io_request<BuffersType> reqs, async_file_handle::_erased_completion_handler &&completion, IORoutine && /*unused*/) noexcept
{
  // Need to keep a set of aiocbs matching the scatter-gather buffers
  struct state_type : public _erased_io_state_type
  {
#if LLFIO_USE_POSIX_AIO
    struct aiocb aiocbs[1]{};
#else
#error todo
#endif
    _erased_completion_handler *completion;
    state_type() = delete;
    state_type(state_type &&) = delete;
    state_type(const state_type &) = delete;
    state_type &operator=(state_type &&) = delete;
    state_type &operator=(const state_type &) = delete;
    state_type(async_file_handle *_parent, operation_t _operation, bool must_deallocate_self, size_t _items)
        : _erased_io_state_type(_parent, _operation, must_deallocate_self, _items)
        , completion(nullptr)
    {
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC _erased_completion_handler *erased_completion_handler() noexcept final { return completion; }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void _system_io_completion(long errcode, long bytes_transferred, void *internal_state) noexcept final
    {
#if LLFIO_USE_POSIX_AIO
      auto **_paiocb = static_cast<struct aiocb **>(internal_state);
      struct aiocb *aiocb = *_paiocb;
      assert(aiocb >= aiocbs && aiocb < aiocbs + this->items);
      *_paiocb = nullptr;
#else
#error todo
#endif
      auto &result = this->result.write;
      if(result)
      {
        if(errcode)
        {
          result = posix_error(static_cast<int>(errcode));
        }
        else
        {
// Figure out which i/o I am and update the buffer in question
#if LLFIO_USE_POSIX_AIO
          size_t idx = aiocb - aiocbs;
#else
#error todo
#endif
          if(idx >= this->items)
          {
            LLFIO_LOG_FATAL(0, "file_handle::io_state::operator() called with invalid index");
            std::terminate();
          }
          result.value()[idx].len = bytes_transferred;
        }
      }
      this->parent->service()->_work_done();
      // Are we done?
      if(!--this->items_to_go)
      {
        (*completion)(this);
      }
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~state_type() final
    {
      // Do we need to cancel pending i/o?
      if(this->items_to_go)
      {
        for(size_t n = 0; n < this->items; n++)
        {
#if LLFIO_USE_POSIX_AIO
          int ret = aio_cancel(this->parent->native_handle().fd, aiocbs + n);
          (void) ret;
#if 0
          if(ret<0 || ret==AIO_NOTCANCELED)
          {
            std::cout << "Failed to cancel " << (aiocbs+n) << std::endl;
          }
          else if(ret==AIO_CANCELED)
          {
            std::cout << "Cancelled " << (aiocbs+n) << std::endl;
          }
          else if(ret==AIO_ALLDONE)
          {
            std::cout << "Already done " << (aiocbs+n) << std::endl;
          }
#endif
#else
#error todo
#endif
        }
        // Pump the i/o service until all pending i/o is completed
        while(this->items_to_go)
        {
          auto res = this->parent->service()->run();
          (void) res;
#ifndef NDEBUG
          if(res.has_error())
          {
            LLFIO_LOG_FATAL(0, "file_handle: io_service failed");
            std::terminate();
          }
          if(!res.value())
          {
            LLFIO_LOG_FATAL(0, "file_handle: io_service returns no work when i/o has not completed");
            std::terminate();
          }
#endif
        }
      }
      completion->~_erased_completion_handler();
    }
  } * state;
  extent_type offset = reqs.offset;
  size_t statelen = sizeof(state_type) + (reqs.buffers.size() - 1) * sizeof(struct aiocb) + completion.bytes();
  if(!mem.empty() && statelen > mem.size())
  {
    return errc::not_enough_memory;
  }
  size_t items(reqs.buffers.size());
#if LLFIO_USE_POSIX_AIO && defined(AIO_LISTIO_MAX)
  // If this i/o could never be done atomically, reject
  if(items > AIO_LISTIO_MAX)
    return errc::invalid_argument;
#if defined(__FreeBSD__) || defined(__APPLE__)
  if(!service()->using_kqueues())
  {
    // BSD and Apple put a tight limit on how many entries
    // aio_suspend() will take in order to have reasonable
    // performance. But their documentation lies, if you
    // feed more than AIO_LISTIO_MAX items to aio_suspend
    // it does NOT return EINVAL as specified, but rather
    // simply marks all items past AIO_LISTIO_MAX as failed
    // with EAGAIN. That punishes performance for AFIO
    // because we loop setting up and tearing down
    // the handlers, so if we would overload llfio_suspend,
    // better to error out now rather that later in io_service.
    if(service()->_aiocbsv.size() + items > AIO_LISTIO_MAX)
    {
      return errc::resource_unavailable_try_again;
    }
  }
#endif
#endif
  bool must_deallocate_self = false;
  if(mem.empty())
  {
    void *_mem = ::calloc(1, statelen);  // NOLINT
    if(!_mem)
    {
      return errc::not_enough_memory;
    }
    mem = {static_cast<char *>(_mem), statelen};
    must_deallocate_self = true;
  }
  io_state_ptr _state(reinterpret_cast<state_type *>(mem.data()));
  new((state = reinterpret_cast<state_type *>(mem.data()))) state_type(this, operation, must_deallocate_self, items);
  state->completion = reinterpret_cast<_erased_completion_handler *>(reinterpret_cast<uintptr_t>(state) + sizeof(state_type) + (reqs.buffers.size() - 1) * sizeof(struct aiocb));
  completion.move(state->completion);

  // Noexcept move the buffers from req into result
  BuffersType &out = state->result.write.value();
  out = std::move(reqs.buffers);
  for(size_t n = 0; n < items; n++)
  {
#if LLFIO_USE_POSIX_AIO
#ifndef NDEBUG
    if(_v.requires_aligned_io())
    {
      assert((offset & 511) == 0);
      assert(((uintptr_t) out[n].data & 511) == 0);
      assert((out[n].len & 511) == 0);
    }
#endif
    struct aiocb *aiocb = state->aiocbs + n;
    aiocb->aio_fildes = _v.fd;
    aiocb->aio_offset = offset;
    aiocb->aio_buf = reinterpret_cast<void *>(const_cast<byte *>(out[n].data));
    aiocb->aio_nbytes = out[n].len;
    aiocb->aio_sigevent.sigev_notify = SIGEV_NONE;
    aiocb->aio_sigevent.sigev_value.sival_ptr = reinterpret_cast<void *>(state);
    switch(operation)
    {
    case operation_t::read:
      aiocb->aio_lio_opcode = LIO_READ;
      break;
    case operation_t::write:
      aiocb->aio_lio_opcode = LIO_WRITE;
      break;
    case operation_t::fsync_async:
    case operation_t::fsync_sync:
      aiocb->aio_lio_opcode = LIO_NOP;
      break;
    case operation_t::dsync_async:
    case operation_t::dsync_sync:
      aiocb->aio_lio_opcode = LIO_NOP;
      break;
    }
#else
#error todo
#endif
    offset += out[n].len;
    ++state->items_to_go;
  }
  int ret = 0;
#if LLFIO_USE_POSIX_AIO
  if(service()->using_kqueues())
  {
#if LLFIO_COMPILE_KQUEUES
    // Only issue one kqueue event when entire scatter-gather has completed
    struct _sigev = {0};
#error todo
#endif
  }
  else
  {
    // Add these i/o's to the quick aio_suspend list
    service()->_aiocbsv.resize(service()->_aiocbsv.size() + items);
    struct aiocb **thislist = service()->_aiocbsv.data() + service()->_aiocbsv.size() - items;
    for(size_t n = 0; n < items; n++)
    {
      struct aiocb *aiocb = state->aiocbs + n;
      thislist[n] = aiocb;
    }
    switch(operation)
    {
    case operation_t::read:
    case operation_t::write:
      ret = lio_listio(LIO_NOWAIT, thislist, items, nullptr);
      break;
    case operation_t::fsync_async:
    case operation_t::fsync_sync:
    case operation_t::dsync_async:
    case operation_t::dsync_sync:
      for(size_t n = 0; n < items; n++)
      {
        struct aiocb *aiocb = state->aiocbs + n;
#if defined(__FreeBSD__) || defined(__APPLE__)  // neither of these have fdatasync()
        ret = aio_fsync(O_SYNC, aiocb);
#else
        ret = aio_fsync((operation == operation_t::dsync_async || operation == operation_t::dsync_sync) ? O_DSYNC : O_SYNC, aiocb);
#endif
      }
      break;
    }
  }
#else
#error todo
#endif
  if(ret < 0)
  {
    service()->_aiocbsv.resize(service()->_aiocbsv.size() - items);
    state->items_to_go = 0;
    state->result.write = posix_error();
    (*state->completion)(state);
    return success(std::move(_state));
  }
  service()->_work_enqueued(items);
  return success(std::move(_state));
}

result<async_file_handle::io_state_ptr> async_file_handle::_begin_io(span<char> mem, async_file_handle::operation_t operation, io_request<const_buffers_type> reqs, async_file_handle::_erased_completion_handler &&completion) noexcept
{
  return _begin_io(mem, operation, reqs, std::move(completion), nullptr);
}

async_file_handle::io_result<async_file_handle::buffers_type> async_file_handle::read(async_file_handle::io_request<async_file_handle::buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  optional<io_result<buffers_type>> ret;
  OUTCOME_TRY(io_state, async_read(reqs, [&ret](async_file_handle *, io_result<buffers_type> &result) { ret = result; }));
  (void) io_state;

  // While i/o is not done pump i/o completion
  while(!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
    {
      return t.error();
    }
#ifndef NDEBUG
    if(!ret && t && !t.value())
    {
      LLFIO_LOG_FATAL(_v.fd, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::write(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  optional<io_result<const_buffers_type>> ret;
  OUTCOME_TRY(io_state, async_write(reqs, [&ret](async_file_handle *, io_result<const_buffers_type> &result) { ret = result; }));
  (void) io_state;

  // While i/o is not done pump i/o completion
  while(!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
    {
      return t.error();
    }
#ifndef NDEBUG
    if(!ret && t && !t.value())
    {
      LLFIO_LOG_FATAL(_v.fd, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}

LLFIO_V2_NAMESPACE_END
