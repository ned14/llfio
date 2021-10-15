/* A handle to something
(C) 2015-2021 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
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

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#endif

#ifdef __linux__
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#endif

//#include <iostream>

LLFIO_V2_NAMESPACE_BEGIN

result<file_handle> file_handle::file(const path_handle &base, file_handle::path_view_type path, file_handle::mode _mode, file_handle::creation _creation,
                                      file_handle::caching _caching, file_handle::flag flags) noexcept
{
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  OUTCOME_TRY(auto &&attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, _creation, _caching, flags));
  attribs &= ~O_NONBLOCK;
  nativeh.behaviour &= ~native_handle_type::disposition::nonblocking;
  path_view::zero_terminated_rendered_path<> zpath(path);
  if(base.is_valid())
  {
    nativeh.fd = ::openat(base.native_handle().fd, zpath.c_str(), attribs, 0x1b0 /*660*/);
  }
  else
  {
    nativeh.fd = ::open(zpath.c_str(), attribs, 0x1b0 /*660*/);
  }
  if(-1 == nativeh.fd)
  {
    if((mode::write == _mode || mode::append == _mode) && creation::always_new == _creation && EEXIST == errno)
    {
      // Take a path handle to the directory containing the file
      auto path_parent = path.parent_path();
      path_handle dirh;
      if(base.is_valid() && path_parent.empty())
      {
        OUTCOME_TRY(auto &&dh, base.clone());
        dirh = path_handle(std::move(dh));
      }
      else if(!path_parent.empty())
      {
        OUTCOME_TRY(auto &&dh, path_handle::path(base, path_parent));
        dirh = std::move(dh);
      }
      // Create a randomly named file, and rename it over
      OUTCOME_TRY(auto &&rfh, uniquely_named_file(dirh, _mode, _caching, flags));
      auto r = rfh.relink(dirh, path.filename());
      if(r)
      {
        return std::move(rfh);
      }
      // If failed to rename, remove
      (void) rfh.unlink();
      return std::move(r).error();
    }
    return posix_error();
  }
  if((flags & flag::disable_prefetching) || (flags & flag::maximum_prefetching))
  {
#ifdef POSIX_FADV_SEQUENTIAL
    int advice = (flags & flag::disable_prefetching) ? POSIX_FADV_RANDOM : (POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
    if(-1 == ::posix_fadvise(nativeh.fd, 0, 0, advice))
    {
      return posix_error();
    }
#elif __APPLE__
    int advice = (flags & flag::disable_prefetching) ? 0 : 1;
    if(-1 == ::fcntl(nativeh.fd, F_RDAHEAD, advice))
    {
      return posix_error();
    }
#endif
  }
  if(_creation == creation::truncate_existing && ret.value().are_safety_barriers_issued())
  {
    fsync(nativeh.fd);
  }
  return ret;
}

result<file_handle> file_handle::temp_inode(const path_handle &dirh, mode _mode, flag flags) noexcept
{
  caching _caching = caching::temporary;
  // No need to check inode before unlink
  flags |= flag::unlink_on_first_close | flag::disable_safety_unlinks;
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, _caching, flags, nullptr));
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  // Open file exclusively to prevent collision
  OUTCOME_TRY(auto &&attribs, attribs_from_handle_mode_caching_and_flags(nativeh, _mode, creation::only_if_not_exist, _caching, flags));
  attribs &= ~O_NONBLOCK;
  nativeh.behaviour &= ~native_handle_type::disposition::nonblocking;
#ifdef O_TMPFILE
  // Linux has a special flag just for this use case
  attribs |= O_TMPFILE;
  attribs &= ~O_EXCL;  // allow relinking later
  nativeh.fd = ::openat(dirh.native_handle().fd, "", attribs, 0600);
  if(-1 != nativeh.fd)
  {
    ret.value()._flags |= flag::anonymous_inode;
    ret.value()._flags &= ~flag::unlink_on_first_close;  // It's already unlinked
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
      return posix_error(errcode);
    }
    // Immediately unlink after creation
    if(-1 == ::unlinkat(dirh.native_handle().fd, random.c_str(), 0))
    {
      return posix_error();
    }
    ret.value()._flags &= ~flag::unlink_on_first_close;
    return ret;
  }
}

result<file_handle> file_handle::reopen(mode mode_, caching caching_, deadline d) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  // Fast path
  if(mode_ == mode::unchanged)
  {
    result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, caching_, _flags, _ctx));
    ret.value()._v.behaviour = _v.behaviour;
    ret.value()._v.fd = ::fcntl(_v.fd, F_DUPFD_CLOEXEC, 0);
    if(-1 == ret.value()._v.fd)
    {
      return posix_error();
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
          return posix_error();
        }
        ret.value()._v.behaviour |= native_handle_type::disposition::aligned_io;
        break;
      case caching::only_metadata:
#ifdef O_DIRECT
        attribs |= O_DIRECT;
#endif
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return posix_error();
        }
        ret.value()._v.behaviour |= native_handle_type::disposition::aligned_io;
        break;
      case caching::reads:
        attribs |= O_SYNC;
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return posix_error();
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
          return posix_error();
        }
        ret.value()._v.behaviour &= ~native_handle_type::disposition::aligned_io;
        break;
      case caching::all:
      case caching::safety_barriers:
      case caching::temporary:
        if(-1 == fcntl(ret.value()._v.fd, F_SETFL, attribs))
        {
          return posix_error();
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
    OUTCOME_TRY(auto &&currentpath, current_path());
    if(currentpath.empty())
    {
      // Cannot reopen a file which has been unlinked
      return errc::no_such_file_or_directory;
    }
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
      if(fh.error() != errc::no_such_file_or_directory)
      {
        return std::move(fh).error();
      }
    }
    // Check timeout
    if(d)
    {
      if(d.steady)
      {
        if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds(d.nsecs)))
        {
          return errc::timed_out;
        }
      }
      else
      {
        if(std::chrono::system_clock::now() >= end_utc)
        {
          return errc::timed_out;
        }
      }
    }
  }
}

result<file_handle::extent_type> file_handle::maximum_extent() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  struct stat s
  {
  };
  memset(&s, 0, sizeof(s));
  if(-1 == ::fstat(_v.fd, &s))
  {
    return posix_error();
  }
  return s.st_size;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(ftruncate(_v.fd, newsize) < 0)
  {
    return posix_error();
  }
  if(are_safety_barriers_issued())
  {
    fsync(_v.fd);
  }
  return newsize;
}

result<std::vector<file_handle::extent_pair>> file_handle::extents() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  try
  {
    std::vector<file_handle::extent_pair> out;
    out.reserve(64);
    extent_type start = 0, end = 0;
    for(;;)
    {
#ifdef __linux__
#ifndef SEEK_DATA
      errno = EINVAL;
      break;
#else
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
#endif
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
          OUTCOME_TRY(auto &&size, file_handle::maximum_extent());
          out.emplace_back(0, size);
          return out;
        }
      }
      else
      {
        return posix_error();
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

result<file_handle::extent_pair> file_handle::clone_extents_to(file_handle::extent_pair extent, io_handle &dest_, io_handle::extent_type destoffset, deadline d,
                                                               bool force_copy_now, bool emulate_if_unsupported) noexcept
{
  try
  {
    LLFIO_LOG_FUNCTION_CALL(this);
    if(!dest_.is_writable())
    {
      return errc::bad_file_descriptor;
    }
    OUTCOME_TRY(auto &&mycurrentlength, maximum_extent());
    if(extent.offset == (extent_type) -1 && extent.length == (extent_type) -1)
    {
      extent.offset = 0;
      extent.length = mycurrentlength;
    }
    if(extent.offset + extent.length < extent.offset)
    {
      return errc::value_too_large;
    }
    if(destoffset + extent.length < destoffset)
    {
      return errc::value_too_large;
    }
    if(extent.length == 0)
    {
      return extent;
    }
    if(extent.offset >= mycurrentlength)
    {
      return {extent.offset, 0};
    }
    if(extent.offset + extent.length >= mycurrentlength)
    {
      extent.length = mycurrentlength - extent.offset;
    }
    LLFIO_POSIX_DEADLINE_TO_SLEEP_INIT(d);
    const extent_type blocksize = utils::file_buffer_default_size();
    byte *buffer = nullptr;
    auto unbufferh = make_scope_exit([&]() noexcept {
      if(buffer != nullptr)
        utils::page_allocator<byte>().deallocate(buffer, blocksize);
    });
    (void) unbufferh;
    extent_pair ret(extent.offset, 0);
    if(!dest_.is_regular())
    {
#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)
      while(extent.length > 0)
      {
#ifdef __APPLE__
        off_t written = extent.length;
        if(-1 == ::sendfile(_v.fd, dest_.native_handle().fd, extent.offset, &written, nullptr, 0))
#elif defined(__FreeBSD__)
        off_t written = 0;
        if(-1 == ::sendfile(_v.fd, dest_.native_handle().fd, extent.offset, extent.length, nullptr, &written, 0))
#else
        off64_t off_in = extent.offset, off_out = 0;
        auto written = ::splice(_v.fd, &off_in, dest_.native_handle().fd, &off_out, extent.length, 0);
        if(written < 0)
#endif
        {
          if(EAGAIN != errno && EWOULDBLOCK == errno)
          {
            if(ret.length == 0)
            {
              break;
            }
            return posix_error();
          }
        }
        extent.offset += written;
        destoffset += written;
        extent.length -= written;
        ret.length += written;
        if(extent.length == 0)
        {
          break;
        }
        if(!d || !d.steady || d.nsecs != 0)
        {
          LLFIO_POSIX_DEADLINE_TO_SLEEP_LOOP(d);
          int mstimeout = (timeout == nullptr) ? -1 : (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000000LL);
          pollfd p;
          memset(&p, 0, sizeof(p));
          p.fd = dest_.native_handle().fd;
          p.events = POLLOUT | POLLERR;
          if(-1 == ::poll(&p, 1, mstimeout))
          {
            return posix_error();
          }
        }
        LLFIO_POSIX_DEADLINE_TO_TIMEOUT_LOOP(d);
      }
      if(ret.length > 0)
      {
        return ret;
      }
#endif
      buffer = utils::page_allocator<byte>().allocate(blocksize);
      while(extent.length > 0)
      {
        deadline nd;
        const size_t towrite = (extent.length < blocksize) ? (size_t) extent.length : blocksize;
        buffer_type b(buffer, utils::round_up_to_page_size(towrite, 4096) /* to allow aligned i/o files */);
        LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
        OUTCOME_TRY(auto &&readed, read({{&b, 1}, extent.offset}, nd));
        const_buffer_type cb(readed.front().data(), std::min(readed.front().size(), towrite));
        if(cb.size() == 0)
        {
          return ret;
        }
        LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
        OUTCOME_TRY(auto &&written_, dest_.write({{&cb, 1}, destoffset}, nd));
        const auto written = written_.front().size();
        extent.offset += written;
        destoffset += written;
        extent.length -= written;
        ret.length += written;
      }
      return ret;
    }
    const auto destoffsetdiff = destoffset - extent.offset;
    struct workitem
    {
      extent_pair src;
      enum
      {
        copy_bytes,
        zero_bytes,
        clone_extents,
        delete_extents
      } op;
      bool destination_extents_are_new{false};
    };
    std::vector<workitem> todo;  // if destination length is 0, punch hole
    todo.reserve(8);
    // Firstly fill todo with the list of allocated and non-allocated extents
    {
#if defined(SEEK_DATA) && !defined(__APPLE__)
      /* Apple's SEEK_HOLE implementation is basically unusable. I discovered this the
      hard way :). There is lots of useful detail as to why at
      https://lists.gnu.org/archive/html/bug-gnulib/2018-09/msg00054.html
      */
      extent_type start = 0, end = 0;
      for(;;)
      {
#ifdef __linux__
        if(1)
        {
          /* There is a bug in ZFSOnLinux where SEEK_DATA returns no data in file
          if the file was rewritten using a mmap which is then closed, and the file
          immediately reopened and SEEK_DATA called upon it. If you read a single
          byte, it seems to kick SEEK_DATA into working correctly.

          See https://github.com/openzfs/zfs/issues/11697 for more.
          */
          alignas(4096) char b[4096];
          if(-1 == ::pread(_v.fd, b, requires_aligned_io() ? 4096 : 1, end))
          {
            return posix_error();
          }
        }
        start = lseek64(_v.fd, end, SEEK_DATA);
#else
        start = lseek(_v.fd, end, SEEK_DATA);
#endif
        // std::cout << "SEEK_DATA from " << end << " finds " << start << std::endl;
        if(static_cast<extent_type>(-1) == start)
        {
          if(ENXIO == errno)
          {
            // There is no data between end, and the file length
            start = mycurrentlength;
          }
          else if(start == 0)
          {
            // extents enumeration is not supported
            todo.push_back(workitem{extent_pair(0, mycurrentlength), workitem::clone_extents});
            break;
          }
        }
        if(end >= extent.offset + extent.length)
        {
          break;  // done
        }
        // std::cout << "There are deallocated extents between " << end << " " << (start - end) << " bytes" << std::endl;
        // hole goes from end to start. end is inclusive, start is exclusive.
        if((end <= extent.offset && start >= extent.offset + extent.length) || (end >= extent.offset && end < extent.offset + extent.length) ||
           (start > extent.offset && start <= extent.offset + extent.length))
        {
          const auto clampedstart = std::max(end, extent.offset);
          const auto clampedend = std::min(start, extent.offset + extent.length);
          todo.push_back(workitem{extent_pair(clampedstart, clampedend - clampedstart), workitem::delete_extents});
        }
        if(start >= extent.offset + extent.length)
        {
          break;  // done
        }
#ifdef __linux__
        end = lseek64(_v.fd, start, SEEK_HOLE);
#else
        end = lseek(_v.fd, start, SEEK_HOLE);
#endif
        // std::cout << "SEEK_HOLE from " << start << " finds " << end << std::endl;
        if(static_cast<extent_type>(-1) == end)
        {
          if(ENXIO == errno)
          {
            // There is no data between start, and the file length
            end = mycurrentlength;
          }
          else if(start == 0)
          {
            // extents enumeration is not supported
            todo.push_back(workitem{extent_pair(0, mycurrentlength), workitem::clone_extents});
            break;
          }
        }
        // std::cout << "There are allocated extents between " << start << " " << (end - start) << " bytes" << std::endl;
        // allocated goes from start to end. start is inclusive, end is exclusive.
        if((start <= extent.offset && end >= extent.offset + extent.length) || (start >= extent.offset && start < extent.offset + extent.length) ||
           (end > extent.offset && end <= extent.offset + extent.length))
        {
          const auto clampedstart = std::max(start, extent.offset);
          const auto clampedend = std::min(end, extent.offset + extent.length);
          todo.push_back(workitem{extent_pair(clampedstart, clampedend - clampedstart), workitem::clone_extents});
        }
      }
#else
      todo.push_back(workitem{{extent.offset, extent.length}, workitem::clone_extents});
#endif
    }
    // Handle there being insufficient source to fill dest
    if(todo.empty())
    {
      todo.push_back(workitem{{extent.offset, extent.length}, workitem::delete_extents});
    }
#ifndef NDEBUG
    {
      assert(todo.front().src.offset == extent.offset);
      assert(todo.back().src.offset + todo.back().src.length == extent.offset + extent.length);
      for(size_t n = 1; n < todo.size(); n++)
      {
        assert(todo[n - 1].src.offset + todo[n - 1].src.length == todo[n].src.offset);
      }
    }
#endif
    // If cloning within the same file, use the appropriate direction
    auto &dest = static_cast<file_handle &>(dest_);
    OUTCOME_TRY(auto &&dest_length, dest.maximum_extent());
    if(dest.unique_id() == unique_id())
    {
      if(abs((int64_t) destoffset - (int64_t) extent.offset) < (int64_t) blocksize)
      {
        return errc::invalid_argument;
      }
      if(destoffset > extent.offset)
      {
        std::reverse(todo.begin(), todo.end());
      }
    }
    // Ensure the destination file is big enough
    if(destoffset + extent.length > dest_length)
    {
      // No need to zero nor deallocate extents in the new length
      auto it = todo.begin();
      while(it != todo.end() && it->src.offset + it->src.length + destoffsetdiff <= dest_length)
      {
        ++it;
      }
      if(it != todo.end() && it->src.offset + destoffsetdiff < dest_length)
      {
        // If it is a zero or remove, split the block
        if(it->op == workitem::delete_extents || it->op == workitem::zero_bytes)
        {
          // TODO
        }
        ++it;
      }
      // Mark all remaining blocks as writing into new extents
      while(it != todo.end())
      {
        it->destination_extents_are_new = true;
        ++it;
      }
    }
    bool duplicate_extents = !force_copy_now, zero_extents = true, buffer_dirty = true;
    bool truncate_back_on_failure = false;
    if(dest_length < destoffset + extent.length)
    {
      OUTCOME_TRY(dest.truncate(destoffset + extent.length));
      truncate_back_on_failure = true;
    }
    auto untruncate = make_scope_exit([&]() noexcept {
      if(truncate_back_on_failure)
      {
        (void) dest.truncate(dest_length);
      }
    });
#if 0
    for(const workitem &item : todo)
    {
      std::cout << "  From offset " << item.src.offset << " " << item.src.length << " bytes do ";
      switch(item.op)
      {
      case workitem::copy_bytes:
        std::cout << "copy bytes";
        break;
      case workitem::zero_bytes:
        std::cout << "zero bytes";
        break;
      case workitem::clone_extents:
        std::cout << "clone extents";
        break;
      case workitem::delete_extents:
        std::cout << "delete extents";
        break;
      }
      if(item.destination_extents_are_new)
      {
        std::cout << " (destination extents are new)";
      }
      std::cout << std::endl;
    }
#endif
    auto _copy_file_range = [&](int infd, off_t *inoffp, int outfd, off_t *outoffp, size_t len, unsigned int flags) -> ssize_t {
#if defined(__linux__)
#if defined __aarch64__
      return syscall(285 /*__NR_copy_file_range*/, infd, inoffp, outfd, outoffp, len, flags);
#elif defined __arm__
      return syscall(391 /*__NR_copy_file_range*/, infd, inoffp, outfd, outoffp, len, flags);
#elif defined __i386__
      return syscall(377 /*__NR_copy_file_range*/, infd, inoffp, outfd, outoffp, len, flags);
#elif defined __powerpc64__
      return syscall(379 /*__NR_copy_file_range*/, infd, inoffp, outfd, outoffp, len, flags);
#elif defined __sparc__
      return syscall(357 /*__NR_copy_file_range*/, infd, inoffp, outfd, outoffp, len, flags);
#elif defined __x86_64__
      return syscall(326 /*__NR_copy_file_range*/, infd, inoffp, outfd, outoffp, len, flags);
#else
#error Unknown Linux platform
#endif
#elif defined(__FreeBSD__)
      // This gets implemented in FreeBSD 13. See https://reviews.freebsd.org/D20584
      return syscall(569 /*copy_file_range*/, intfd, inoffp, outfd, outoffp, len, flags);
#else
      (void) infd;
      (void) inoffp;
      (void) outfd;
      (void) outoffp;
      (void) len;
      (void) flags;
      errno = ENOSYS;
      return -1;
#endif
    };
    auto _zero_file_range = [&](int fd, off_t offset, size_t len) -> int {
#if defined(__linux__)
      return fallocate(fd, 0x02 /*FALLOC_FL_PUNCH_HOLE*/ | 0x01 /*FALLOC_FL_KEEP_SIZE*/, offset, len);
#else
      (void) fd;
      (void) offset;
      (void) len;
      errno = EOPNOTSUPP;
      return -1;
#endif
    };
    for(const workitem &item : todo)
    {
      for(extent_type thisoffset = 0; thisoffset < item.src.length; thisoffset += blocksize)
      {
      retry_clone:
        bool done = false;
        const auto thisblock = std::min(blocksize, item.src.length - thisoffset);
        if(duplicate_extents && item.op == workitem::clone_extents)
        {
          off_t off_in = item.src.offset + thisoffset, off_out = item.src.offset + thisoffset + destoffsetdiff;
          auto bytes_cloned = _copy_file_range(_v.fd, &off_in, dest.native_handle().fd, &off_out, thisblock, 0);
          if(bytes_cloned <= 0)
          {
            if(bytes_cloned < 0 && ((EXDEV != errno && EOPNOTSUPP != errno && ENOSYS != errno) || !emulate_if_unsupported))
            {
              return posix_error();
            }
            duplicate_extents = false;  // emulate using copy of bytes
          }
          else if((size_t) bytes_cloned == thisblock)
          {
            done = true;
          }
          else
          {
            thisoffset += bytes_cloned;
            goto retry_clone;
          }
        }
        if(!done && (item.op == workitem::copy_bytes || (!duplicate_extents && item.op == workitem::clone_extents)))
        {
          if(buffer == nullptr)
          {
            buffer = utils::page_allocator<byte>().allocate(blocksize);
          }
          deadline nd;
          buffer_type b(buffer, utils::round_up_to_page_size(thisblock, 4096) /* to allow aligned i/o files */);
          LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
          OUTCOME_TRY(auto &&readed, read({{&b, 1}, item.src.offset + thisoffset}, nd));
          buffer_dirty = true;
          if(readed.front().size() < thisblock)
          {
            return errc::resource_unavailable_try_again;  // something is wrong
          }
          readed.front() = {readed.front().data(), thisblock};
          LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
          const_buffer_type cb(readed.front().data(), thisblock);
          if(item.destination_extents_are_new)
          {
            // If we don't need to reset the bytes in the destination, try to elide
            // regions of zero bytes before writing to save on extents newly allocated
            const char *ds = (const char *) readed.front().data(), *e = (const char *) readed.front().data() + readed.front().size(), *zs, *ze;
            while(ds < e)
            {
              // Take zs to end of non-zero data
              for(zs = ds; zs < e && *zs != 0; ++zs)
              {
              }
            zero_block_too_small:
              // Take ze to end of zero data
              for(ze = zs; ze < e && *ze == 0; ++ze)
              {
              }
              if(ze - zs < 1024 && ze < e)
              {
                zs = ze + 1;
                goto zero_block_too_small;
              }
              if(zs != ds)
              {
                // Write portion from ds to zs
                cb = {(const byte *) ds, (size_t)(zs - ds)};
                auto localoffset = cb.data() - readed.front().data();
                // std::cout << "*** " << (item.src.offset + thisoffset + localoffset) << " - " << cb.size() << std::endl;
                OUTCOME_TRY(auto &&written, dest.write({{&cb, 1}, item.src.offset + thisoffset + localoffset + destoffsetdiff}, nd));
                if(written.front().size() != (size_t)(zs - ds))
                {
                  return errc::resource_unavailable_try_again;  // something is wrong
                }
              }
              ds = ze;
            }
          }
          else
          {
            // Straight write
            OUTCOME_TRY(auto &&written, dest.write({{&cb, 1}, item.src.offset + thisoffset + destoffsetdiff}, nd));
            if(written.front().size() != thisblock)
            {
              return errc::resource_unavailable_try_again;  // something is wrong
            }
          }
          done = true;
        }
        if(!done && !item.destination_extents_are_new && (zero_extents && item.op == workitem::delete_extents))
        {
          if(_zero_file_range(dest.native_handle().fd, item.src.offset + thisoffset + destoffsetdiff, thisblock) < 0)
          {
            if((EOPNOTSUPP != errno && ENOSYS != errno) || !emulate_if_unsupported)
            {
              return posix_error();
            }
            zero_extents = false;  // emulate using a write of bytes
          }
          else
          {
            done = true;
          }
        }
        if(!done && !item.destination_extents_are_new && (item.op == workitem::zero_bytes || (!zero_extents && item.op == workitem::delete_extents)))
        {
          if(buffer == nullptr)
          {
            buffer = utils::page_allocator<byte>().allocate(blocksize);
          }
          deadline nd;
          const_buffer_type cb(buffer, thisblock);
          if(buffer_dirty)
          {
            memset(buffer, 0, thisblock);
            buffer_dirty = false;
          }
          LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
          OUTCOME_TRY(auto &&written, dest.write({{&cb, 1}, item.src.offset + thisoffset + destoffsetdiff}, nd));
          if(written.front().size() != thisblock)
          {
            return errc::resource_unavailable_try_again;  // something is wrong
          }
          done = true;
        }
        dest_length = destoffset + extent.length;
        truncate_back_on_failure = false;
        LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
        ret.length += thisblock;
      }
    }
    return ret;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<file_handle::extent_type> file_handle::zero(file_handle::extent_pair extent, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(extent.offset + extent.length < extent.offset)
  {
    return errc::value_too_large;
  }
#if defined(__linux__)
  if(-1 == fallocate(_v.fd, 0x02 /*FALLOC_FL_PUNCH_HOLE*/ | 0x01 /*FALLOC_FL_KEEP_SIZE*/, extent.offset, extent.length))
  {
    // The filing system may not support trim
    if(EOPNOTSUPP != errno)
    {
      return posix_error();
    }
  }
#endif
  // Fall back onto a write of zeros
  if(extent.length < utils::page_size())
  {
    auto *buffer = static_cast<byte *>(alloca((size_type) extent.length));
    memset(buffer, 0, (size_type) extent.length);
    OUTCOME_TRY(auto &&written, write(extent.offset, {{buffer, (size_type) extent.length}}, d));
    return written;
  }
  try
  {
    extent_type ret = 0;
    auto blocksize = utils::file_buffer_default_size();
    byte *buffer = utils::page_allocator<byte>().allocate(blocksize);
    auto unbufferh = make_scope_exit([buffer, blocksize]() noexcept { utils::page_allocator<byte>().deallocate(buffer, blocksize); });
    (void) unbufferh;
    while(extent.length > 0)
    {
      auto towrite = (extent.length < blocksize) ? (size_t) extent.length : blocksize;
      OUTCOME_TRY(auto &&written, write(extent.offset, {{buffer, towrite}}, d));
      extent.offset += written;
      extent.length -= written;
      ret += written;
    }
    return ret;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

LLFIO_V2_NAMESPACE_END
