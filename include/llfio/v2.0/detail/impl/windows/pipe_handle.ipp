/* A handle to a pipe
(C) 2015-2019 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#include "../../../pipe_handle.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

result<pipe_handle> pipe_handle::pipe(pipe_handle::path_view_type path, pipe_handle::mode _mode, pipe_handle::creation _creation, pipe_handle::caching _caching, pipe_handle::flag flags, const path_handle & /*unused*/) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<pipe_handle> ret(pipe_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::pipe;
  DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, _mode, flags));
  OUTCOME_TRY(attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  ret.value()._flags |= flag::unlink_on_first_close;
  if(creation::truncate_existing == _creation || creation::always_new == _creation || path.is_ntpath())
  {
    return errc::operation_not_supported;
  }
  path_view::c_str<> zpath(path, false);

  if(zpath.buffer[0] != '\\')
  {
    if(zpath.length + 9 >= zpath.internal_buffer_size)
    {
      return errc::argument_out_of_domain;  // I should try harder here
    }
    wchar_t *_zpathbuffer = const_cast<wchar_t *>(zpath.buffer);
    memmove(_zpathbuffer + 9, _zpathbuffer, (zpath.length + 1) * sizeof(wchar_t));
    memcpy(_zpathbuffer, L"\\\\.\\pipe\\", 9 * sizeof(wchar_t));
  }
  if(creation::open_existing == _creation)
  {
    // Open existing pipe
    DWORD creation = OPEN_EXISTING;
    switch(_creation)
    {
    case creation::open_existing:
      break;
    case creation::only_if_not_exist:
      creation = CREATE_NEW;
      break;
    case creation::if_needed:
      creation = OPEN_ALWAYS;
      break;
    case creation::truncate_existing:
      creation = TRUNCATE_EXISTING;
      break;
    case creation::always_new:
      creation = CREATE_ALWAYS;
      break;
    }
    if(mode::append == _mode)
    {
      access = SYNCHRONIZE | DELETE | GENERIC_WRITE;
    }
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateFileW_(zpath.buffer, access, fileshare, nullptr, creation, attribs, nullptr)))  // NOLINT
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return win32_error(errcode);
    }
  }
  else
  {
    switch(_mode)
    {
    case handle::mode::unchanged:
    case handle::mode::none:
    case handle::mode::attr_read:
    case handle::mode::attr_write:
      break;
    case handle::mode::read:
      attribs |= PIPE_ACCESS_INBOUND;
      break;
    case handle::mode::write:
      attribs |= PIPE_ACCESS_DUPLEX;
      break;
    case handle::mode::append:
      attribs |= PIPE_ACCESS_OUTBOUND;
      break;
    }
    if(creation::only_if_not_exist == _creation)
    {
      attribs |= FILE_FLAG_FIRST_PIPE_INSTANCE;
    }
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateNamedPipe(zpath.buffer, attribs, PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, 65535, 65536, NMPWAIT_USE_DEFAULT_WAIT, nullptr)))  // NOLINT
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return win32_error(errcode);
    }
  }
  if(!nativeh.is_nonblocking())
  {
    if(nativeh.is_readable() && !nativeh.is_writable())
    {
      // Opening blocking pipes for reads must block until other end opens with write
      if(!ConnectNamedPipe(nativeh.h, nullptr))
      {
        return win32_error();
      }
      ret.value()._set_is_connected(true);
    }
  }
  return ret;
}

result<std::pair<pipe_handle, pipe_handle>> pipe_handle::anonymous_pipe(caching _caching, flag flags) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  std::pair<pipe_handle, pipe_handle> ret(pipe_handle(native_handle_type(), 0, 0, _caching, flags), pipe_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &readnativeh = ret.first._v, &writenativeh = ret.second._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  readnativeh.behaviour |= native_handle_type::disposition::pipe;
  readnativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  writenativeh.behaviour |= native_handle_type::disposition::pipe;
  writenativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  if(!CreatePipe(&readnativeh.h, &writenativeh.h, nullptr, 65536))
  {
    DWORD errcode = GetLastError();
    // assert(false);
    return win32_error(errcode);
  }
  ret.first._set_is_connected(true);
  ret.second._set_is_connected(true);
  return ret;
}

static inline result<void> _do_connect(const native_handle_type &nativeh, deadline &d) noexcept
{
  LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  OVERLAPPED ol{};
  memset(&ol, 0, sizeof(ol));
  ol.Internal = static_cast<ULONG_PTR>(-1);
  if(!ConnectNamedPipe(nativeh.h, &ol))
  {
    if(ERROR_IO_PENDING != GetLastError())
    {
      return win32_error();
    }
    if(STATUS_TIMEOUT == ntwait(nativeh.h, ol, d))
    {
      return errc::timed_out;
    }
    // It seems the NT kernel is guilty of casting bugs sometimes
    ol.Internal = ol.Internal & 0xffffffff;
    if(ol.Internal != 0)
    {
      return ntkernel_error(static_cast<NTSTATUS>(ol.Internal));
    }
    if(d.steady)
    {
      auto remaining = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());
      if(remaining.count() < 0)
      {
        remaining = std::chrono::nanoseconds(0);
      }
      d = deadline(remaining);
    }
  }
  return success();
}

pipe_handle::io_result<pipe_handle::buffers_type> pipe_handle::read(pipe_handle::io_request<pipe_handle::buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(!_is_connected())
  {
    OUTCOME_TRY(_do_connect(_v, d));
    _set_is_connected(true);
  }
  return io_handle::read(reqs, d);
}

pipe_handle::io_result<pipe_handle::const_buffers_type> pipe_handle::write(pipe_handle::io_request<pipe_handle::const_buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(!_is_connected())
  {
    OUTCOME_TRY(_do_connect(_v, d));
    _set_is_connected(true);
  }
  return io_handle::write(reqs, d);
}

LLFIO_V2_NAMESPACE_END
