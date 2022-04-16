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

#include "../../../byte_io_handle.hpp"
#include "import.hpp"

#include <WinSock2.h>
#include <ws2ipdef.h>

LLFIO_V2_NAMESPACE_BEGIN

size_t byte_io_handle::_do_max_buffers() const noexcept
{
  if(is_socket())
  {
    /* On Windows we make use of the ApcContext parameter in NtReadFile/NtWriteFile to pass through the operation state
    to IOCP which is not a facility WSASend provides, so if an i/o multiplexer is set, we do each scatter-gather buffer
    individually.

    Note that a non-test IOCP multiplexer would override max_buffers() in any case so this code would never get called.
    */
    return 64;
  }
  return 1;  // TODO FIXME support ReadFileScatter/WriteFileGather
}

template <class BuffersType>
inline bool do_cancel(const native_handle_type &nativeh, span<windows_nt_kernel::IO_STATUS_BLOCK> ols, byte_io_handle::io_request<BuffersType> reqs) noexcept
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
inline bool do_read_write(byte_io_handle::io_result<BuffersType> &ret, Syscall &&syscall, const native_handle_type &nativeh,
                          byte_io_multiplexer::io_operation_state *state, span<windows_nt_kernel::IO_STATUS_BLOCK> ols,
                          byte_io_handle::io_request<BuffersType> reqs, deadline d) noexcept
{
  using namespace windows_nt_kernel;
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
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
  auto cancel_io = make_scope_exit(
  [&]() noexcept
  {
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
        auto r = [&]() -> result<void>
        {
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
#ifndef LLFIO_EXCLUDE_NETWORKING
// We use a separate implementation here so for non-multiplexed as-if blocking socket i/o so
// we get proper atomic scatter-gather i/o. A proper IOCP multiplexer would need to implement
// the usual tricks of appending to OVERLAPPED etc to store the i/o state pointer, and not
// be naughty like our test IOCP multiplexer is by misusing the ApcContext parameter.
//
// Returns true if operation completed immediately
template <bool blocking, class Syscall, class Flags, class BuffersType>
inline bool do_read_write(byte_io_handle::io_result<BuffersType> &ret, Syscall &&syscall, const native_handle_type &nativeh, span<WSABUF> bufs, Flags flags,
                          byte_io_handle::io_request<BuffersType> reqs, deadline d) noexcept
{
  LLFIO_DEADLINE_TO_SLEEP_INIT(d);
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    bufs[n].buf = (char *) reqs.buffers[n].data();
    bufs[n].len = (ULONG) reqs.buffers[n].size();
  }
retry:
  DWORD transferred = 0;
  WSAOVERLAPPED ol;
  memset(&ol, 0, sizeof(ol));
  ol.Internal = (ULONG_PTR) -1;
  const bool op_errored = (SOCKET_ERROR == syscall(nativeh.sock, bufs.data(), (DWORD) reqs.buffers.size(), &transferred, flags, d ? &ol : nullptr, nullptr));
  // It would seem that sometimes Winsock returns zero bytes transferred and no error for nonblocking overlapped sockets
  // In fairness, the Windows documentation does warn in the strongest possible terms to not combine nonblocking semantics
  // with overlapped i/o, however it is so very much easier for this implementation if we do.
  if(op_errored || (transferred == 0 && nativeh.is_nonblocking() && blocking))
  {
    bool io_pending = false;
    if(op_errored)
    {
      auto retcode = WSAGetLastError();
      if(WSA_IO_PENDING == retcode)
      {
        io_pending = true;
      }
      if(!io_pending && WSAEWOULDBLOCK != retcode)
      {
        if(WSAESHUTDOWN == retcode)
        {
          // Emulate POSIX here
          transferred = 0;
          goto exit_now;
        }
        ret = win32_error(retcode);
        return true;
      }
    }
    if(nativeh.is_nonblocking() && blocking)
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      if(io_pending)
      {
        if(STATUS_TIMEOUT == ntwait(nativeh.h, ol, nd))
        {
          // ntwait cancels the i/o, undoer will cancel all the other i/o
          auto r = [&]() -> result<void>
          {
            LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
            return success();
          }();
          if(!r)
          {
            ret = std::move(r).error();
            return true;
          }
        }
        DWORD flags_ = 0;
        if(!WSAGetOverlappedResult(nativeh.sock, &ol, &transferred, false, &flags_))
        {
          ret = win32_error(WSAGetLastError());
          return true;
        }
      }
      else
      {
        // i/o was never started, so poll
        static constexpr bool is_write = std::is_const<typename BuffersType::value_type>::value;
        pollfd fds;
        fds.fd = nativeh.sock;
        fds.events = is_write ? POLLOUT : POLLIN;
        fds.revents = 0;
        int timeout = -1;
        if(nd)
        {
          std::chrono::milliseconds ms;
          if(nd.steady)
          {
            ms =
            std::chrono::duration_cast<std::chrono::milliseconds>((began_steady + std::chrono::nanoseconds((nd).nsecs)) - std::chrono::steady_clock::now());
          }
          else
          {
            ms = std::chrono::duration_cast<std::chrono::milliseconds>(nd.to_time_point() - std::chrono::system_clock::now());
          }
          if(ms.count() < 0)
          {
            timeout = 0;
          }
          else
          {
            timeout = (int) ms.count();
          }
        }
        if(SOCKET_ERROR == WSAPoll(&fds, 1, timeout))
        {
          ret = win32_error(WSAGetLastError());
          return true;
        }
        auto r = [&]() -> result<void>
        {
          LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
          return success();
        }();
        if(!r)
        {
          ret = std::move(r).error();
          return true;
        }
        goto retry;
      }
    }
  }
  if(!blocking)
  {
    // If all the operations already completed, great
    if(ol.Internal == static_cast<ULONG_PTR>(0x103 /*STATUS_PENDING*/))
    {
      return false;  // at least one buffer is not completed yet
    }
  }
exit_now:
  for(size_t i = 0; i < reqs.buffers.size(); i++)
  {
    auto &buffer = reqs.buffers[i];
    if(buffer.size() <= static_cast<size_t>(transferred))
    {
      transferred -= (DWORD) buffer.size();
    }
    else
    {
      buffer = {buffer.data(), (size_t) transferred};
      ret = {reqs.buffers.data(), i + 1};
      break;
    }
  }
  return true;
}

result<size_t> poll(span<poll_what> out, span<pollable_handle *> handles, span<const poll_what> query, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(nullptr);
  if(out.size() != handles.size())
  {
    return errc::invalid_argument;
  }
  if(out.size() != query.size())
  {
    return errc::invalid_argument;
  }
  if(handles.empty())
  {
    return 0;
  }
  if(handles.size() > 1024)
  {
    return errc::argument_out_of_domain;
  }
  LLFIO_DEADLINE_TO_SLEEP_INIT(d);
  auto *fds = (WSAPOLLFD *) alloca(handles.size() * sizeof(WSAPOLLFD));
  memset(fds, 0, handles.size() * sizeof(WSAPOLLFD));
  ULONG fdscount = 0;
  for(size_t n = 0; n < handles.size(); n++)
  {
    if(handles[n] != nullptr)
    {
      auto &h_ = handles[n]->_get_handle();
      auto &h = h_.native_handle().is_third_party_pointer() ? *(handle *) h_.native_handle().ptr : h_;
      if(h.is_kernel_handle())
      {
        fds[fdscount].fd = h.native_handle().sock;
        if(query[n] & poll_what::is_readable)
        {
          fds[fdscount].events |= POLLIN;
        }
        if(query[n] & poll_what::is_writable)
        {
          fds[fdscount].events |= POLLOUT;
        }
        fdscount++;
      }
    }
  }
  if(0 == fdscount)
  {
    return 0;
  }
  for(;;)
  {
    int timeout = -1;
    if(d)
    {
      std::chrono::milliseconds ms;
      if(d.steady)
      {
        ms = std::chrono::duration_cast<std::chrono::milliseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());
      }
      else
      {
        ms = std::chrono::duration_cast<std::chrono::milliseconds>(d.to_time_point() - std::chrono::system_clock::now());
      }
      if(ms.count() < 0)
      {
        timeout = 0;
      }
      else
      {
        timeout = (int) ms.count();
      }
    }
    if(timeout < 0 || timeout > 1000)
    {
      /* There is some weird race in Windows' WSAPoll where if thread A connects a socket
      being polled by thread B to a listening socket being polled in thread C then it
      can occasionally hang forever. We work around this by breaking and retrying the poll
      no longer than every second to let i/o proceed.
      */
      timeout = 1000;
    }
    auto ret = WSAPoll(fds, fdscount, timeout);
    if(SOCKET_ERROR == ret)
    {
      return win32_error(WSAGetLastError());
    }
    if(ret > 0)
    {
      auto count = ret;
      fdscount = 0;
      for(size_t n = 0; n < handles.size(); n++)
      {
        if(handles[n] != nullptr)
        {
          auto &h_ = handles[n]->_get_handle();
          auto &h = h_.native_handle().is_third_party_pointer() ? *(handle *) h_.native_handle().ptr : h_;
          if(h.is_kernel_handle())
          {
            if(fds[fdscount].revents != 0)
            {
              if(fds[fdscount].revents & POLLIN)
              {
                out[n] |= poll_what::is_readable;
              }
              if(fds[fdscount].revents & POLLOUT)
              {
                out[n] |= poll_what::is_writable;
              }
              if(fds[fdscount].revents & POLLERR)
              {
                out[n] |= poll_what::is_errored;
              }
              if(fds[fdscount].revents & POLLHUP)
              {
                out[n] |= poll_what::is_closed;
              }
              if(--count == 0)
              {
                return ret;
              }
            }
          }
          else
          {
            out[n] |= poll_what::not_pollable;
          }
          fdscount++;
        }
      }
      return ret;
    }
    LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
  }
}
#endif

byte_io_handle::io_result<byte_io_handle::buffers_type> byte_io_handle::_do_read(byte_io_handle::io_request<byte_io_handle::buffers_type> reqs,
                                                                                 deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  byte_io_handle::io_result<byte_io_handle::buffers_type> ret(reqs.buffers);
  if(d && !_v.is_nonblocking())
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > 64)
  {
    return errc::argument_list_too_long;
  }
  if(is_socket())
  {
#ifdef LLFIO_EXCLUDE_NETWORKING
    return errc::not_supported;
#else
    std::array<WSABUF, 64> bufs;
    DWORD flags = 0;
    do_read_write<true>(ret, WSARecv, _v, bufs, &flags, reqs, d);
    return ret;
#endif
  }
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  std::array<EIOSB, 64> _ols{};
  do_read_write<true>(ret, NtReadFile, _v, nullptr, {_ols.data(), _ols.size()}, reqs, d);
  return ret;
}

byte_io_handle::io_result<byte_io_handle::const_buffers_type> byte_io_handle::_do_write(byte_io_handle::io_request<byte_io_handle::const_buffers_type> reqs,
                                                                                        deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  byte_io_handle::io_result<byte_io_handle::const_buffers_type> ret(reqs.buffers);
  if(d && !_v.is_nonblocking())
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > 64)
  {
    return errc::argument_list_too_long;
  }
  if(is_socket())
  {
#ifdef LLFIO_EXCLUDE_NETWORKING
    return errc::not_supported;
#else
    std::array<WSABUF, 64> bufs;
    DWORD flags = 0;
    do_read_write<true>(ret, WSASend, _v, bufs, flags, reqs, d);
    return ret;
#endif
  }
  using EIOSB = windows_nt_kernel::IO_STATUS_BLOCK;
  std::array<EIOSB, 64> _ols{};
  do_read_write<true>(ret, NtWriteFile, _v, nullptr, {_ols.data(), _ols.size()}, reqs, d);
  return ret;
}

byte_io_handle::io_result<byte_io_handle::const_buffers_type> byte_io_handle::_do_barrier(byte_io_handle::io_request<byte_io_handle::const_buffers_type> reqs,
                                                                                          barrier_kind kind, deadline d) noexcept
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
