/* A handle to something
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
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

#include "../../../stat.hpp"

#include <fcntl.h>
#include <unistd.h>

AFIO_V2_NAMESPACE_BEGIN

// allocate at process start to ensure later failure to allocate won't cause failure
static fixme_path temporary_files_directory_("/tmp/no_temporary_directories_accessible");
const fixme_path &fixme_temporary_files_directory() noexcept
{
  static struct temporary_files_directory_done_
  {
    temporary_files_directory_done_()
    {
      try
      {
        fixme_path::string_type buffer;
        auto testpath = [&]() {
          size_t len = buffer.size();
          if(buffer[len - 1] == '/')
            buffer.resize(--len);
          buffer.append("/afio_tempfile_probe_file.tmp");
          int h = open(buffer.c_str(), O_WRONLY | O_CREAT, 0666);
          if(-1 != h)
          {
            unlink(buffer.c_str());
            close(h);
            buffer.resize(len);
            temporary_files_directory_ = std::move(buffer);
            return true;
          }
          return false;
        };
        // Note that XDG_RUNTIME_DIR is the systemd runtime directory for the current user
        static const char *variables[] = {"TMPDIR", "TMP", "TEMP", "TEMPDIR", "XDG_RUNTIME_DIR"};
        for(size_t n = 0; n < sizeof(variables) / sizeof(variables[0]); n++)
        {
          const char *env = getenv(variables[n]);
          if(env)
          {
            buffer.assign(env);
            if(testpath())
              return;
          }
        }
        // If everything earlier failed e.g. if our environment block is zeroed,
        // fall back to /tmp and then /var/tmp, the last of which should succeed even if tmpfs is not mounted
        buffer.assign("/tmp");
        if(testpath())
          return;
        buffer.assign("/var/tmp");
        if(testpath())
          return;
        // If even no system tmp directory is available, as a hail mary try $HOME
        static const char *variables2[] = {"HOME"};
        for(size_t n = 0; n < sizeof(variables2) / sizeof(variables2[0]); n++)
        {
          const char *env = getenv(variables2[n]);
          if(env)
          {
            buffer.assign(env);
            if(testpath())
              return;
          }
        }
      }
      catch(...)
      {
      }
    }
  } init;
  return temporary_files_directory_;
}

inline result<int> attribs_from_handle_mode_caching_and_flags(native_handle_type &nativeh, file_handle::mode _mode, file_handle::creation _creation, file_handle::caching _caching, file_handle::flag) noexcept
{
  int attribs = 0;
  switch(_mode)
  {
  case file_handle::mode::unchanged:
    return make_errored_result<int>(std::errc::invalid_argument);
  case file_handle::mode::none:
    break;
  case file_handle::mode::attr_read:
  case file_handle::mode::read:
    attribs = O_RDONLY;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
    break;
  case file_handle::mode::attr_write:
  case file_handle::mode::write:
    attribs = O_RDWR;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
    break;
  case file_handle::mode::append:
    attribs = O_APPEND;
    nativeh.behaviour |= native_handle_type::disposition::writable | native_handle_type::disposition::append_only;
    break;
  }
  switch(_creation)
  {
  case file_handle::creation::open_existing:
    break;
  case file_handle::creation::only_if_not_exist:
    attribs |= O_CREAT | O_EXCL;
    break;
  case file_handle::creation::if_needed:
    attribs |= O_CREAT;
    break;
  case file_handle::creation::truncate:
    attribs |= O_TRUNC;
    break;
  }
  switch(_caching)
  {
  case file_handle::caching::unchanged:
    return make_errored_result<int>(std::errc::invalid_argument);
  case file_handle::caching::none:
    attribs |= O_SYNC | O_DIRECT;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case file_handle::caching::only_metadata:
    attribs |= O_DIRECT;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case file_handle::caching::reads:
    attribs |= O_SYNC;
    break;
  case file_handle::caching::reads_and_metadata:
#ifdef O_DSYNC
    attribs |= O_DSYNC;
#else
    attribs |= O_SYNC;
#endif
    break;
  case file_handle::caching::all:
  case file_handle::caching::safety_fsyncs:
  case file_handle::caching::temporary:
    break;
  }
  return attribs;
}

result<void> file_handle::_fetch_inode() noexcept
{
  stat_t s;
  OUTCOME_TRYV(s.fill(*this, stat_t::want::dev | stat_t::want::ino));
  _devid = s.st_dev;
  _inode = s.st_ino;
  return make_valued_result<void>();
}

inline result<void> check_inode(const file_handle &h) noexcept
{
  stat_t s;
  OUTCOME_TRYV(s.fill(h, stat_t::want::dev | stat_t::want::ino));
  if(s.st_dev != h.st_dev() || s.st_ino != h.st_ino())
    return make_errored_result<void>(std::errc::no_such_file_or_directory);
  return make_valued_result<void>();
}

result<file_handle> file_handle::file(const path_handle &base, file_handle::path_view_type _path, file_handle::mode _mode, file_handle::creation _creation, file_handle::caching _caching, file_handle::flag flags) noexcept
{
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, std::move(_path), _caching, flags));
  native_handle_type &nativeh = ret.get()._v;
  OUTCOME_TRY(attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, _creation, _caching, flags));
  nativeh.behaviour |= native_handle_type::disposition::file;
  const char *path_ = ret.value()._path.c_str();
  nativeh.fd = ::open(path_, attribs, 0x1b0 /*660*/);
  if(-1 == nativeh.fd)
    return make_errored_result<file_handle>(errno, last190(ret.value()._path.native()));
  AFIO_LOG_FUNCTION_CALL(&ret);
  if(!(flags & flag::disable_safety_unlinks))
  {
    OUTCOME_TRYV(ret.value()._fetch_inode());
  }
  if(_creation == creation::truncate && ret.value().are_safety_fsyncs_issued())
    fsync(nativeh.fd);
  return ret;
}

result<file_handle> file_handle::temp_inode(path_type dirpath, mode _mode, flag flags) noexcept
{
  caching _caching = caching::temporary;
  // No need to rename to random on unlink or check inode before unlink
  flags |= flag::unlink_on_close | flag::disable_safety_unlinks;
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, path_type(), _caching, flags));
  native_handle_type &nativeh = ret.get()._v;
  // Open file exclusively to prevent collision
  OUTCOME_TRY(attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, creation::only_if_not_exist, _caching, flags));
  nativeh.behaviour |= native_handle_type::disposition::file;
#ifdef O_TMPFILE
  // Linux has a special flag just for this use case
  attribs |= O_TMPFILE;
  attribs &= ~O_EXCL;  // allow relinking later
  const char *path_ = ret.value()._path.c_str();
  nativeh.fd = ::open(path_, attribs, 0600);
  if(-1 != nativeh.fd)
  {
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    return ret;
  }
  // If it failed, assume this kernel or FS doesn't support O_TMPFILE
  attribs &= ~O_TMPFILE;
  attribs |= O_EXCL;
#endif
  AFIO_LOG_FUNCTION_CALL(&ret);
  for(;;)
  {
    try
    {
      ret.value()._path = dirpath / (utils::random_string(32) + ".tmp");
    }
    BOOST_OUTCOME_CATCH_ALL_EXCEPTION_TO_RESULT
    const char *path_ = ret.value()._path.c_str();
    nativeh.fd = ::open(path_, attribs, 0600);  // user read/write perms only
    if(-1 == nativeh.fd)
    {
      int errcode = errno;
      if(EEXIST == errcode)
        continue;
      return make_errored_result<file_handle>(errcode);
    }
    // Immediately unlink after creation
    if(-1 == ::unlink(path_))
      return make_errored_result<file_handle>(errno);
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    return ret;
  }
}

file_handle::io_result<file_handle::const_buffers_type> file_handle::barrier(file_handle::io_request<file_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(d)
    return make_errored_result<>(std::errc::not_supported);
#ifdef __linux__
  if(!wait_for_device && !and_metadata)
  {
    // Linux has a lovely dedicated syscall giving us exactly what we need here
    extent_type offset = reqs.offset, bytes = 0;
    for(const auto &req : reqs.buffers)  // empty buffers means bytes = 0 which means sync entire file
      bytes += req.second;
    unsigned flags = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE;  // start writing all dirty pages in range now
    if(-1 != ::sync_file_range(_v.fd, offset, bytes, flags))
      return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
  }
#endif
#if !defined(__FreeBSD__) && !defined(__APPLE__)  // neither of these have fdatasync()
  if(!and_metadata)
  {
    if(-1 == ::fdatasync(_v.fd))
      return make_errored_result<>(errno);
    return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
  }
#endif
#ifdef __APPLE__
  if(!wait_for_device)
  {
    // OS X fsync doesn't wait for the device to flush its buffers
    if(-1 == ::fsync(_v.fd))
      return make_errored_result<>(errno);
    return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
  }
  // This is the fsync as on every other OS
  if(-1 == ::fcntl(_v.fd, F_FULLFSYNC))
    return make_errored_result<>(errno);
#else
  if(-1 == ::fsync(_v.fd))
    return make_errored_result<>(errno);
#endif
  return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
}

result<file_handle> file_handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, _path, _caching, _flags));
  ret.value()._service = _service;
  ret.value()._v.behaviour = _v.behaviour;
  ret.value()._v.fd = ::dup(_v.fd);
  if(-1 == ret.value()._v.fd)
    return make_errored_result<file_handle>(errno, last190(_path.native()));
  return ret;
}

result<file_handle::path_type> file_handle::relink(path_type newpath) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(newpath.is_relative())
    newpath = _path.parent_path() / newpath;
#ifdef O_TMPFILE
  // If the handle was created with O_TMPFILE, we need a different approach
  if(_path.empty() && (_caching == file_handle::caching::temporary))
  {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/proc/self/fd/%d", _v.fd);
    if(-1 == ::linkat(AT_FDCWD, path, AT_FDCWD, newpath.c_str(), AT_SYMLINK_FOLLOW))
      return make_errored_result<path_type>(errno);
  }
  else
#endif
  {
    // FIXME: As soon as we implement fat paths, make this race free
    if(!(_flags & flag::disable_safety_unlinks))
    {
      OUTCOME_TRYV(check_inode(*this));
    }
    if(-1 == ::rename(_path.c_str(), newpath.c_str()))
      return make_errored_result<path_type>(errno, last190(_path.native()));
  }
  _path = std::move(newpath);
  return _path;
}

result<void> file_handle::unlink() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // FIXME: As soon as we implement fat paths, make this race free
  if(!(_flags & flag::disable_safety_unlinks))
  {
    OUTCOME_TRYV(check_inode(*this));
  }
  if(-1 == ::unlink(_path.c_str()))
    return make_errored_result<void>(errno, last190(_path.native()));
  _path.clear();
  return make_valued_result<void>();
}

result<file_handle::extent_type> file_handle::length() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  struct stat s;
  memset(&s, 0, sizeof(s));
  if(-1 == ::fstat(_v.fd, &s))
    return make_errored_result<file_handle::extent_type>(errno, last190(_path.native()));
  return s.st_size;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(ftruncate(_v.fd, newsize) < 0)
    return make_errored_result<extent_type>(errno, last190(_path.native()));
  if(are_safety_fsyncs_issued())
  {
    fsync(_v.fd);
  }
  return newsize;
}

AFIO_V2_NAMESPACE_END
