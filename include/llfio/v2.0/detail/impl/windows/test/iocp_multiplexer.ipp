/* Multiplex file i/o
(C) 2019 Niall Douglas <http://www.nedproductions.biz/> (9 commits)
File Created: Nov 2019


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

#include "../io_handle.ipp"

#ifndef _WIN32
#error This implementation file is for Microsoft Windows only
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace test
{
  template <bool is_threadsafe> class win_iocp_multiplexer final : public io_multiplexer_impl<is_threadsafe>
  {
    using _base = io_multiplexer_impl<is_threadsafe>;
    using _multiplexer_lock_guard = typename _base::_lock_guard;

    using barrier_kind = typename _base::barrier_kind;
    using const_buffers_type = typename _base::const_buffers_type;
    using buffers_type = typename _base::buffers_type;
    using registered_buffer_type = typename _base::registered_buffer_type;
    template <class T> using io_request = typename _base::template io_request<T>;
    template <class T> using io_result = typename _base::template io_result<T>;
    using io_operation_state = typename _base::io_operation_state;
    using io_operation_state_visitor = typename _base::io_operation_state_visitor;
    using check_for_any_completed_io_statistics = typename _base::check_for_any_completed_io_statistics;

    // static constexpr size_t _iocp_operation_state_alignment = (sizeof(void *) == 4) ? 1024 : 2048;
    struct _iocp_operation_state final : public std::conditional_t<is_threadsafe, typename _base::_synchronised_io_operation_state, typename _base::_unsynchronised_io_operation_state>
    {
      using _impl = std::conditional_t<is_threadsafe, typename _base::_synchronised_io_operation_state, typename _base::_unsynchronised_io_operation_state>;

      windows_nt_kernel::IO_STATUS_BLOCK _ols[64];  // 1Kb just on its own

      _iocp_operation_state() = default;
      _iocp_operation_state(_impl &&o) noexcept
          : _impl(std::move(o))
      {
      }
      using _impl::_impl;

      virtual io_operation_state *relocate_to(byte *to_) noexcept override
      {
        auto *to = _impl::relocate_to(to_);
        // restamp the vptr with my own
        new(to) _iocp_operation_state(std::move(*static_cast<_impl *>(to)));
        return to;
      }
    };
    // static_assert(sizeof(_iocp_operation_state) <= _iocp_operation_state_alignment, "_iocp_operation_state alignment is insufficiently large!");

    bool _disable_immediate_completions{false};

  public:
    constexpr win_iocp_multiplexer() {}
    win_iocp_multiplexer(const win_iocp_multiplexer &) = delete;
    win_iocp_multiplexer(win_iocp_multiplexer &&) = delete;
    win_iocp_multiplexer &operator=(const win_iocp_multiplexer &) = delete;
    win_iocp_multiplexer &operator=(win_iocp_multiplexer &&) = delete;
    virtual ~win_iocp_multiplexer()
    {
      if(this->_v)
      {
        (void) win_iocp_multiplexer::close();
      }
    }
    result<void> init(size_t threads, bool disable_immediate_completions)
    {
      this->_v.h = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, (DWORD) threads);
      if(nullptr == this->_v.h)
      {
        return win32_error();
      }
      _disable_immediate_completions = disable_immediate_completions;
      this->_v.behaviour |= native_handle_type::disposition::multiplexer;
      return success();
    }

    // virtual result<path_type> current_path() const noexcept override;
    virtual result<void> close() noexcept override
    {
#ifndef NDEBUG
      if(this->_v)
      {
        // Tell handle::close() that we have correctly executed
        this->_v.behaviour |= native_handle_type::disposition::_child_close_executed;
      }
#endif
      return _base::close();
    }
    // virtual native_handle_type release() noexcept override { return _base::release(); }

    virtual result<uint8_t> do_io_handle_register(io_handle *h) noexcept override
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      LLFIO_LOG_FUNCTION_CALL(this);
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_COMPLETION_INFORMATION fci{};
      memset(&fci, 0, sizeof(fci));
      fci.Port = this->_v.h;
      fci.Key = nullptr;
      NTSTATUS ntstat = NtSetInformationFile(h->native_handle().h, &isb, &fci, sizeof(fci), FileCompletionInformation);
      if(STATUS_PENDING == ntstat)
      {
        ntstat = ntwait(h->native_handle().h, isb, deadline());
      }
      if(ntstat < 0)
      {
        return ntkernel_error(ntstat);
      }
      if(_disable_immediate_completions)
      {
        return success();
      }
      // If this works, we can avoid IOCP entirely for immediately completing i/o
      // It'll set native_handle_type::disposition::_multiplexer_state_bit0 if
      // we successfully executed this
      return SetFileCompletionNotificationModes(h->native_handle().h, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE) ? (uint8_t) 1 : (uint8_t) 0;
    }
    virtual result<void> do_io_handle_deregister(io_handle *h) noexcept override
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      LLFIO_LOG_FUNCTION_CALL(this);
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_COMPLETION_INFORMATION fci{};
      memset(&fci, 0, sizeof(fci));
      fci.Port = nullptr;
      fci.Key = nullptr;
      NTSTATUS ntstat = NtSetInformationFile(h->native_handle().h, &isb, &fci, sizeof(fci), FileReplaceCompletionInformation);
      if(STATUS_PENDING == ntstat)
      {
        ntstat = ntwait(h->native_handle().h, isb, deadline());
      }
      if(ntstat < 0)
      {
        return ntkernel_error(ntstat);
      }
      return success();
    }
    // virtual size_t do_io_handle_max_buffers(const io_handle *h) const noexcept override {}
    // virtual result<registered_buffer_type> do_io_handle_allocate_registered_buffer(io_handle *h, size_t &bytes) noexcept override {}
    virtual std::pair<size_t, size_t> io_state_requirements() noexcept override { return {sizeof(_iocp_operation_state), alignof(_iocp_operation_state)}; }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<buffers_type> reqs) noexcept override
    {
      assert(storage.size() >= sizeof(_iocp_operation_state));
      // assert(((uintptr_t) storage.data() & (_iocp_operation_state_alignment - 1)) == 0);
      if(storage.size() < sizeof(_iocp_operation_state) /*|| ((uintptr_t) storage.data() & (_iocp_operation_state_alignment - 1)) != 0*/)
      {
        return nullptr;
      }
      return new(storage.data()) _iocp_operation_state(_h, _visitor, std::move(b), d, std::move(reqs));
    }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs) noexcept override
    {
      assert(storage.size() >= sizeof(_iocp_operation_state));
      // assert(((uintptr_t) storage.data() & (_iocp_operation_state_alignment - 1)) == 0);
      if(storage.size() < sizeof(_iocp_operation_state) /*|| ((uintptr_t) storage.data() & (_iocp_operation_state_alignment - 1)) != 0*/)
      {
        return nullptr;
      }
      return new(storage.data()) _iocp_operation_state(_h, _visitor, std::move(b), d, std::move(reqs));
    }
    virtual io_operation_state *construct(span<byte> storage, io_handle *_h, io_operation_state_visitor *_visitor, registered_buffer_type &&b, deadline d, io_request<const_buffers_type> reqs, barrier_kind kind) noexcept override
    {
      assert(storage.size() >= sizeof(_iocp_operation_state));
      // assert(((uintptr_t) storage.data() & (_iocp_operation_state_alignment - 1)) == 0);
      if(storage.size() < sizeof(_iocp_operation_state) /*|| ((uintptr_t) storage.data() & (_iocp_operation_state_alignment - 1)) != 0*/)
      {
        return nullptr;
      }
      return new(storage.data()) _iocp_operation_state(_h, _visitor, std::move(b), d, std::move(reqs), kind);
    }
    virtual io_operation_state_type init_io_operation(io_operation_state *_op) noexcept override
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      LLFIO_LOG_FUNCTION_CALL(this);
      auto *state = static_cast<_iocp_operation_state *>(_op);
      switch(state->state)
      {
      case io_operation_state_type::unknown:
        abort();
      case io_operation_state_type::read_initialised:
      {
        io_handle::io_result<io_handle::buffers_type> ret(state->payload.noncompleted.params.read.reqs.buffers);
        if(do_read_write<false>(ret, NtReadFile, state->h->native_handle(), state, state->_ols, state->payload.noncompleted.params.read.reqs, state->payload.noncompleted.d))
        {
          // Completed immediately
          const bool failed = !ret;
          state->read_completed(std::move(ret).value());
          if(failed || state->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            state->read_finished();
            return io_operation_state_type::read_finished;
          }
          return io_operation_state_type::read_completed;
        }
        else
        {
          state->read_initiated();
          return io_operation_state_type::read_initiated;
        }
        break;
      }
      case io_operation_state_type::write_initialised:
      {
        io_handle::io_result<io_handle::const_buffers_type> ret(state->payload.noncompleted.params.write.reqs.buffers);
        if(do_read_write<false>(ret, NtWriteFile, state->h->native_handle(), state, state->_ols, state->payload.noncompleted.params.write.reqs, state->payload.noncompleted.d))
        {
          // Completed immediately
          const bool failed = !ret;
          state->write_completed(std::move(ret).value());
          if(failed || state->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            state->write_or_barrier_finished();
            return io_operation_state_type::write_or_barrier_finished;
          }
          return io_operation_state_type::write_or_barrier_completed;
        }
        else
        {
          state->write_initiated();
          return io_operation_state_type::write_initiated;
        }
        break;
      }
      case io_operation_state_type::barrier_initialised:
      {
        state->barrier_completed(errc::operation_not_supported);  // barrier requires work to implement :)
        return io_operation_state_type::write_or_barrier_finished;
      }
      case io_operation_state_type::read_initiated:
      case io_operation_state_type::read_completed:
      case io_operation_state_type::read_finished:
      case io_operation_state_type::write_initiated:
      case io_operation_state_type::barrier_initiated:
      case io_operation_state_type::write_or_barrier_completed:
      case io_operation_state_type::write_or_barrier_finished:
        assert(false);
        break;
      }
      return state->state;
    }
    // virtual result<void> flush_inited_io_operations() noexcept { return success(); }
    template <class U> io_operation_state_type _check_io_operation(_iocp_operation_state *state, U &&f) noexcept
    {
      auto fill_io_result = [&](auto &ret, auto &params) -> bool {
        for(size_t n = 0; n < params.reqs.buffers.size(); n++)
        {
          if(state->_ols[n].Status == STATUS_PENDING)
          {
            f(state->_ols[n]);
            break;
          }
        }
        if(state->state == io_operation_state_type::unknown || is_completed(state->state) || is_finished(state->state))
        {
          return false;
        }
        for(size_t n = 0; n < params.reqs.buffers.size(); n++)
        {
          assert(state->_ols[n].Status != -1);
          if(state->_ols[n].Status == STATUS_PENDING)
          {
            return false;
          }
          if(state->_ols[n].Status < 0)
          {
            ret = ntkernel_error(static_cast<NTSTATUS>(state->_ols[n].Status));
            break;
          }
          params.reqs.buffers[n] = {params.reqs.buffers[n].data(), state->_ols[n].Information};
          if(params.reqs.buffers[n].size() != 0)
          {
            ret = {params.reqs.buffers.data(), n + 1};
          }
        }
        return true;
      };
      auto v = state->current_state();
      switch(v)
      {
      case io_operation_state_type::read_initialised:
      case io_operation_state_type::read_initiated:
      {
        io_result<buffers_type> ret = {state->payload.noncompleted.params.read.reqs.buffers.data(), 0};
        if(fill_io_result(ret, state->payload.noncompleted.params.read))
        {
          state->read_completed(std::move(ret));
          if(state->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            state->read_finished();
            return io_operation_state_type::read_finished;
          }
          return io_operation_state_type::read_completed;
        }
        return state->current_state();
      }
      case io_operation_state_type::write_initialised:
      case io_operation_state_type::write_initiated:
      {
        io_result<const_buffers_type> ret = {state->payload.noncompleted.params.write.reqs.buffers.data(), 0};
        if(fill_io_result(ret, state->payload.noncompleted.params.write))
        {
          state->write_completed(std::move(ret));
          if(state->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            state->write_or_barrier_finished();
            return io_operation_state_type::write_or_barrier_finished;
          }
          return io_operation_state_type::write_or_barrier_completed;
        }
        return state->current_state();
      }
      case io_operation_state_type::barrier_initialised:
      case io_operation_state_type::barrier_initiated:
      {
        io_result<const_buffers_type> ret = {state->payload.noncompleted.params.barrier.reqs.buffers.data(), 0};
        if(fill_io_result(ret, state->payload.noncompleted.params.barrier))
        {
          state->barrier_completed(std::move(ret));
          if(state->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            state->write_or_barrier_finished();
            return io_operation_state_type::write_or_barrier_finished;
          }
          return io_operation_state_type::write_or_barrier_completed;
        }
        return state->current_state();
      }
      default:
        break;
      }
      return v;
    }
    virtual io_operation_state_type check_io_operation(io_operation_state *_op) noexcept override
    {
      windows_nt_kernel::init();
      LLFIO_LOG_FUNCTION_CALL(this);
      auto *state = static_cast<_iocp_operation_state *>(_op);
      // On Windows, one can update the STATUS_PENDING in an IO_STATUS_BLOCK by calling NtWaitForSingleObject() on it
      return _check_io_operation(state, [&](windows_nt_kernel::IO_STATUS_BLOCK &ol) {
        LARGE_INTEGER timeout;
        timeout.QuadPart = 0;  // poll, don't wait
        windows_nt_kernel::NtWaitForSingleObject(state->h->native_handle().h, false, &timeout);
        if(ol.Status == STATUS_PENDING)
        {
          // If it still hasn't budged, try poking IOCP
          (void) check_for_any_completed_io(std::chrono::seconds(0), 1);
        }
      });
    }
    virtual result<io_operation_state_type> cancel_io_operation(io_operation_state *_op, deadline d = {}) noexcept override
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      LLFIO_LOG_FUNCTION_CALL(this);
      auto *state = static_cast<_iocp_operation_state *>(_op);
      LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
      typename io_operation_state::lock_guard g(state);
      switch(state->state)
      {
      case io_operation_state_type::unknown:
        return errc::invalid_argument;
      case io_operation_state_type::read_initialised:
      case io_operation_state_type::read_initiated:
      {
        if(do_cancel(state->h->native_handle(), state->_ols, state->payload.noncompleted.params.read.reqs))
        {
          state->_unsynchronised_io_operation_state::read_completed(io_result<buffers_type>(errc::operation_canceled));
          // i/o cancellation always requires IOCP to finish it
        }
        break;
      }
      case io_operation_state_type::read_completed:
      case io_operation_state_type::read_finished:
        break;
      case io_operation_state_type::write_initialised:
      case io_operation_state_type::write_initiated:
      {
        if(do_cancel(state->h->native_handle(), state->_ols, state->payload.noncompleted.params.write.reqs))
        {
          state->_unsynchronised_io_operation_state::write_completed(io_result<const_buffers_type>(errc::operation_canceled));
          // i/o cancellation always requires IOCP to finish it
        }
        break;
      }
      case io_operation_state_type::barrier_initialised:
      case io_operation_state_type::barrier_initiated:
      {
        if(do_cancel(state->h->native_handle(), state->_ols, state->payload.noncompleted.params.barrier.reqs))
        {
          state->_unsynchronised_io_operation_state::barrier_completed(io_result<const_buffers_type>(errc::operation_canceled));
          // i/o cancellation always requires IOCP to finish it
        }
        break;
      }
      case io_operation_state_type::write_or_barrier_completed:
      case io_operation_state_type::write_or_barrier_finished:
        break;
      }
      return state->state;
    }
    virtual result<check_for_any_completed_io_statistics> check_for_any_completed_io(deadline d = std::chrono::seconds(0), size_t max_completions = (size_t) -1) noexcept override
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      LLFIO_LOG_FUNCTION_CALL(this);
      LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
      FILE_IO_COMPLETION_INFORMATION entries[64];
      ULONG filled = 0;
      NTSTATUS ntstat;
      if(max_completions == 0)
      {
        return success();
      }
      else if(max_completions == 1)
      {
        // NtRemoveIoCompletion is markedly quicker than NtRemoveIoCompletionEx,
        // so if it is just a single scheduled op don't pay the extra cost
        ntstat = NtRemoveIoCompletion(this->_v.h, &entries[0].KeyContext, &entries[0].ApcContext, &entries[0].IoStatusBlock, timeout);
        if(ntstat >= 0)
        {
          filled = 1;
        }
      }
      else
      {
        ntstat = NtRemoveIoCompletionEx(this->_v.h, entries, sizeof(entries) / sizeof(entries[0]), &filled, timeout, false);
      }
      if(ntstat < 0 && ntstat != STATUS_TIMEOUT)
      {
        return ntkernel_error(ntstat);
      }
      if(filled == 0 || ntstat == STATUS_TIMEOUT)
      {
        return success();
      }
      check_for_any_completed_io_statistics stats;
      for(ULONG n = 0; n < filled; n++)
      {
        // The context is the i/o state
        auto *state = (_iocp_operation_state *) entries[n].ApcContext;
        if(state == nullptr)
        {
          // wake_check_for_any_completed_io() poke
          continue;
        }
        auto s = state->current_state();
        // std::cout << "Coroutine " << state << " before check has state " << (int) s << std::endl;
        s = _check_io_operation(state, [&](windows_nt_kernel::IO_STATUS_BLOCK &) {});
        // std::cout << "Coroutine " << state << " after check has state " << (int) s << std::endl;
        if(is_completed(s))
        {
          switch(state->state)
          {
          case io_operation_state_type::read_completed:
            state->read_finished();
            s = io_operation_state_type::read_finished;
            break;
          case io_operation_state_type::write_or_barrier_completed:
            state->write_or_barrier_finished();
            s = io_operation_state_type::write_or_barrier_finished;
            break;
          default:
            break;
          }
        }
        if(is_completed(s))
        {
          ++stats.initiated_ios_completed;
        }
        else if(is_finished(s))
        {
          ++stats.initiated_ios_finished;
        }
      }
      return stats;
    }
    virtual result<void> wake_check_for_any_completed_io() noexcept override
    {
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      LLFIO_LOG_FUNCTION_CALL(this);
      NTSTATUS ntstat = NtSetIoCompletion(this->_v.h, 0, nullptr, 0, 0);
      if(ntstat < 0)
      {
        return ntkernel_error(ntstat);
      }
      return success();
    }
  };

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<io_multiplexer_ptr> multiplexer_win_iocp(size_t threads, bool disable_immediate_completions) noexcept
  {
    try
    {
      if(1 == threads)
      {
        auto ret = std::make_unique<win_iocp_multiplexer<false>>();
        OUTCOME_TRY(ret->init(1, disable_immediate_completions));
        return ret;
      }
      auto ret = std::make_unique<win_iocp_multiplexer<true>>();
      OUTCOME_TRY(ret->init(threads, disable_immediate_completions));
      return ret;
    }
    catch(...)
    {
      return error_from_exception();
    }
  }
}  // namespace test

LLFIO_V2_NAMESPACE_END
