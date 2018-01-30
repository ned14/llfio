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

#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

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
    nativeh.fd = ::openat(base.native_handle().fd, zpath.buffer, attribs, 0x1b0 /*660*/);
  }
  else
  {
    nativeh.fd = ::open(zpath.buffer, attribs, 0x1b0 /*660*/);
  }
  if(-1 == nativeh.fd)
  {
    return {errno, std::system_category()};
  }
  if(!(flags & flag::disable_safety_unlinks))
  {
    if(!ret.value()._fetch_inode())
    {
      // If fetching inode failed e.g. were opening device, disable safety unlinks
      ret.value()._flags &= ~flag::disable_safety_unlinks;
    }
  }
  if((flags & flag::disable_prefetching) || (flags & flag::maximum_prefetching))
  {
#ifdef POSIX_FADV_SEQUENTIAL
    int advice = (flags & flag::disable_prefetching) ? POSIX_FADV_RANDOM : (POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
    if(-1 == ::posix_fadvise(nativeh.fd, 0, 0, advice))
    {
      return {errno, std::system_category()};
    }
#elif __APPLE__
    int advice = (flags & flag::disable_prefetching) ? 0 : 1;
    if(-1 == ::fcntl(nativeh.fd, F_RDAHEAD, advice))
    {
      return {errno, std::system_category()};
    }
#endif
  }
  if(_creation == creation::truncate && ret.value().are_safety_fsyncs_issued())
  {
    fsync(nativeh.fd);
  }
  return ret;
}

result<file_handle> file_handle::temp_inode(const path_handle &dirh, mode _mode, flag flags) noexcept
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
#ifdef O_TMPFILE
  // Linux has a special flag just for this use case
  attribs |= O_TMPFILE;
  attribs &= ~O_EXCL;  // allow relinking later
  nativeh.fd = ::openat(dirh.native_handle().fd, "", attribs, 0600);
  if(-1 != nativeh.fd)
  {
    ret.value()._flags |= flag::anonymous_inode;
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    return ret;
  }
  // If it failed, assume this kernel or FS doesn't support O_TMPFILE
  attribs &= ~O_TMPFILE;
  attribs |= O_EXCL;
#endif
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
      {
        continue;
      }
      return {errcode, std::system_category()};
    }
    // Immediately unlink after creation
    if(-1 == ::unlinkat(dirh.native_handle().fd, random.c_str(), 0))
    {
      return {errno, std::system_category()};
    }
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    return ret;
  }
}

file_handle::io_result<file_handle::const_buffers_type> file_handle::barrier(file_handle::io_request<file_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  (void) wait_for_device;
  (void) and_metadata;
  AFIO_LOG_FUNCTION_CALL(this);
  if(d)
  {
    return std::errc::not_supported;
  }
#ifdef __linux__
  if(!wait_for_device && !and_metadata)
  {
    // Linux has a lovely dedicated syscall giving us exactly what we need here
    extent_type offset = reqs.offset, bytes = 0;
    // empty buffers means bytes = 0 which means sync entire file
    for(const auto &req : reqs.buffers)
    {
      bytes += req.len;
    }
    unsigned flags = SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE;  // start writing all dirty pages in range now
    if(-1 != ::sync_file_range(_v.fd, offset, bytes, flags))
    {
      return {reqs.buffers};
    }
  }
#endif
#if !defined(__FreeBSD__) && !defined(__APPLE__)  // neither of these have fdatasync()
  if(!and_metadata)
  {
    if(-1 == ::fdatasync(_v.fd))
    {
      return {errno, std::system_category()};
    }
    return {reqs.buffers};
  }
#endif
#ifdef __APPLE__
  if(!wait_for_device)
  {
    // OS X fsync doesn't wait for the device to flush its buffers
    if(-1 == ::fsync(_v.fd))
      return {errno, std::system_category()};
    return {std::move(reqs.buffers)};
  }
  // This is the fsync as on every other OS
  if(-1 == ::fcntl(_v.fd, F_FULLFSYNC))
    return {errno, std::system_category()};
#else
  if(-1 == ::fsync(_v.fd))
  {
    return {errno, std::system_category()};
  }
#endif
  return {reqs.buffers};
}

result<file_handle> file_handle::clone(mode mode_, caching caching_, deadline d) const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // Fast path
  if(mode_ == mode::unchanged)
  {
    result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, caching_, _flags));
    ret.value()._service = _service;
    ret.value()._v.behaviour = _v.behaviour;
    ret.value()._v.fd = ::fcntl(_v.fd, F_DUPFD_CLOEXEC);
    if(-1 == ret.value()._v.fd)
    {
      return {errno, std::system_category()};
    }
    if(caching_ == caching::unchanged)
    {
      return ret;
    }

    int attribs = fcntl(ret.value()._v.fd, F_GETFL);
    if(-1 != attribs)
    {
      attribs &= ~(O_SYNC
#ifdef O_DIRECT
                   | O_DIRECT
#endif
#ifdef O_DSYNC
                   | O_DSYNC
#endif
                   );
      switch(caching_)
      {
      case caching::unchanged:
        break;
      case caching::none:
        attribs |= O_SYNC
#ifdef O_DIRECT
          | O_DIRECT
#endif
          ;
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return {errno, std::system_category()};
        }
        ret.value()._v.behaviour |= native_handle_type::disposition::aligned_io;
        break;
      case caching::only_metadata:
#ifdef O_DIRECT
        attribs |= O_DIRECT;
#endif
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return {errno, std::system_category()};
        }
        ret.value()._v.behaviour |= native_handle_type::disposition::aligned_io;
        break;
      case caching::reads:
        attribs |= O_SYNC;
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return {errno, std::system_category()};
        }
        ret.value()._v.behaviour &= ~native_handle_type::disposition::aligned_io;
        break;
      case caching::reads_and_metadata:
#ifdef O_DSYNC
        attribs |= O_DSYNC;
#else
        attribs |= O_SYNC;
#endif
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return {errno, std::system_category()};
        }
        ret.value()._v.behaviour &= ~native_handle_type::disposition::aligned_io;
        break;
      case caching::all:
      case caching::safety_fsyncs:
      case caching::temporary:
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return {errno, std::system_category()};
        }
        ret.value()._v.behaviour &= ~native_handle_type::disposition::aligned_io;
        break;
      }
      return ret;
    }
  }
  // Slow path
  std::chrono::steady_clock::time_point began_steady;
  std::chrono::system_clock::time_point end_utc;
  if(d)
  {
    if(d.steady)
    {
      began_steady = std::chrono::steady_clock::now();
    }
    else
    {
      end_utc = d.to_time_point();
    }
  }
  for(;;)
  {
    // Get the current path of myself
    OUTCOME_TRY(currentpath, current_path());
    // Open myself
    auto fh = file({}, currentpath, mode_, creation::open_existing, caching_, _flags);
    if(fh)
    {
      if(fh.value().unique_id() == unique_id())
      {
        return fh;
      }
    }
    else
    {
      if(fh.error() != std::errc::no_such_file_or_directory)
      {
        return fh.error();
      }
    }
    // Check timeout
    if(d)
    {
      if(d.steady)
      {
        if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds(d.nsecs)))
        {
          return std::errc::timed_out;
        }
      }
      else
      {
        if(std::chrono::system_clock::now() >= end_utc)
        {
          return std::errc::timed_out;
        }
      }
    }
  }
}

result<file_handle::extent_type> file_handle::length() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  struct stat s
  {
  };
  memset(&s, 0, sizeof(s));
  if(-1 == ::fstat(_v.fd, &s))
  {
    return {errno, std::system_category()};
  }
  return s.st_size;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(ftruncate(_v.fd, newsize) < 0)
  {
    return {errno, std::system_category()};
  }
  if(are_safety_fsyncs_issued())
  {
    fsync(_v.fd);
  }
  return newsize;
}

result<std::vector<std::pair<file_handle::extent_type, file_handle::extent_type>>> file_handle::extents() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  try
  {
    std::vector<std::pair<file_handle::extent_type, file_handle::extent_type>> out;
    out.reserve(64);
    extent_type start = 0, end = 0;
    for(;;)
    {
#ifdef __linux__
      start = lseek64(_v.fd, end, SEEK_DATA);
      if(static_cast<extent_type>(-1) == start)
      {
        break;
      }
      end = lseek64(_v.fd, start, SEEK_HOLE);
      if(static_cast<extent_type>(-1) == end)
      {
        break;
      }
#elif defined(__APPLE__)
      // Can't find any support for extent enumeration in OS X
      errno = EINVAL;
      break;
#elif defined(__FreeBSD__)
      start = lseek(_v.fd, end, SEEK_DATA);
      if((extent_type) -1 == start)
        break;
      end = lseek(_v.fd, start, SEEK_HOLE);
      if((extent_type) -1 == end)
        break;
#else
#error Unknown system
#endif
      // Data region may have been concurrently deleted
      if(end > start)
      {
        out.emplace_back(start, end - start);
      }
    }
    if(ENXIO != errno)
    {
      if(EINVAL == errno)
      {
        // If it failed with no output, probably this filing system doesn't support extents
        if(out.empty())
        {
          OUTCOME_TRY(size, file_handle::length());
          out.emplace_back(0, size);
          return out;
        }
      }
      else
      {
        return {errno, std::system_category()};
      }
    }
#if 0
  // A problem with SEEK_DATA and SEEK_HOLE is that they are racy under concurrent extents changing
  // Coalesce sequences of contiguous data e.g. 0, 64; 64, 64; 128, 64 ...
  std::vector<std::pair<extent_type, extent_type>> outfixed; outfixed.reserve(out.size());
  outfixed.push_back(out.front());
  for (size_t n = 1; n<out.size(); n++)
  {
    if (outfixed.back().first + outfixed.back().second == out[n].first)
      outfixed.back().second += out[n].second;
    else
      outfixed.push_back(out[n]);
  }
  return outfixed;
#endif
    return out;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<file_handle::extent_type> file_handle::zero(file_handle::extent_type offset, file_handle::extent_type bytes, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
#if defined(__linux__)
  if(-1 == fallocate(_v.fd, 0x02 /*FALLOC_FL_PUNCH_HOLE*/ | 0x01 /*FALLOC_FL_KEEP_SIZE*/, offset, bytes))
  {
    // The filing system may not support trim
    if(EOPNOTSUPP != errno)
    {
      return {errno, std::system_category()};
    }
  }
#endif
  // Fall back onto a write of zeros
  if(bytes < utils::page_size())
  {
    auto *buffer = static_cast<char *>(alloca(bytes));
    memset(buffer, 0, bytes);
    OUTCOME_TRY(written, write(offset, buffer, bytes, d));
    return written.len;
  }
  try
  {
    extent_type ret = 0;
    auto blocksize = utils::file_buffer_default_size();
    char *buffer = utils::page_allocator<char>().allocate(blocksize);
    auto unbufferh = undoer([buffer, blocksize] { utils::page_allocator<char>().deallocate(buffer, blocksize); });
    (void) unbufferh;
    while(bytes > 0)
    {
      auto towrite = (bytes < blocksize) ? bytes : blocksize;
      OUTCOME_TRY(written, write(offset, buffer, towrite, d));
      offset += written.len;
      bytes -= written.len;
      ret += written.len;
    }
    return ret;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

AFIO_V2_NAMESPACE_END
