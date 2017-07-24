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
#if AFIO_USE_POSIX_AIO
#include <aio.h>
#endif

AFIO_V2_NAMESPACE_BEGIN

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::barrier(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, bool /*wait_for_device*/, bool and_metadata, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  io_result<const_buffers_type> ret;
  auto _io_state(_begin_io(and_metadata ? operation_t::fsync : operation_t::dsync, std::move(reqs), [&ret](auto *state) { ret = std::move(state->result); }, nullptr));
  OUTCOME_TRY(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while(!ret.is_ready())
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
      return make_errored_result<const_buffers_type>(t.get_error());
#ifndef NDEBUG
    if(!ret.is_ready() && t && !t.get())
    {
      AFIO_LOG_FATAL(_v.fd, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return ret;
}

result<async_file_handle> async_file_handle::clone(io_service &service) const noexcept
{
  OUTCOME_TRY(v, clone());
  async_file_handle ret(std::move(v));
  ret._service = &service;
  return std::move(ret);
}

template <class CompletionRoutine, class BuffersType, class IORoutine>
result<async_file_handle::io_state_ptr<CompletionRoutine, BuffersType>> async_file_handle::_begin_io(async_file_handle::operation_t operation, async_file_handle::io_request<BuffersType> reqs, CompletionRoutine &&completion, IORoutine && /*ioroutine*/) noexcept
{
  // Need to keep a set of aiocbs matching the scatter-gather buffers
  struct state_type : public _io_state_type<CompletionRoutine, BuffersType>
  {
#if AFIO_USE_POSIX_AIO
    struct aiocb aiocbs[1];
#else
#error todo
#endif
    state_type(async_file_handle *_parent, operation_t _operation, CompletionRoutine &&f, size_t _items)
        : _io_state_type<CompletionRoutine, BuffersType>(_parent, _operation, std::forward<CompletionRoutine>(f), _items)
    {
    }
    virtual void operator()(long errcode, long bytes_transferred, void *internal_state) noexcept override final
    {
#if AFIO_USE_POSIX_AIO
      struct aiocb **_paiocb = (struct aiocb **) internal_state;
      struct aiocb *aiocb = *_paiocb;
      assert(aiocb >= aiocbs && aiocb < aiocbs + this->items);
      *_paiocb = nullptr;
#else
#error todo
#endif
      if(this->result)
      {
        if(errcode)
          this->result = make_errored_result<BuffersType>((int) errcode);
        else
        {
// Figure out which i/o I am and update the buffer in question
#if AFIO_USE_POSIX_AIO
          size_t idx = aiocb - aiocbs;
#else
#error todo
#endif
          if(idx >= this->items)
          {
            AFIO_LOG_FATAL(0, "file_handle::io_state::operator() called with invalid index");
            std::terminate();
          }
          this->result.value()[idx].second = bytes_transferred;
        }
      }
      this->parent->service()->_work_done();
      // Are we done?
      if(!--this->items_to_go)
        this->completion(this);
    }
    virtual ~state_type() override final
    {
      // Do we need to cancel pending i/o?
      if(this->items_to_go)
      {
        for(size_t n = 0; n < this->items; n++)
        {
#if AFIO_USE_POSIX_AIO
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
            AFIO_LOG_FATAL(0, "file_handle: io_service failed");
            std::terminate();
          }
          if(!res.get())
          {
            AFIO_LOG_FATAL(0, "file_handle: io_service returns no work when i/o has not completed");
            std::terminate();
          }
#endif
        }
      }
    }
  } * state;
  extent_type offset = reqs.offset;
  size_t statelen = sizeof(state_type) + (reqs.buffers.size() - 1) * sizeof(struct aiocb), items(reqs.buffers.size());
  using return_type = io_state_ptr<CompletionRoutine, BuffersType>;
#if AFIO_USE_POSIX_AIO && defined(AIO_LISTIO_MAX)
  if(items > AIO_LISTIO_MAX)
    return make_errored_result<return_type>(std::errc::invalid_argument);
#endif
  void *mem = ::calloc(1, statelen);
  if(!mem)
    return make_errored_result<return_type>(std::errc::not_enough_memory);
  return_type _state((_io_state_type<CompletionRoutine, BuffersType> *) mem);
  new((state = (state_type *) mem)) state_type(this, operation, std::forward<CompletionRoutine>(completion), items);
  // Noexcept move the buffers from req into result
  BuffersType &out = state->result.value();
  out = std::move(reqs.buffers);
  for(size_t n = 0; n < items; n++)
  {
#if AFIO_USE_POSIX_AIO
    struct aiocb *aiocb = state->aiocbs + n;
    aiocb->aio_fildes = _v.fd;
    aiocb->aio_offset = offset;
    aiocb->aio_buf = (void *) out[n].first;
    aiocb->aio_nbytes = out[n].second;
    aiocb->aio_sigevent.sigev_notify = SIGEV_NONE;
    aiocb->aio_sigevent.sigev_value.sival_ptr = (void *) state;
    switch(operation)
    {
    case operation_t::read:
      aiocb->aio_lio_opcode = LIO_READ;
      break;
    case operation_t::write:
      aiocb->aio_lio_opcode = LIO_WRITE;
      break;
    case operation_t::fsync:
      aiocb->aio_lio_opcode = LIO_NOP;
      break;
    case operation_t::dsync:
      aiocb->aio_lio_opcode = LIO_NOP;
      break;
    }
#else
#error todo
#endif
    offset += out[n].second;
    ++state->items_to_go;
  }
  int ret = 0;
#if AFIO_USE_POSIX_AIO
  if(service()->using_kqueues())
  {
#if AFIO_COMPILE_KQUEUES
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
    case operation_t::fsync:
    case operation_t::dsync:
      for(size_t n = 0; n < items; n++)
      {
        struct aiocb *aiocb = state->aiocbs + n;
#if defined(__FreeBSD__) || defined(__APPLE__)  // neither of these have fdatasync()
        ret = aio_fsync(O_SYNC, aiocb);
#else
        ret = aio_fsync(operation == operation_t::dsync ? O_DSYNC : O_SYNC, aiocb);
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
    state->result = make_errored_result<BuffersType>(errno);
    state->completion(state);
    return make_result<return_type>(std::move(_state));
  }
  service()->_work_enqueued(items);
  return make_result<return_type>(std::move(_state));
}

template <class CompletionRoutine> result<async_file_handle::io_state_ptr<CompletionRoutine, async_file_handle::buffers_type>> async_file_handle::async_read(async_file_handle::io_request<async_file_handle::buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return _begin_io(operation_t::read, std::move(reqs), [completion = std::forward<CompletionRoutine>(completion)](auto *state) { completion(state->parent, state->result); }, nullptr);
}

template <class CompletionRoutine> result<async_file_handle::io_state_ptr<CompletionRoutine, async_file_handle::const_buffers_type>> async_file_handle::async_write(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  return _begin_io(operation_t::write, std::move(reqs), [completion = std::forward<CompletionRoutine>(completion)](auto *state) { completion(state->parent, state->result); }, nullptr);
}

async_file_handle::io_result<async_file_handle::buffers_type> async_file_handle::read(async_file_handle::io_request<async_file_handle::buffers_type> reqs, deadline d) noexcept
{
  io_result<buffers_type> ret;
  auto _io_state(_begin_io(operation_t::read, std::move(reqs), [&ret](auto *state) { ret = std::move(state->result); }, nullptr));
  OUTCOME_TRY(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while(!ret.is_ready())
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
      return make_errored_result<buffers_type>(t.get_error());
#ifndef NDEBUG
    if(!ret.is_ready() && t && !t.get())
    {
      AFIO_LOG_FATAL(_v.fd, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return ret;
}

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::write(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, deadline d) noexcept
{
  io_result<const_buffers_type> ret;
  auto _io_state(_begin_io(operation_t::write, std::move(reqs), [&ret](auto *state) { ret = std::move(state->result); }, nullptr));
  OUTCOME_TRY(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while(!ret.is_ready())
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
      return make_errored_result<const_buffers_type>(t.get_error());
#ifndef NDEBUG
    if(!ret.is_ready() && t && !t.get())
    {
      AFIO_LOG_FATAL(_v.fd, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return ret;
}

AFIO_V2_NAMESPACE_END
