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

#include <signal.h>  // for siginfo_t
#include <spawn.h>
#include <sys/wait.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>
extern "C" char **environ;
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>  // for _NSGetExecutablePath
extern "C" char **environ;
#endif

LLFIO_V2_NAMESPACE_BEGIN

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC bool process_handle::is_running() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(!_v)
  {
    return false;
  }
  siginfo_t info;
  memset(&info, 0, sizeof(info));
  if(-1 == ::waitid(P_PID, _v.pid, &info, WEXITED | WNOHANG | WNOWAIT))
  {
    return false;
  }
  return info.si_pid == 0;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle::path_type> process_handle::current_path() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_v.pid != getpid())
  {
    // Laziness ...
    return success();
  }
  char buffer[PATH_MAX + 1];
#ifdef __linux__
  // Read what the symbolic link at /proc/self/exe points at
  ssize_t len = ::readlink("/proc/self/exe", buffer, PATH_MAX);
  if(len > 0)
  {
    buffer[len] = 0;
    return path_type(path_type::string_type(buffer, len));
  }
  return posix_error();
#elif defined(__FreeBSD__)
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
  size_t len = PATH_MAX;
  int ret = sysctl(mib, 4, buffer, &len, NULL, 0);
  if(ret >= 0)
  {
    buffer[len] = 0;
    return path_type(path_type::string_type(buffer, len));
  }
  return posix_error();
#elif defined(__APPLE__)
  uint32_t bufsize = PATH_MAX;
  if(_NSGetExecutablePath(buffer, &bufsize) == 0)
  {
    return path_type(path_type::string_type(buffer));
  }
  return errc::resource_unavailable_try_again;
#else
#error Unknown platform
#endif
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> process_handle::close() noexcept
{
  OUTCOME_TRY(close_pipes());
  if(_flags & flag::wait_on_close)
  {
    log_level_guard g(log_level::fatal);
    OUTCOME_TRY(wait());
  }
  _v = {};
  return success();
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle> process_handle::clone() const noexcept
{
  return process_handle(_v, _flags);
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC std::unique_ptr<span<path_view_component>, process_handle::_byte_array_deleter> process_handle::environment() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_v.pid != getpid())
  {
    // Laziness ...
    return {};
  }
#ifdef __linux__
  char **environ = __environ;
#endif
  size_t bytesneeded = sizeof(span<path_view_component>);
  size_t count = 0;
  char **e;
  for(e = environ; *e; ++e)
  {
    bytesneeded += sizeof(path_view_component);
    bytesneeded += strlen(*e) + 1;
    ++count;
  }
  auto ret = std::make_unique<byte[]>(bytesneeded);
  auto &out = *(span<path_view_component> *) ret.get();
  auto *array = (path_view_component *) (ret.get() + sizeof(span<path_view_component>)), *arraye = array;
  auto *after = (char *) (ret.get() + sizeof(span<path_view_component>) + sizeof(path_view_component) * count);
  out = {array, arraye};
  for(e = environ; *e; ++e)
  {
    size_t len = strlen(*e);
    memcpy(after, *e, len + 1);
    *arraye++ = path_view_component(after, len, path_view::zero_terminated);
    out = {array, arraye};
    after += len + 1;
  }
  return std::unique_ptr<span<path_view_component>, process_handle::_byte_array_deleter>((span<path_view_component> *) ret.release());
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<intptr_t> process_handle::wait(deadline d) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  intptr_t ret = 0;
  auto check_child = [&]() -> result<bool> {
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    int options = WEXITED | WSTOPPED;
    if(d)
      options |= WNOHANG;
    if(-1 == ::waitid(P_PID, _v.pid, &info, options))
    {
      return posix_error();
    }
    if(info.si_signo == SIGCHLD)
    {
      ret = info.si_status;
      return false;
    }
    return true;
  };
  LLFIO_POSIX_DEADLINE_TO_SLEEP_INIT(d);
  (void) timeout;
  // We currently spin poll non-infinite non-zero waits :(
  for(;;)
  {
    OUTCOME_TRY(auto &&running, check_child());
    if(!running)
      return ret;
    LLFIO_POSIX_DEADLINE_TO_TIMEOUT_LOOP(d);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC const process_handle &process_handle::current() noexcept
{
  static process_handle self = []() -> process_handle {
    process_handle ret(native_handle_type(native_handle_type::disposition::process, getpid()), flag::release_pipes_on_close);
    ret._in_pipe = pipe_handle(native_handle_type(native_handle_type::disposition::pipe | native_handle_type::disposition::readable, STDIN_FILENO), pipe_handle::caching::all, pipe_handle::flag::none, nullptr);
    ret._out_pipe = pipe_handle(native_handle_type(native_handle_type::disposition::pipe | native_handle_type::disposition::writable, STDOUT_FILENO), pipe_handle::caching::all, pipe_handle::flag::none, nullptr);
    ret._error_pipe = pipe_handle(native_handle_type(native_handle_type::disposition::pipe | native_handle_type::disposition::writable, STDERR_FILENO), pipe_handle::caching::all, pipe_handle::flag::none, nullptr);
    return ret;
  }();
  return self;
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<process_handle> process_handle::launch_process(path_view path, span<path_view_component> args, span<path_view_component> env, flag flags) noexcept
{
  try
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

    if(childinpipe.is_valid())
    {
      if(-1 == ::fcntl(childinpipe.native_handle().fd, F_SETFD, FD_CLOEXEC))
        return posix_error();
    }
    if(childoutpipe.is_valid())
    {
      if(-1 == ::fcntl(childoutpipe.native_handle().fd, F_SETFD, FD_CLOEXEC))
        return posix_error();
    }
    if(childerrorpipe.is_valid())
    {
      if(-1 == ::fcntl(childerrorpipe.native_handle().fd, F_SETFD, FD_CLOEXEC))
        return posix_error();
    }

    using small_path_view_c_str = path_view::zero_terminated_rendered_path<filesystem::path::value_type, std::default_delete<filesystem::path::value_type[]>, 1>;
    std::vector<const char *> argptrs(args.size() + 2);
    std::vector<small_path_view_c_str> _args;
    _args.reserve(args.size() + 1);
    _args.emplace_back(path);
    argptrs[0] = _args[0].c_str();
    for(size_t n = 0; n < args.size(); ++n)
    {
      _args.emplace_back(args[n]);
      argptrs[n + 1] = _args[n + 1].c_str();
    }
    std::vector<small_path_view_c_str> _envs;
    std::vector<const char *> envptrs;
    _envs.reserve(env.size());
    envptrs.reserve(env.size() + 1);
    for(const auto &i : env)
    {
      _envs.emplace_back(i);
      envptrs.push_back(_envs.back().c_str());
    }
    envptrs.push_back(nullptr);
#if 0
    ret._processh.fd = ::fork();
    if(0 == ret._processh.fd)
    {
      // I am the child
      if(-1 == ::dup2(childinpipe.native_handle().fd, STDIN_FILENO))
      {
        ::perror("dup2 readh");
        ::exit(1);
      }
      ::close(childreadh.fd);
      if(-1 == ::dup2(childwriteh.fd, STDOUT_FILENO))
      {
        ::perror("dup2 writeh");
        ::exit(1);
      }
      ::close(childwriteh.fd);
      if(!use_parent_errh)
      {
        if(-1 == ::dup2(childerrh.fd, STDERR_FILENO))
        {
          ::perror("dup2 errh");
          ::exit(1);
        }
        ::close(childerrh.fd);
      }
      if(-1 == ::execve(ret._path.c_str(), (char **) argptrs.data(), (char **) envptrs.data()))
      {
        ::perror("execve");
        ::exit(1);
      }
    }
    if(-1 == ret._processh.fd)
      return posix_error();
#else
    posix_spawn_file_actions_t child_fd_actions;
    if(childinpipe.is_valid() || childoutpipe.is_valid() || childerrorpipe.is_valid())
    {
      int err = ::posix_spawn_file_actions_init(&child_fd_actions);
      if(err)
        return posix_error(err);
      if(childinpipe.is_valid())
      {
        err = ::posix_spawn_file_actions_adddup2(&child_fd_actions, childinpipe.native_handle().fd, STDIN_FILENO);
        if(err)
          return posix_error(err);
        err = ::posix_spawn_file_actions_addclose(&child_fd_actions, childinpipe.native_handle().fd);
        if(err)
          return posix_error(err);
      }
      if(childoutpipe.is_valid())
      {
        err = ::posix_spawn_file_actions_adddup2(&child_fd_actions, childoutpipe.native_handle().fd, STDOUT_FILENO);
        if(err)
          return posix_error(err);
        err = ::posix_spawn_file_actions_addclose(&child_fd_actions, childoutpipe.native_handle().fd);
        if(err)
          return posix_error(err);
      }
      if(childerrorpipe.is_valid())
      {
        err = ::posix_spawn_file_actions_adddup2(&child_fd_actions, childerrorpipe.native_handle().fd, STDERR_FILENO);
        if(err)
          return posix_error(err);
        err = ::posix_spawn_file_actions_addclose(&child_fd_actions, childerrorpipe.native_handle().fd);
        if(err)
          return posix_error(err);
      }
    }
    int err = ::posix_spawn(&nativeh.pid, argptrs[0], (childinpipe.is_valid() || childoutpipe.is_valid() || childerrorpipe.is_valid()) ? &child_fd_actions : nullptr, nullptr, (char **) argptrs.data(), (char **) envptrs.data());
    if(err)
      return posix_error(err);
    if(childinpipe.is_valid() || childoutpipe.is_valid() || childerrorpipe.is_valid())
    {
      ::posix_spawn_file_actions_destroy(&child_fd_actions);
    }
#endif
    return ret;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

LLFIO_V2_NAMESPACE_END
