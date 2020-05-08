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

#include <fcntl.h>
#include <unistd.h>

#ifdef __FreeBSD__
#include <sys/sysctl.h>  // for sysctl()
#include <sys/user.h>    // for struct kinfo_file
#endif

#if defined(__APPLE__)
#include <sys/stat.h>  // for struct stat
#endif

LLFIO_V2_NAMESPACE_BEGIN

handle::~handle()
{
  if(_v)
  {
    // Call close() below
    auto ret = handle::close();
    if(ret.has_error())
    {
      LLFIO_LOG_FATAL(_v.fd, "handle::~handle() close failed");
      abort();
    }
  }
}

result<handle::path_type> handle::current_path() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  try
  {
    // Most efficient, least memory copying method is direct fill of a string which is moved into filesystem::path
    filesystem::path::string_type ret;
#if defined(__linux__)
    ret.resize(32769);
    auto *out = const_cast<char *>(ret.data());
    // Linux keeps a symlink at /proc/self/fd/n
    char in[64];
    snprintf(in, sizeof(in), "/proc/self/fd/%d", _v.fd);
    ssize_t len;
    if((len = readlink(in, out, 32768)) == -1)
    {
      return posix_error();
    }
    ret.resize(len);
    // Linux prepends or appends a " (deleted)" when a fd is nameless
    // TODO(ned): Should I stat the target to be really sure?
    if(ret.size() >= 10 && ((ret.compare(0, 10, " (deleted)") == 0) || (ret.compare(ret.size() - 10, 10, " (deleted)") == 0)))
    {
      ret.clear();
    }
#elif defined(__APPLE__)
    ret.resize(32769);
    char *out = const_cast<char *>(ret.data());
    // Yes, this API is instant memory corruption. Thank you Apple.
    if(-1 == fcntl(_v.fd, F_GETPATH, out))
      return posix_error();
    ret.resize(strchr(out, 0) - out);  // no choice :(
    // Apple returns the previous path when deleted, so lstat to be sure
    struct stat ls;
    bool exists = (-1 != ::lstat(out, &ls));
    if(!exists)
      ret.clear();
#elif defined(__FreeBSD__)
    // Unfortunately this call is broken on FreeBSD 10 where it is currently returning
    // null paths most of the time for regular files. Directories work perfectly. I've
    // logged a bug with test case at https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=197695.
    size_t len;
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_FILEDESC, getpid()};
    if(-1 == sysctl(mib, 4, NULL, &len, NULL, 0))
      return posix_error();
    std::vector<char> buffer(len * 2);
    if(-1 == sysctl(mib, 4, buffer.data(), &len, NULL, 0))
      return posix_error();
#if 0  // ndef NDEBUG
    for (char *p = buffer.data(); p<buffer.data() + len;)
    {
      struct kinfo_file *kif = (struct kinfo_file *) p;
      std::cout << kif->kf_type << " " << kif->kf_fd << " " << kif->kf_path << std::endl;
      p += kif->kf_structsize;
    }
#endif
    for(char *p = buffer.data(); p < buffer.data() + len;)
    {
      struct kinfo_file *kif = (struct kinfo_file *) p;
      if(kif->kf_fd == _v.fd)
      {
        ret = std::string(kif->kf_path);
        break;
      }
      p += kif->kf_structsize;
    }
#else
#error Unknown system
#endif
    return path_type(std::move(ret));
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<void> handle::close() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_v)
  {
#ifndef NDEBUG
    // Trap when refined handle implementations don't set their vptr properly (this took a while to debug!)
    if((static_cast<unsigned>(_v.behaviour) & 0xff00) != 0 && !(_v.behaviour & native_handle_type::disposition::_child_close_executed))
    {
      LLFIO_LOG_FATAL(this, "handle::close() called on a derived handle implementation, this suggests vptr is incorrect");
      abort();
    }
#endif
    if(are_safety_barriers_issued() && is_writable() && !is_pipe())
    {
      if(-1 == fsync(_v.fd))
      {
        return posix_error();
      }
    }
    if(-1 == ::close(_v.fd))
    {
      return posix_error();
    }
    _v = native_handle_type();
  }
  return success();
}

result<handle> handle::clone() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  result<handle> ret(handle(native_handle_type(), kernel_caching(), _flags));
  ret.value()._v.behaviour = _v.behaviour;
  ret.value()._v.fd = ::fcntl(_v.fd, F_DUPFD_CLOEXEC, 0);
  if(-1 == ret.value()._v.fd)
  {
    int e = errno;
    if(e == EBADF)
    {
      // Fall back onto F_DUPFD
      ret.value()._v.fd = ::fcntl(_v.fd, F_DUPFD, 0);
      if(-1 != ret.value()._v.fd)
      {
        // Set close on exec
        int attribs = ::fcntl(_v.fd, F_GETFL);
        if(-1 == attribs)
        {
          return posix_error();
        }
        attribs |= FD_CLOEXEC;
        if(-1 == ::fcntl(_v.fd, F_SETFL, attribs))
        {
          return posix_error();
        }
        return ret;
      }
    }
    return posix_error(e);
  }
  return ret;
}

result<void> handle::set_append_only(bool enable) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  int attribs = ::fcntl(_v.fd, F_GETFL);
  if(-1 == attribs)
  {
    return posix_error();
  }
  if(enable)
  {
    // Set append_only
    attribs |= O_APPEND;
    if(-1 == ::fcntl(_v.fd, F_SETFL, attribs))
    {
      return posix_error();
    }
    _v.behaviour |= native_handle_type::disposition::append_only;
  }
  else
  {
    // Remove append_only
    attribs &= ~O_APPEND;
    if(-1 == ::fcntl(_v.fd, F_SETFL, attribs))
    {
      return posix_error();
    }
    _v.behaviour &= ~native_handle_type::disposition::append_only;
  }
  return success();
}

LLFIO_V2_NAMESPACE_END
