/* A handle to something
(C) 2015-2021 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
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

#include "../../../io_handle.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

size_t io_handle::_do_max_buffers() const noexcept
{
  return 1;  // TODO FIXME support ReadFileScatter/WriteFileGather
}

template <class BuffersType> inline bool do_cancel(const native_handle_type &nativeh, span<windows_nt_kernel::IO_STATUS_BLOCK> ols, io_handle::io_request<BuffersType> reqs) noexcept
{
  using namespace windows_nt_kernel;
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  bool did_cancel = false;
  ols = span<EIOSB>(ols.data(), reqs.buffers.size());
  for(auto &ol : ols)
  {
    if(ol.Status == -1)
    {
      // No need to cancel an i/o never begun
      continue;
    }
    NTSTATUS ntstat = ntcancel_pending_io(nativeh.h, ol);
    if(ntstat < 0 && ntstat != (NTSTATUS) 0xC0000120 /*STATUS_CANCELLED*/)
    {
      LLFIO_LOG_FATAL(nullptr, "Failed to cancel earlier i/o");
      abort();
    }
    if(ntstat == (NTSTATUS) 0xC0000120 /*STATUS_CANCELLED*/)
    {
      did_cancel = true;
    }
  }
  return did_cancel;
}

// Returns true if operation completed immediately
template <bool blocking, class Syscall, class BuffersType>
inline bool do_read_write(io_handle::io_result<BuffersType> &ret, Syscall &&syscall, const native_handle_type &nativeh, io_multiplexer::io_operation_state *state, span<windows_nt_kernel::IO_STATUS_BLOCK> ols, io_handle::io_request<BuffersType> reqs, deadline d) noexcept
{
  using namespace windows_nt_kernel;
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  if(d && !nativeh.is_nonblocking())
  {
    ret = errc::not_supported;
    return true;
  }
  LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  ols = span<EIOSB>(ols.data(), reqs.buffers.size());
  memset(ols.data(), 0, reqs.buffers.size() * sizeof(EIOSB));
  auto ol_it = ols.begin();
  for(auto &req : reqs.buffers)
  {
    (void) req;
    EIOSB &ol = *ol_it++;
    ol.Status = -1;
  }
  auto cancel_io = make_scope_exit([&]() noexcept {
    if(nativeh.is_nonblocking())
    {
      if(ol_it != ols.begin() + 1)
      {
        do_cancel(nativeh, ols, reqs);
      }
    }
  });
  ol_it = ols.begin();
  for(auto &req : reqs.buffers)
  {
    EIOSB &ol = *ol_it++;
    LARGE_INTEGER offset;
    if(nativeh.is_append_only())
    {
      offset.QuadPart = -1;
    }
    else
    {
#ifndef NDEBUG
      if(nativeh.requires_aligned_io())
      {
        assert((reqs.offset & 511) == 0);
      }
#endif
      offset.QuadPart = reqs.offset;
    }
#ifndef NDEBUG
    if(nativeh.requires_aligned_io())
    {
      assert(((uintptr_t) req.data() & 511) == 0);
      // Reads must use aligned length as well
      assert(std::is_const<typename BuffersType::value_type>::value || (req.size() & 511) == 0);
    }
#endif
    reqs.offset += req.size();
    ol.Status = 0x103 /*STATUS_PENDING*/;
    NTSTATUS ntstat = syscall(nativeh.h, nullptr, nullptr, state, &ol, (PVOID) req.data(), static_cast<DWORD>(req.size()), &offset, nullptr);
    if(ntstat < 0 && ntstat != 0x103 /*STATUS_PENDING*/)
    {
      InterlockedCompareExchange(&ol.Status, ntstat, 0x103 /*STATUS_PENDING*/);
      ret = ntkernel_error(ntstat);
      return true;
    }
  }
  // If handle is overlapped, wait for completion of each i/o.
  if(nativeh.is_nonblocking() && blocking)
  {
    for(auto &ol : ols)
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      if(STATUS_TIMEOUT == ntwait(nativeh.h, ol, nd))
      {
        // ntwait cancels the i/o, undoer will cancel all the other i/o
        auto r = [&]() -> result<void> {
          LLFIO_WIN_DEADLINE_TO_TIMEOUT_LOOP(d);
          return success();
        }();
        if(!r)
        {
          ret = std::move(r).error();
          return true;
        }
      }
    }
  }
  cancel_io.release();
  if(!blocking)
  {
    // If all the operations already completed, great
    for(size_t n = 0; n < reqs.buffers.size(); n++)
    {
      if(ols[n].Status == static_cast<ULONG_PTR>(0x103 /*STATUS_PENDING*/))
      {
        return false;  // at least one buffer is not completed yet
      }
    }
  }
  ret = {reqs.buffers.data(), 0};
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    assert(ols[n].Status != -1);
    if(ols[n].Status < 0)
    {
      ret = ntkernel_error(static_cast<NTSTATUS>(ols[n].Status));
      return true;
    }
    reqs.buffers[n] = {reqs.buffers[n].data(), ols[n].Information};
    if(reqs.buffers[n].size() != 0)
    {
      ret = {reqs.buffers.data(), n + 1};
    }
  }
  return true;
}

io_handle::io_result<io_handle::buffers_type> io_handle::_do_read(io_handle::io_request<io_handle::buffers_type> reqs, deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  std::array<EIOSB, 64> _ols{};
  if(reqs.buffers.size() > 64)
  {
    return errc::argument_list_too_long;
  }
  io_handle::io_result<io_handle::buffers_type> ret(reqs.buffers);
  do_read_write<true>(ret, NtReadFile, _v, nullptr, {_ols.data(), _ols.size()}, reqs, d);
  return ret;
}

io_handle::io_result<io_handle::const_buffers_type> io_handle::_do_write(io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  std::array<EIOSB, 64> _ols{};
  if(reqs.buffers.size() > 64)
  {
    return errc::argument_list_too_long;
  }
  io_handle::io_result<io_handle::const_buffers_type> ret(reqs.buffers);
  do_read_write<true>(ret, NtWriteFile, _v, nullptr, {_ols.data(), _ols.size()}, reqs, d);
  return ret;
}

io_handle::io_result<io_handle::const_buffers_type> io_handle::_do_barrier(io_handle::io_request<io_handle::const_buffers_type> reqs, barrier_kind kind, deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  if(d && !_v.is_nonblocking())
  {
    return errc::not_supported;
  }
  LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  EIOSB ol{};
  memset(&ol, 0, sizeof(ol));
  auto *isb = &ol;
  *isb = make_iostatus();
  NTSTATUS ntstat;
  if(NtFlushBuffersFileEx != nullptr)
  {
    ULONG flags = 0;
    switch(kind)
    {
    case barrier_kind::nowait_view_only:
    case barrier_kind::wait_view_only:
    case barrier_kind::nowait_data_only:
    case barrier_kind::wait_data_only:
      flags = 1 /*FLUSH_FLAGS_FILE_DATA_ONLY*/;
      break;
    case barrier_kind::nowait_all:
    case barrier_kind::wait_all:
      flags = 0;
      break;
    }
    if(((uint8_t) kind & 1) == 0)
    {
      flags |= 2 /*FLUSH_FLAGS_NO_SYNC*/;
    }
    ntstat = NtFlushBuffersFileEx(_v.h, flags, nullptr, 0, isb);
  }
  else
  {
    ntstat = NtFlushBuffersFile(_v.h, isb);
  }
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(_v.h, ol, d);
    if(STATUS_TIMEOUT == ntstat)
    {
      return errc::timed_out;
    }
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  return {reqs.buffers};
}

LLFIO_V2_NAMESPACE_END
