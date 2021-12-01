/* A handle to a process
(C) 2016-2020 Niall Douglas <http://www.nedproductions.biz/> (5 commits)
File Created: Mar 2017


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

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool process_handle::is_running() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  DWORD retcode = 0;
  if(!GetExitCodeProcess(_v.h, &retcode))
    return false;
  return retcode == STILL_ACTIVE;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle::path_type> process_handle::current_path() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_v.h != GetCurrentProcess())
  {
    // Laziness ...
    return success();
  }
  path_type::string_type buffer(32768, 0);
  DWORD len = GetModuleFileNameW(nullptr, const_cast<path_type::string_type::value_type *>(buffer.data()), (DWORD) buffer.size());
  if(!len)
  {
    return win32_error();
  }
  buffer.resize(len);
  return path_type(std::move(buffer));
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> process_handle::close() noexcept
{
  OUTCOME_TRY(close_pipes());
  if(_flags & flag::wait_on_close)
  {
    OUTCOME_TRY(wait());
  }
  if(_v.h != GetCurrentProcess())
  {
    if(!CloseHandle(_v.h))
    {
      return win32_error();
    }
    _v = {};
  }
  return success();
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle> process_handle::clone() const noexcept
{
  OUTCOME_TRY(auto &&duph, handle::clone());
  return process_handle(std::move(duph));
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC std::unique_ptr<span<path_view_component>, process_handle::_byte_array_deleter> process_handle::environment() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_v.h != GetCurrentProcess())
  {
    // Laziness ...
    return {};
  }
  wchar_t *strings = GetEnvironmentStringsW();
  if(strings == nullptr)
  {
    return {};
  }
  auto unstrings = make_scope_exit([strings]() noexcept { FreeEnvironmentStringsW(strings); });
  auto *s = strings, *e = strings;
  size_t count = 0;
  for(; *s; s = (e = e + 1))
  {
    for(; *e; e++)
    {
    }
    if(*s != '=')
      ++count;
  }
  const size_t bytesneeded = sizeof(span<path_view_component>) + sizeof(path_view_component) * count + (e - strings + 1) * sizeof(wchar_t);
  auto ret = std::make_unique<byte[]>(bytesneeded);
  auto &out = *(span<path_view_component> *) ret.get();
  auto *array = (path_view_component *) (ret.get() + sizeof(span<path_view_component>)), *arraye = array;
  auto *after = (wchar_t *) (ret.get() + sizeof(span<path_view_component>) + sizeof(path_view_component) * count);
  memcpy(after, strings, (e - strings + 1) * sizeof(wchar_t));
  out = {array, arraye};
  s = e = after;
  for(; *s; s = (e = e + 1))
  {
    for(; *e; e++)
    {
    }
    if(*s != '=')
    {
      *arraye++ = path_view_component(s, e, path_view::zero_terminated);
      out = {array, arraye};
    }
  }
  return std::unique_ptr<span<path_view_component>, process_handle::_byte_array_deleter>((span<path_view_component> *) ret.release());
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<intptr_t> process_handle::wait(deadline d) const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  for(;;)
  {
    NTSTATUS ntstat = NtWaitForSingleObject(_v.h, false, &_timeout);
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
    DWORD retcode = 0;
    if(!GetExitCodeProcess(_v.h, &retcode))
      return win32_error();
    if(retcode != STILL_ACTIVE)
    {
      return (intptr_t) retcode;
    }
    LLFIO_WIN_DEADLINE_TO_TIMEOUT_LOOP(d);
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const process_handle &process_handle::current() noexcept
{
  static process_handle self = []() -> process_handle {
    process_handle ret(native_handle_type(native_handle_type::disposition::process, GetCurrentProcess()), flag::release_pipes_on_close);
    ret._in_pipe = pipe_handle(native_handle_type(native_handle_type::disposition::pipe | native_handle_type::disposition::readable, GetStdHandle(STD_INPUT_HANDLE)), pipe_handle::caching::all, pipe_handle::flag::none, nullptr);
    ret._out_pipe = pipe_handle(native_handle_type(native_handle_type::disposition::pipe | native_handle_type::disposition::writable, GetStdHandle(STD_OUTPUT_HANDLE)), pipe_handle::caching::all, pipe_handle::flag::none, nullptr);
    ret._error_pipe = pipe_handle(native_handle_type(native_handle_type::disposition::pipe | native_handle_type::disposition::writable, GetStdHandle(STD_ERROR_HANDLE)), pipe_handle::caching::all, pipe_handle::flag::none, nullptr);
    return ret;
  }();
  return self;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle> process_handle::launch_process(path_view path, span<path_view_component> args, span<path_view_component> env, flag flags) noexcept
{
  result<process_handle> ret(in_place_type<process_handle>, native_handle_type(), flags);
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::process;
  pipe_handle childinpipe, childoutpipe, childerrorpipe;
  pipe_handle::flag pipeflags = !(flags & flag::no_multiplexable_pipes) ? pipe_handle::flag::multiplexable : pipe_handle::flag::none;

  if(!(flags & flag::no_redirect_in_pipe))
  {
    OUTCOME_TRY(auto &&handles, pipe_handle::anonymous_pipe(pipe_handle::caching::all, pipeflags));
    ret.value()._in_pipe = std::move(handles.first);
    childoutpipe = std::move(handles.second);
  }
  if(!(flags & flag::no_redirect_out_pipe))
  {
    OUTCOME_TRY(auto &&handles, pipe_handle::anonymous_pipe(pipe_handle::caching::all, pipeflags));
    ret.value()._out_pipe = std::move(handles.second);
    childinpipe = std::move(handles.first);
  }
  if(!(flags & flag::no_redirect_error_pipe))
  {
    // stderr must not buffer writes
    OUTCOME_TRY(auto &&handles, pipe_handle::anonymous_pipe(pipe_handle::caching::reads, pipeflags));
    ret.value()._error_pipe = std::move(handles.first);
    childerrorpipe = std::move(handles.second);
  }

  STARTUPINFOW si;
  memset(&si, 0, sizeof(si));
  si.cb = sizeof(STARTUPINFOW);
  if(childinpipe.is_valid() || childoutpipe.is_valid() || childerrorpipe.is_valid())
  {
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = childinpipe.is_valid() ? childinpipe.native_handle().h : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = childoutpipe.is_valid() ? childoutpipe.native_handle().h : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = childerrorpipe.is_valid() ? childerrorpipe.native_handle().h : GetStdHandle(STD_ERROR_HANDLE);
    if(!SetHandleInformation(si.hStdInput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
      return win32_error();
    if(!SetHandleInformation(si.hStdOutput, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
      return win32_error();
    if(!SetHandleInformation(si.hStdError, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
      return win32_error();
  }

  wchar_t argsbuffer[32768], *argsbuffere = argsbuffer;
  // First copy in the executable path
  *argsbuffere++ = '"';
  OUTCOME_TRY(visit(path, [&](auto sv) -> result<void> {
    for(auto c : sv)
    {
      if(argsbuffere - argsbuffer >= sizeof(argsbuffer))
      {
        return errc::value_too_large;
      }
      *argsbuffere++ = c;
    }
    return success();
  }));
  if(argsbuffere - argsbuffer >= sizeof(argsbuffer) - 2)
  {
    return errc::value_too_large;
  }
  *argsbuffere++ = '"';
  *argsbuffere++ = ' ';
  for(auto arg : args)
  {
    OUTCOME_TRY(visit(arg, [&](auto sv) -> result<void> {
      for(auto c : sv)
      {
        if(argsbuffere - argsbuffer >= sizeof(argsbuffer))
        {
          return errc::value_too_large;
        }
        *argsbuffere++ = c;
      }
      if(argsbuffere - argsbuffer >= sizeof(argsbuffer))
      {
        return errc::value_too_large;
      }
      *argsbuffere++ = ' ';
      return success();
    }));
  }
  *(--argsbuffere) = 0;
  wchar_t envbuffer[32768], *envbuffere = envbuffer;
  for(auto i : env)
  {
    OUTCOME_TRY(visit(i, [&](auto sv) -> result<void> {
      for(auto c : sv)
      {
        if(envbuffere - envbuffer >= sizeof(envbuffer))
        {
          return errc::value_too_large;
        }
        *envbuffere++ = c;
      }
      if(envbuffere - envbuffer >= sizeof(argsbuffer))
      {
        return errc::value_too_large;
      }
      *envbuffere++ = 0;
      return success();
    }));
  }
  *envbuffere = 0;
  path_view::zero_terminated_rendered_path<> zpath(path);
  PROCESS_INFORMATION pi;
  if(!CreateProcessW(zpath.data(), argsbuffer, nullptr, nullptr, true, CREATE_UNICODE_ENVIRONMENT, envbuffer, nullptr, &si, &pi))
    return win32_error();
  nativeh.h = pi.hProcess;
  (void) CloseHandle(pi.hThread);
  return ret;
}

LLFIO_V2_NAMESPACE_END
