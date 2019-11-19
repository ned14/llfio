/* A handle to something
(C) 2015-2019 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
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

size_t io_handle::max_buffers() const noexcept
{
  return 1;  // async_file_handle may override this virtual function
}

template <class BuffersType, class Syscall> inline io_handle::io_result<BuffersType> do_read_write(const native_handle_type &nativeh, Syscall &&syscall, io_handle::io_request<BuffersType> reqs, deadline d) noexcept
{
  if(d && !nativeh.is_overlapped())
  {
    return errc::not_supported;
  }
  if(reqs.buffers.size() > 64)
  {
    return errc::argument_list_too_long;
  }

  LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  std::array<OVERLAPPED, 64> _ols{};
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
    ol.Internal = static_cast<ULONG_PTR>(-1);
    if(nativeh.is_append_only())
    {
      ol.OffsetHigh = ol.Offset = 0xffffffff;
    }
    else
    {
#ifndef NDEBUG
      if(nativeh.requires_aligned_io())
      {
        assert((reqs.offset & 511) == 0);
      }
#endif
      ol.OffsetHigh = (reqs.offset >> 32) & 0xffffffff;
      ol.Offset = reqs.offset & 0xffffffff;
    }
#ifndef NDEBUG
    if(nativeh.requires_aligned_io())
    {
      assert(((uintptr_t) req.data() & 511) == 0);
      assert((req.size() & 511) == 0);
    }
#endif
    if(!syscall(nativeh.h, req.data(), static_cast<DWORD>(req.size()), &transferred, &ol) && ERROR_IO_PENDING != GetLastError())
    {
      return win32_error();
    }
    reqs.offset += req.size();
  }
  // If handle is overlapped, wait for completion of each i/o.
  if(nativeh.is_overlapped())
  {
    for(auto &ol : ols)
    {
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      if(STATUS_TIMEOUT == ntwait(nativeh.h, ol, nd))
      {
        LLFIO_WIN_DEADLINE_TO_TIMEOUT(d);
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
      return ntkernel_error(static_cast<NTSTATUS>(ols[n].Internal));
    }
    reqs.buffers[n] = {reqs.buffers[n].data(), ols[n].InternalHigh};
  }
  return io_handle::io_result<BuffersType>(std::move(reqs.buffers));
}

io_handle::io_result<io_handle::buffers_type> io_handle::read(io_handle::io_request<io_handle::buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  return do_read_write(_v, &ReadFile, reqs, d);
}

io_handle::io_result<io_handle::const_buffers_type> io_handle::write(io_handle::io_request<io_handle::const_buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  return do_read_write(_v, &WriteFile, reqs, d);
}

LLFIO_V2_NAMESPACE_END
