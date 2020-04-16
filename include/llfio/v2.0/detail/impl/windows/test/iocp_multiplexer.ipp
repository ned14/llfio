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

#include "../../../../io_multiplexer.hpp"

#ifndef _WIN32
#error This implementation file is for Microsoft Windows only
#endif

#include "../import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

namespace test
{
  template <bool is_threadsafe> struct win_iocp_multiplexer final : io_multiplexer_impl<is_threadsafe>
  {
    using _base = io_multiplexer_impl<is_threadsafe>;
    using _lock_guard = typename _base::_lock_guard;

    explicit win_iocp_multiplexer(size_t threads) { (void) threads; }
    virtual ~win_iocp_multiplexer()
    {
      if(_v)
      {
        (void) win_iocp_multiplexer::close();
      }
    }
    // LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<path_type> current_path() const noexcept override;
    //LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override { return _base::close(); }
    //LLFIO_HEADERS_ONLY_VIRTUAL_SPEC native_handle_type release() noexcept override { return _base::release(); }

    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<uint8_t> do_io_handle_register(io_handle *h) noexcept override
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
      // If this works, we can avoid IOCP entirely for immediately completing i/o
      // It'll set native_handle_type::disposition::_multiplexer_state_bit0 if
      // we successfully executed this
      return SetFileCompletionNotificationModes(h->native_handle().h, FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE) ? (uint8_t) 1 : (uint8_t) 0;
    }
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> do_io_handle_deregister(io_handle *h) noexcept override
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
    // LLFIO_HEADERS_ONLY_VIRTUAL_SPEC size_t do_io_handle_max_buffers(const io_handle *h) const noexcept override {}
    // LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<registered_buffer_type> do_io_handle_allocate_registered_buffer(io_handle *h, size_t &bytes) noexcept override {}
    LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void begin_io_operation(unsynchronised_io_operation_state *op) noexcept override
    {
      if(is_threadsafe)
      {
        // Restamp the vptr of the state to be synchronised
        op = new(op) synchronised_io_operation_state(std::move(*op));
      }
      // On Windows, we can initiate the i/o using a zeroed deadline
      constexpr deadline nonblocking(std::chrono::seconds(0));
      switch(op->state)
      {
      case io_operation_state_type::unknown:
      case io_operation_state_type::read_initiated:
      case io_operation_state_type::read_completed:
      case io_operation_state_type::read_finished:
      case io_operation_state_type::write_initiated:
      case io_operation_state_type::write_or_barrier_completed:
      case io_operation_state_type::write_or_barrier_finished:
      case io_operation_state_type::barrier_initiated:
        abort();
      case io_operation_state_type::read_requested:
      {
        auto res = op->payload.noncompleted.base ? op->h->_do_read(std::move(op->payload.noncompleted.base), std::move(op->payload.noncompleted.params.read.reqs), nonblocking) : op->h->_do_read(std::move(op->payload.noncompleted.params.read.reqs), nonblocking);
        if(res)
        {
          op->read_completed(std::move(res).value());
          if(op->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            op->read_finished();
            return;
          }
        }
        break;
      }
      case io_operation_state_type::write_requested:
      {
        auto res = op->payload.noncompleted.base ? op->h->_do_write(std::move(op->payload.noncompleted.base), std::move(op->payload.noncompleted.params.write.reqs), nonblocking) : op->h->_do_write(std::move(op->payload.noncompleted.params.write.reqs), nonblocking);
        if(res)
        {
          op->write_completed(std::move(res).value());
          if(op->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            op->write_finished();
            return;
          }
        }
        break;
      }
      case io_operation_state_type::barrier_requested:
        auto res = op->h->_do_barrier(std::move(op->payload.noncompleted.params.barrier.reqs), op->payload.noncompleted.params.barrier.kind, nonblocking);
        if(res)
        {
          op->barrier_completed(std::move(res).value());
          if(op->h->native_handle().behaviour & native_handle_type::disposition::_multiplexer_state_bit0)
          {
            // SetFileCompletionNotificationModes() above was successful, so we are done
            op->barrier_finished();
            return;
          }
        }
        break;
      }
      // TODO need to record this to call *_finished() when IOCP is done with it.
    }
    // LLFIO_HEADERS_ONLY_VIRTUAL_SPEC bool check_io_operation(io_operation_state *op) noexcept { return false; }
    // LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<wait_for_completed_io_statistics> check_for_any_completed_io(deadline d = std::chrono::seconds(0), size_t max_completions = (size_t) -1) noexcept { return success(); }
    // LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> wake_check_for_any_completed_io() noexcept { return success(); }
  };

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<io_multiplexer_ptr> multiplexer_win_iocp(size_t threads) noexcept
  {
    if(1 == threads)
    {
      return std::make_unique<win_iocp_multiplexer<false>>(1);
    }
    return std::make_unique<win_iocp_multiplexer<true>>(threads);
  }
}  // namespace test

LLFIO_V2_NAMESPACE_END
