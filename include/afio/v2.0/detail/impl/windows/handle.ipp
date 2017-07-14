/* A handle to something
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
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

handle::~handle()
{
  if(_v)
  {
    // Call close() below
    auto ret = handle::close();
    if(ret.has_error())
    {
      AFIO_LOG_FATAL(_v.h, "handle::~handle() close failed");
      abort();
    }
  }
}

result<void> handle::close() noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  if(_v)
  {
    if(are_safety_fsyncs_issued())
    {
      if(!FlushFileBuffers(_v.h))
        return make_errored_result<void>(GetLastError());
    }
    if(!CloseHandle(_v.h))
      return make_errored_result<void>(GetLastError());
    _v = native_handle_type();
  }
  return success();
}

result<handle> handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  result<handle> ret(handle(native_handle_type(), _caching, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  if(!DuplicateHandle(GetCurrentProcess(), _v.h, GetCurrentProcess(), &ret.value()._v.h, 0, false, DUPLICATE_SAME_ACCESS))
    return make_errored_result<handle>(GetLastError(), last190(ret.value().path().u8string()));
  return ret;
}

result<void> handle::set_append_only(bool enable) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  // This works only due to special handling in OVERLAPPED later
  if(enable)
  {
    // Set append_only
    _v.behaviour |= native_handle_type::disposition::append_only;
  }
  else
  {
    // Remove append_only
    _v.behaviour &= ~native_handle_type::disposition::append_only;
  }
  return success();
}

result<void> handle::set_kernel_caching(caching caching) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  native_handle_type nativeh;
  handle::mode _mode = mode::none;
  if(is_append_only())
    _mode = mode::append;
  else if(is_writable())
    _mode = mode::write;
  else if(is_readable())
    _mode = mode::read;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, _mode));
  OUTCOME_TRY(attribs, attributes_from_handle_caching_and_flags(nativeh, caching, _flags));
  nativeh.behaviour |= native_handle_type::disposition::file;
  if(INVALID_HANDLE_VALUE == (nativeh.h = ReOpenFile(_v.h, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, attribs)))
    return make_errored_result<void>(GetLastError());
  _v.swap(nativeh);
  if(!CloseHandle(nativeh.h))
    return make_errored_result<void>(GetLastError());
  return success();
}


/************************************* io_handle ****************************************/

template <class BuffersType, class Syscall> inline io_handle::io_result<BuffersType> do_read_write(const native_handle_type &nativeh, Syscall &&syscall, io_handle::io_request<BuffersType> reqs, deadline d) noexcept
{
  if(d && !nativeh.is_overlapped())
    return make_errored_result<BuffersType>(std::errc::not_supported);
  if(reqs.buffers.size() > 64)
    return make_errored_result<BuffersType>(std::errc::argument_list_too_long);

  AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  std::array<OVERLAPPED, 64> _ols;
  memset(_ols.data(), 0, reqs.buffers.size() * sizeof(OVERLAPPED));
  span<OVERLAPPED> ols(_ols.data(), reqs.buffers.size());
  auto ol_it = ols.begin();
  DWORD transferred = 0;
  auto cancel_io = undoer([&] {
    if(nativeh.is_overlapped())
    {
      for(auto &ol : ols)
      {
        CancelIoEx(nativeh.h, &ol);
      }
      for(auto &ol : ols)
      {
        ntwait(nativeh.h, ol, deadline());
      }
    }
  });
  for(auto &req : reqs.buffers)
  {
    OVERLAPPED &ol = *ol_it++;
    ol.Internal = (ULONG_PTR) -1;
    if(nativeh.is_append_only())
      ol.OffsetHigh = ol.Offset = 0xffffffff;
    else
    {
      ol.OffsetHigh = (reqs.offset >> 32) & 0xffffffff;
      ol.Offset = reqs.offset & 0xffffffff;
    }
    if(!syscall(nativeh.h, req.first, (DWORD) req.second, &transferred, &ol) && ERROR_IO_PENDING != GetLastError())
      return make_errored_result<BuffersType>(GetLastError());
    reqs.offset += req.second;
  }
  // If handle is overlapped, wait for completion of each i/o.
  if(nativeh.is_overlapped())
  {
    for(auto &ol : ols)
    {
      deadline nd = d;
      AFIO_WIN_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      if(STATUS_TIMEOUT == ntwait(nativeh.h, ol, nd))
      {
        AFIO_WIN_DEADLINE_TO_TIMEOUT(d);
      }
    }
  }
  cancel_io.dismiss();
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    // It seems the NT kernel is guilty of casting bugs sometimes
    ols[n].Internal = ols[n].Internal & 0xffffffff;
    if(ols[n].Internal != 0)
    {
      return make_errored_result_nt<BuffersType>((NTSTATUS) ols[n].Internal);
    }
    reqs.buffers[n].second = ols[n].InternalHigh;
  }
  return io_handle::io_result<BuffersType>(std::move(reqs.buffers));
}

io_handle::io_result<io_handle::buffers_type> io_handle::read(io_handle::io_request<io_handle::buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  return do_read_write(_v, &ReadFile, std::move(reqs), std::move(d));
}

io_handle::io_result<io_handle::const_buffers_type> io_handle::write(io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  return do_read_write(_v, &WriteFile, std::move(reqs), std::move(d));
}

result<io_handle::extent_guard> io_handle::lock(io_handle::extent_type offset, io_handle::extent_type bytes, bool exclusive, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  if(d && d.nsecs > 0 && !_v.is_overlapped())
    return std::errc::not_supported;
  DWORD flags = exclusive ? LOCKFILE_EXCLUSIVE_LOCK : 0;
  if(d && !d.nsecs)
    flags |= LOCKFILE_FAIL_IMMEDIATELY;
  AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  OVERLAPPED ol;
  memset(&ol, 0, sizeof(ol));
  ol.Internal = (ULONG_PTR) -1;
  ol.OffsetHigh = (offset >> 32) & 0xffffffff;
  ol.Offset = offset & 0xffffffff;
  DWORD bytes_high = !bytes ? MAXDWORD : (DWORD)((bytes >> 32) & 0xffffffff);
  DWORD bytes_low = !bytes ? MAXDWORD : (DWORD)(bytes & 0xffffffff);
  if(!LockFileEx(_v.h, flags, 0, bytes_low, bytes_high, &ol))
  {
    if(ERROR_LOCK_VIOLATION == GetLastError() && d && !d.nsecs)
      return std::errc::timed_out;
    if(ERROR_IO_PENDING != GetLastError())
      return make_errored_result<io_handle::extent_guard>(GetLastError());
  }
  // If handle is overlapped, wait for completion of each i/o.
  if(_v.is_overlapped())
  {
    if(STATUS_TIMEOUT == ntwait(_v.h, ol, d))
    {
      AFIO_WIN_DEADLINE_TO_TIMEOUT(d);
    }
    // It seems the NT kernel is guilty of casting bugs sometimes
    ol.Internal = ol.Internal & 0xffffffff;
    if(ol.Internal != 0)
      return make_errored_result_nt<io_handle::extent_guard>((NTSTATUS) ol.Internal);
  }
  return extent_guard(this, offset, bytes, exclusive);
}

void io_handle::unlock(io_handle::extent_type offset, io_handle::extent_type bytes) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  OVERLAPPED ol;
  memset(&ol, 0, sizeof(ol));
  ol.Internal = (ULONG_PTR) -1;
  ol.OffsetHigh = (offset >> 32) & 0xffffffff;
  ol.Offset = offset & 0xffffffff;
  DWORD bytes_high = !bytes ? MAXDWORD : (DWORD)((bytes >> 32) & 0xffffffff);
  DWORD bytes_low = !bytes ? MAXDWORD : (DWORD)(bytes & 0xffffffff);
  if(!UnlockFileEx(_v.h, 0, bytes_low, bytes_high, &ol))
  {
    if(ERROR_IO_PENDING != GetLastError())
    {
      auto ret = make_errored_result<void>(GetLastError());
      (void) ret;
      AFIO_LOG_FATAL(_v.h, "io_handle::unlock() failed");
      std::terminate();
    }
  }
  // If handle is overlapped, wait for completion of each i/o.
  if(_v.is_overlapped())
  {
    ntwait(_v.h, ol, deadline());
    if(ol.Internal != 0)
    {
      // It seems the NT kernel is guilty of casting bugs sometimes
      ol.Internal = ol.Internal & 0xffffffff;
      auto ret = make_errored_result_nt<void>((NTSTATUS) ol.Internal);
      (void) ret;
      AFIO_LOG_FATAL(_v.h, "io_handle::unlock() failed");
      std::terminate();
    }
  }
}

AFIO_V2_NAMESPACE_END
