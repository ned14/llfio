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
  return file_handle::barrier(reqs, wait_for_device, and_metadata, d);
}

template <class BuffersType, class IORoutine> result<async_file_handle::io_state_ptr> async_file_handle::_begin_io(span<char> mem, async_file_handle::operation_t operation, async_file_handle::io_request<BuffersType> reqs, async_file_handle::_erased_completion_handler &&completion, IORoutine &&ioroutine) noexcept
{
  // Need to keep a set of OVERLAPPED matching the scatter-gather buffers
  struct state_type : public _erased_io_state_type
  {
    OVERLAPPED ols[1];
    _erased_completion_handler *completion;
    state_type(async_file_handle *_parent, operation_t _operation, bool must_deallocate_self, size_t _items)  // NOLINT
    : _erased_io_state_type(_parent, _operation, must_deallocate_self, _items),
      completion(nullptr)
    {
    }
    state_type(state_type &&) = delete;
    state_type(const state_type &) = delete;
    state_type &operator=(state_type &&) = delete;
    state_type &operator=(const state_type &) = delete;
    AFIO_HEADERS_ONLY_VIRTUAL_SPEC _erased_completion_handler *erased_completion_handler() noexcept override final { return completion; }
    AFIO_HEADERS_ONLY_VIRTUAL_SPEC void _system_io_completion(long errcode, long bytes_transferred, void *internal_state) noexcept override final
    {
      auto ol = static_cast<LPOVERLAPPED>(internal_state);
      ol->hEvent = nullptr;
      auto &result = this->result.write;
      if(result)
      {
        if(errcode)
        {
          result = error_info{errcode, std::system_category()};
        }
        else
        {
          // Figure out which i/o I am and update the buffer in question
          size_t idx = ol - ols;
          if(idx >= this->items)
          {
            AFIO_LOG_FATAL(0, "async_file_handle::io_state::operator() called with invalid index");
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
    AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~state_type() override final
    {
      // Do we need to cancel pending i/o?
      if(this->items_to_go)
      {
        for(size_t n = 0; n < this->items; n++)
        {
          // If this is non-zero, probably this i/o still in flight
          if(ols[n].hEvent)
          {
            CancelIoEx(this->parent->native_handle().h, ols + n);
          }
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
          if(!res.value())
          {
            AFIO_LOG_FATAL(0, "async_file_handle: io_service returns no work when i/o has not completed");
            std::terminate();
          }
#endif
        }
      }
      completion->~_erased_completion_handler();
    }
  } * state;
  extent_type offset = reqs.offset;
  size_t statelen = sizeof(state_type) + (reqs.buffers.size() - 1) * sizeof(OVERLAPPED) + completion.bytes();
  if(!mem.empty() && statelen > mem.size())
  {
    return std::errc::not_enough_memory;
  }
  size_t items(reqs.buffers.size());
  // On Windows i/o must be scheduled on the same thread pumping completion
  if(GetCurrentThreadId() != service()->_threadid)
  {
    return std::errc::operation_not_supported;
  }

  bool must_deallocate_self = false;
  if(mem.empty())
  {
    void *_mem = ::calloc(1, statelen);  // NOLINT
    if(!_mem)
    {
      return std::errc::not_enough_memory;
    }
    mem = {static_cast<char *>(_mem), statelen};
    must_deallocate_self = true;
  }
  io_state_ptr _state(reinterpret_cast<state_type *>(mem.data()));
  new((state = reinterpret_cast<state_type *>(mem.data()))) state_type(this, operation, must_deallocate_self, items);
  state->completion = reinterpret_cast<_erased_completion_handler *>(reinterpret_cast<uintptr_t>(state) + sizeof(state_type) + (reqs.buffers.size() - 1) * sizeof(OVERLAPPED));
  completion.move(state->completion);

  // To be called once each buffer is read
  struct handle_completion
  {
    static VOID CALLBACK Do(DWORD errcode, DWORD bytes_transferred, LPOVERLAPPED ol)
    {
      auto *state = reinterpret_cast<state_type *>(ol->hEvent);
      state->_system_io_completion(errcode, bytes_transferred, ol);
    }
  };
  // Noexcept move the buffers from req into result
  auto &out = state->result.write.value();
  out = std::move(reqs.buffers);
  for(size_t n = 0; n < items; n++)
  {
    LPOVERLAPPED ol = state->ols + n;
    ol->Internal = static_cast<ULONG_PTR>(-1);
    if(_v.is_append_only())
    {
      ol->OffsetHigh = ol->Offset = 0xffffffff;
    }
    else
    {
#ifndef NDEBUG
      if(_v.requires_aligned_io())
      {
        assert((offset & 511) == 0);
      }
#endif
      ol->Offset = offset & 0xffffffff;
      ol->OffsetHigh = (offset >> 32) & 0xffffffff;
    }
    // Use the unused hEvent member to pass through the state
    ol->hEvent = reinterpret_cast<HANDLE>(state);
    offset += out[n].len;
    ++state->items_to_go;
#ifndef NDEBUG
    if(_v.requires_aligned_io())
    {
      assert((reinterpret_cast<uintptr_t>(out[n].data) & 511) == 0);
      assert((out[n].len & 511) == 0);
    }
#endif
    if(!ioroutine(_v.h, const_cast<char *>(out[n].data), static_cast<DWORD>(out[n].len), ol, handle_completion::Do))
    {
      --state->items_to_go;
      state->result.write = {GetLastError(), std::system_category()};
      // Fire completion now if we didn't schedule anything
      if(!n)
      {
        (*state->completion)(state);
      }
      return _state;
    }
    service()->_work_enqueued();
  }
  return _state;
}

result<async_file_handle::io_state_ptr> async_file_handle::_begin_io(span<char> mem, async_file_handle::operation_t operation, io_request<const_buffers_type> reqs, async_file_handle::_erased_completion_handler &&completion) noexcept
{
  switch(operation)
  {
  case operation_t::read:
    return _begin_io(mem, operation, reqs, std::move(completion), ReadFileEx);
  case operation_t::write:
    return _begin_io(mem, operation, reqs, std::move(completion), WriteFileEx);
  case operation_t::fsync_async:
  case operation_t::dsync_async:
  case operation_t::fsync_sync:
  case operation_t::dsync_sync:
    break;
  }
  return std::errc::operation_not_supported;
}

async_file_handle::io_result<async_file_handle::buffers_type> async_file_handle::read(async_file_handle::io_request<async_file_handle::buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  optional<io_result<buffers_type>> ret;
  OUTCOME_TRY(io_state, async_read(reqs, [&ret](async_file_handle *, io_result<buffers_type> &result) { ret = result; }));
  (void) io_state;  // holds i/o open until it completes

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
      AFIO_LOG_FATAL(_v.h, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}

async_file_handle::io_result<async_file_handle::const_buffers_type> async_file_handle::write(async_file_handle::io_request<async_file_handle::const_buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  optional<io_result<const_buffers_type>> ret;
  OUTCOME_TRY(io_state, async_write(reqs, [&ret](async_file_handle *, io_result<const_buffers_type> &result) { ret = result; }));
  (void) io_state;  // holds i/o open until it completes

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
      AFIO_LOG_FATAL(_v.h, "async_file_handle: io_service returns no work when i/o has not completed");
      std::terminate();
    }
#endif
  }
  return *ret;
}


AFIO_V2_NAMESPACE_END
