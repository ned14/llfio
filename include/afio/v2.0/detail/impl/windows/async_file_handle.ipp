/* A handle to a file
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (7 commits)
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
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::barrier(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  // Pass through the file_handle's implementation, it understands overlapped handles
  return file_handle::barrier(std::move(reqs), wait_for_device, and_metadata, std::move(d));
}

result<async_file_handle> async_file_handle::clone(io_service &service) const noexcept
{
  OUTCOME_TRY(v, clone());
  async_file_handle ret(std::move(v));
  ret._service = &service;
  return std::move(ret);
}

template <class CompletionRoutine, class BuffersType, class IORoutine>
result<async_file_handle::io_state_ptr<CompletionRoutine, BuffersType>> async_file_handle::_begin_io(async_file_handle::operation_t operation, async_file_handle::io_request<BuffersType> reqs, CompletionRoutine &&completion, IORoutine &&ioroutine) noexcept
{
  // Need to keep a set of OVERLAPPED matching the scatter-gather buffers
  struct state_type : public _io_state_type<CompletionRoutine, BuffersType>
  {
    OVERLAPPED ols[1];
    state_type(async_file_handle *_parent, operation_t _operation, CompletionRoutine &&f, size_t _items)
        : _io_state_type<CompletionRoutine, BuffersType>(_parent, _operation, std::forward<CompletionRoutine>(f), _items)
    {
    }
    virtual void operator()(long errcode, long bytes_transferred, void *internal_state) noexcept override final
    {
      LPOVERLAPPED ol = (LPOVERLAPPED) internal_state;
      ol->hEvent = nullptr;
      if(this->result)
      {
        if(errcode)
          this->result = make_errored_result<BuffersType>((DWORD) errcode);
        else
        {
          // Figure out which i/o I am and update the buffer in question
          size_t idx = ol - ols;
          if(idx >= this->items)
          {
            AFIO_LOG_FATAL(0, "async_file_handle::io_state::operator() called with invalid index");
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
          // If this is non-zero, probably this i/o still in flight
          if(ols[n].hEvent)
            CancelIoEx(this->parent->native_handle().h, ols + n);
        }
        // Pump the i/o service until all pending i/o is completed
        while(this->items_to_go)
        {
          auto res = this->parent->service()->run();
          (void) res;
#ifndef NDEBUG
          if(res.has_error())
          {
            AFIO_LOG_FATAL(0, "async_file_handle: io_service failed");
            std::terminate();
          }
          if(!res.get())
          {
            AFIO_LOG_FATAL(0, "async_file_handle: io_service returns no work when i/o has not completed");
            std::terminate();
          }
#endif
        }
      }
    }
  } * state;
  extent_type offset = reqs.offset;
  size_t statelen = sizeof(state_type) + (reqs.buffers.size() - 1) * sizeof(OVERLAPPED), items(reqs.buffers.size());
  using return_type = io_state_ptr<CompletionRoutine, BuffersType>;
  // On Windows i/o must be scheduled on the same thread pumping completion
  if(GetCurrentThreadId() != service()->_threadid)
    return make_errored_result<return_type>(std::errc::operation_not_supported);

  void *mem = ::calloc(1, statelen);
  if(!mem)
    return make_errored_result<return_type>(std::errc::not_enough_memory);
  return_type _state((_io_state_type<CompletionRoutine, BuffersType> *) mem);
  new((state = (state_type *) mem)) state_type(this, operation, std::forward<CompletionRoutine>(completion), items);

  // To be called once each buffer is read
  struct handle_completion
  {
    static VOID CALLBACK Do(DWORD errcode, DWORD bytes_transferred, LPOVERLAPPED ol)
    {
      state_type *state = (state_type *) ol->hEvent;
      (*state)(errcode, bytes_transferred, ol);
    }
  };
  // Noexcept move the buffers from req into result
  BuffersType &out = state->result.value();
  out = std::move(reqs.buffers);
  for(size_t n = 0; n < items; n++)
  {
    LPOVERLAPPED ol = state->ols + n;
    ol->Internal = (ULONG_PTR) -1;
    if(_v.is_append_only())
      ol->OffsetHigh = ol->Offset = 0xffffffff;
    else
    {
      ol->Offset = offset & 0xffffffff;
      ol->OffsetHigh = (offset >> 32) & 0xffffffff;
    }
    // Use the unused hEvent member to pass through the state
    ol->hEvent = (HANDLE) state;
    offset += out[n].second;
    ++state->items_to_go;
    if(!ioroutine(_v.h, out[n].first, (DWORD) out[n].second, ol, handle_completion::Do))
    {
      --state->items_to_go;
      state->result = make_errored_result<BuffersType>(GetLastError());
      // Fire completion now if we didn't schedule anything
      if(!n)
        state->completion(state);
      return make_result<return_type>(std::move(_state));
    }
    service()->_work_enqueued();
  }
  return make_result<return_type>(std::move(_state));
}

template <class CompletionRoutine> result<async_file_handle::io_state_ptr<CompletionRoutine, async_file_handle::buffers_type>> async_file_handle::async_read(async_file_handle::io_request<async_file_handle::buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  return _begin_io(operation_t::read, std::move(reqs), [completion = std::forward<CompletionRoutine>(completion)](auto *state) { completion(state->parent, state->result); }, ReadFileEx);
}

template <class CompletionRoutine> result<async_file_handle::io_state_ptr<CompletionRoutine, async_file_handle::const_buffers_type>> async_file_handle::async_write(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, CompletionRoutine &&completion) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  return _begin_io(operation_t::write, std::move(reqs), [completion = std::forward<CompletionRoutine>(completion)](auto *state) { completion(state->parent, state->result); }, WriteFileEx);
}

async_file_handle::io_result<async_file_handle::buffers_type> async_file_handle::read(async_file_handle::io_request<async_file_handle::buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  optional<io_result<buffers_type>> ret;
  auto _io_state(_begin_io(operation_t::read, std::move(reqs), [&ret](auto *state) { ret = std::move(state->result); }, ReadFileEx));
  OUTCOME_TRY(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while(!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
      return t.error();
#ifndef NDEBUG
    if(!ret && t && !t.value())
    {
      AFIO_LOG_FATAL(_v.h, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::write(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  optional<io_result<const_buffers_type>> ret;
  auto _io_state(_begin_io(operation_t::write, std::move(reqs), [&ret](auto *state) { ret = std::move(state->result); }, WriteFileEx));
  OUTCOME_TRY(io_state, _io_state);

  // While i/o is not done pump i/o completion
  while(!ret)
  {
    auto t(_service->run_until(d));
    // If i/o service pump failed or timed out, cancel outstanding i/o and return
    if(!t)
      return t.error();
#ifndef NDEBUG
    if(!ret && t && !t.value())
    {
      AFIO_LOG_FATAL(_v.h, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}


AFIO_V2_NAMESPACE_END
