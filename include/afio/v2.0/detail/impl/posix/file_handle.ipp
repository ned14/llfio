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

#include "../../../file_handle.hpp"

#include "../../../stat.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

// allocate at process start to ensure later failure to allocate won't cause failure
static filesystem::path temporary_files_directory_("/tmp/no_temporary_directories_accessible");
AFIO_HEADERS_ONLY_FUNC_SPEC path_view temporary_files_directory() noexcept
{
  static struct temporary_files_directory_done_
  {
    temporary_files_directory_done_()
    {
      try
      {
        filesystem::path::string_type buffer;
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

result<void> file_handle::_fetch_inode() noexcept
{
  stat_t s;
  OUTCOME_TRYV(s.fill(*this, stat_t::want::dev | stat_t::want::ino));
  _devid = s.st_dev;
  _inode = s.st_ino;
  return success();
}

inline result<path_handle> containing_directory(filesystem::path &filename, const file_handle &h, deadline d) noexcept
{
#ifdef AFIO_DISABLE_RACE_FREE_PATH_FUNCTIONS
  return std::errc::function_not_supported;
#endif
  std::chrono::steady_clock::time_point began_steady;
  std::chrono::system_clock::time_point end_utc;
  if(d)
  {
    if(d.steady)
      began_steady = std::chrono::steady_clock::now();
    else
      end_utc = d.to_time_point();
  }
  try
  {
    for(;;)
    {
      // Get current path for handle and open its containing dir
      auto currentpath_ = h.current_path();
      if(!currentpath_)
        continue;
      filesystem::path currentpath = std::move(currentpath_.value());
      // If current path is empty, it's been deleted
      if(currentpath.empty())
        return std::errc::no_such_file_or_directory;
      filename = currentpath.filename();
      currentpath.remove_filename();
      auto currentdirh_ = path_handle::path(currentpath);
      if(!currentdirh_)
        continue;
      path_handle currentdirh = std::move(currentdirh_.value());
      if(h.flags() & handle::flag::disable_safety_unlinks)
        return success(std::move(currentdirh));
      // Open the same file name, and compare dev and inode
      auto nh_ = file_handle::file(currentdirh, filename);
      if(!nh_)
        continue;
      file_handle nh = std::move(nh_.value());
      // If the same, we know for a fact that this is the correct containing dir for now at least
      if(nh.st_dev() == h.st_dev() && nh.st_ino() == h.st_ino())
        return success(std::move(currentdirh));
      // Check timeout
      if(d)
      {
        if(d.steady)
        {
          if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds(d.nsecs)))
            return std::errc::timed_out;
        }
        else
        {
          if(std::chrono::system_clock::now() >= end_utc)
            return std::errc::timed_out;
        }
      }
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<file_handle> file_handle::file(const path_handle &base, file_handle::path_view_type path, file_handle::mode _mode, file_handle::creation _creation, file_handle::caching _caching, file_handle::flag flags) noexcept
{
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  AFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  OUTCOME_TRY(attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, _creation, _caching, flags));
  path_view::c_str zpath(path);
  if(base.is_valid())
  {
#ifdef AFIO_DISABLE_RACE_FREE_PATH_FUNCTIONS
    return std::errc::function_not_supported;
#else
    nativeh.fd = ::openat(base.native_handle().fd, zpath.buffer, attribs, 0x1b0 /*660*/);
#endif
  }
  else
  {
    nativeh.fd = ::open(zpath.buffer, attribs, 0x1b0 /*660*/);
  }
  if(-1 == nativeh.fd)
    return {errno, std::system_category()};
  if(!(flags & flag::disable_safety_unlinks))
  {
    OUTCOME_TRYV(ret.value()._fetch_inode());
  }
  if(_creation == creation::truncate && ret.value().are_safety_fsyncs_issued())
    fsync(nativeh.fd);
  return ret;
}

result<file_handle> file_handle::temp_inode(path_view_type dirpath, mode _mode, flag flags) noexcept
{
  caching _caching = caching::temporary;
  // No need to rename to random on unlink or check inode before unlink
  flags |= flag::unlink_on_close | flag::disable_safety_unlinks;
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  AFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  // Open file exclusively to prevent collision
  OUTCOME_TRY(attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, creation::only_if_not_exist, _caching, flags));
  path_view::c_str zpath(dirpath);
#ifdef O_TMPFILE
  // Linux has a special flag just for this use case
  attribs |= O_TMPFILE;
  attribs &= ~O_EXCL;  // allow relinking later
  nativeh.fd = ::open(zpath.buffer, attribs, 0600);
  if(-1 != nativeh.fd)
  {
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    return ret;
  }
  // If it failed, assume this kernel or FS doesn't support O_TMPFILE
  attribs &= ~O_TMPFILE;
  attribs |= O_EXCL;
#endif
#ifdef AFIO_DISABLE_RACE_FREE_PATH_FUNCTIONS
  return std::errc::function_not_supported;
#endif
  OUTCOME_TRY(dirh, path_handle::path(dirpath));
  std::string random;
  for(;;)
  {
    try
    {
      random = utils::random_string(32) + ".tmp";
    }
    catch(...)
    {
      return error_from_exception();
    }
    nativeh.fd = ::openat(dirh.native_handle().fd, random.c_str(), attribs, 0600);  // user read/write perms only
    if(-1 == nativeh.fd)
    {
      int errcode = errno;
      if(EEXIST == errcode)
        continue;
      return {errcode, std::system_category()};
    }
    // Immediately unlink after creation
    if(-1 == ::unlinkat(dirh.native_handle().fd, random.c_str(), 0))
      return {errno, std::system_category()};
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    return ret;
  }
}

file_handle::io_result<file_handle::const_buffers_type> file_handle::barrier(file_handle::io_request<file_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(d)
    return std::errc::not_supported;
#ifdef __linux__
  if(!wait_for_device && !and_metadata)
  {
    // Linux has a lovely dedicated syscall giving us exactly what we need here
    extent_type offset = reqs.offset, bytes = 0;
    for(const auto &req : reqs.buffers)  // empty buffers means bytes = 0 which means sync entire file
      bytes += req.len;
    unsigned flags = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE;  // start writing all dirty pages in range now
    if(-1 != ::sync_file_range(_v.fd, offset, bytes, flags))
      return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
  }
#endif
#if !defined(__FreeBSD__) && !defined(__APPLE__)  // neither of these have fdatasync()
  if(!and_metadata)
  {
    if(-1 == ::fdatasync(_v.fd))
      return {errno, std::system_category()};
    return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
  }
#endif
#ifdef __APPLE__
  if(!wait_for_device)
  {
    // OS X fsync doesn't wait for the device to flush its buffers
    if(-1 == ::fsync(_v.fd))
      return {errno, std::system_category()};
    return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
  }
  // This is the fsync as on every other OS
  if(-1 == ::fcntl(_v.fd, F_FULLFSYNC))
    return {errno, std::system_category()};
#else
  if(-1 == ::fsync(_v.fd))
    return {errno, std::system_category()};
#endif
  return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
}

result<file_handle> file_handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, _caching, _flags));
  ret.value()._service = _service;
  ret.value()._v.behaviour = _v.behaviour;
  ret.value()._v.fd = ::dup(_v.fd);
  if(-1 == ret.value()._v.fd)
    return {errno, std::system_category()};
  return ret;
}

result<void> file_handle::relink(const path_handle &base, path_view_type path, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  path_view::c_str zpath(path);
#ifdef O_TMPFILE
  // If the handle was created with O_TMPFILE, we need a different approach
  if(path.empty() && (_caching == file_handle::caching::temporary))
  {
    char _path[PATH_MAX];
    snprintf(_path, PATH_MAX, "/proc/self/fd/%d", _v.fd);
    if(-1 == ::linkat(AT_FDCWD, _path, base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer, AT_SYMLINK_FOLLOW))
      return {errno, std::system_category()};
  }
#endif
  // Open our containing directory
  filesystem::path filename;
  OUTCOME_TRY(dirh, containing_directory(filename, *this, d));
  if(-1 == ::renameat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer))
    return {errno, std::system_category()};
  return success();
}

result<void> file_handle::unlink(deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // Open our containing directory
  filesystem::path filename;
  OUTCOME_TRY(dirh, containing_directory(filename, *this, d));
  if(-1 == ::unlinkat(dirh.native_handle().fd, filename.c_str(), 0))
    return {errno, std::system_category()};
  return success();
}

result<file_handle::extent_type> file_handle::length() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  struct stat s;
  memset(&s, 0, sizeof(s));
  if(-1 == ::fstat(_v.fd, &s))
    return {errno, std::system_category()};
  return s.st_size;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(ftruncate(_v.fd, newsize) < 0)
    return {errno, std::system_category()};
  if(are_safety_fsyncs_issued())
  {
    fsync(_v.fd);
  }
  return newsize;
}

AFIO_V2_NAMESPACE_END
