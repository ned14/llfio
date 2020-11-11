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

result<pipe_handle> pipe_handle::pipe(pipe_handle::path_view_type path, pipe_handle::mode _mode, pipe_handle::creation _creation, pipe_handle::caching _caching, pipe_handle::flag flags, const path_handle &base) noexcept
{
  result<pipe_handle> ret(pipe_handle(native_handle_type(), 0, 0, _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::pipe;
  OUTCOME_TRY(auto &&attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, _creation, _caching, flags));
  attribs &= ~(O_CREAT | O_EXCL);                                   // needs to be emulated for fifos
  nativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  if(creation::truncate_existing == _creation)
  {
    return errc::operation_not_supported;
  }
  path_handle dirh;
  path_type leafname;
  int dirhfd = AT_FDCWD;
  try
  {
    // Take a path handle to the directory containing the symlink
    auto path_parent = path.parent_path();
    leafname = path.filename().path();
    if(base.is_valid() && path_parent.empty())
    {
      dirhfd = base.native_handle().fd;
    }
    else if(!path_parent.empty())
    {
      OUTCOME_TRY(auto &&dh, path_handle::path(base, path_parent.empty() ? "." : path_parent));
      dirh = std::move(dh);
      dirhfd = dirh.native_handle().fd;
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
  bool need_to_create_fifo = false;
  switch(_creation)
  {
  case creation::open_existing:
  case creation::if_needed:
  {
    // Complain if it doesn't exist
    struct stat s;
    if(-1 == ::fstatat(dirhfd, leafname.c_str(), &s, AT_SYMLINK_NOFOLLOW))
    {
      if(creation::open_existing == _creation)
      {
        return posix_error();
      }
      need_to_create_fifo = true;
    }
    break;
  }
  case creation::only_if_not_exist:
  {
    // Complain if it does exist
    struct stat s;
    if(-1 != ::fstatat(dirhfd, leafname.c_str(), &s, AT_SYMLINK_NOFOLLOW))
    {
      return errc::file_exists;
    }
    need_to_create_fifo = true;
    break;
  }
  case creation::truncate_existing:
  case creation::always_new:
  {
    (void) ::unlinkat(dirhfd, leafname.c_str(), 0);
    need_to_create_fifo = true;
    break;
  }
  }
  if(need_to_create_fifo)
  {
#ifdef __APPLE__
    native_handle_type dirhfd_;
    dirhfd_.fd = dirhfd;
    path_handle dirhfdwrap(dirhfd_, path_handle::caching::none, path_handle::flag::none);
    auto dirhpath = dirhfdwrap.current_path();
    dirhfdwrap.release();
    OUTCOME_TRY(std::move(dirhpath));
    dirhpath.value() /= leafname;
    if(-1 == ::mkfifo(dirhpath.value().c_str(), 0x1b0 /*660*/))
#else
    if(-1 == ::mkfifoat(dirhfd, leafname.c_str(), 0x1b0 /*660*/))
#endif
    {
      return posix_error();
    }
    ret.value()._flags |= flag::unlink_on_first_close;
  }
  nativeh.fd = ::openat(dirhfd, leafname.c_str(), attribs, 0x1b0 /*660*/);
  if(-1 == nativeh.fd)
  {
    return posix_error();
  }
  return ret;
}

result<std::pair<pipe_handle, pipe_handle>> pipe_handle::anonymous_pipe(caching _caching, flag flags) noexcept
{
  result<std::pair<pipe_handle, pipe_handle>> ret(pipe_handle(native_handle_type(), 0, 0, _caching, flags, nullptr), pipe_handle(native_handle_type(), 0, 0, _caching, flags, nullptr));
  native_handle_type &readnativeh = ret.value().first._v, &writenativeh = ret.value().second._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  OUTCOME_TRY(auto &&readattribs, attribs_from_handle_mode_caching_and_flags(readnativeh, mode::read, creation::open_existing, _caching, flags));
  OUTCOME_TRY(attribs_from_handle_mode_caching_and_flags(writenativeh, mode::append, creation::open_existing, _caching, flags));
#ifdef O_DIRECT
  readattribs &= O_DIRECT | O_NONBLOCK;
#else
  readattribs &= O_NONBLOCK;
#endif
  readnativeh.behaviour |= native_handle_type::disposition::pipe;
  readnativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  writenativeh.behaviour |= native_handle_type::disposition::pipe;
  writenativeh.behaviour &= ~native_handle_type::disposition::seekable;  // not seekable
  int pipefds[2];
#if defined(__linux__) || defined(__FreeBSD__)
  if(-1 == ::pipe2(pipefds, readattribs))
  {
    return posix_error();
  }
#else
  if(-1 == ::pipe(pipefds))
  {
    return posix_error();
  }
  // Set close-on-exec, and any caching attributes
  if(-1 == ::fcntl(pipefds[0], F_SETFD, FD_CLOEXEC))
  {
    return posix_error();
  }
  if(-1 == ::fcntl(pipefds[1], F_SETFD, FD_CLOEXEC))
  {
    return posix_error();
  }
  if(-1 == ::fcntl(pipefds[0], F_SETFL, ::fcntl(pipefds[0], F_GETFL) | readattribs))
  {
    return posix_error();
  }
  if(-1 == ::fcntl(pipefds[1], F_SETFL, ::fcntl(pipefds[1], F_GETFL) | readattribs))
  {
    return posix_error();
  }
#endif
  readnativeh.fd = pipefds[0];
  writenativeh.fd = pipefds[1];
  return ret;
}

LLFIO_V2_NAMESPACE_END
