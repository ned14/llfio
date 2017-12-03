/* A handle to a source of mapped memory
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (10 commits)
File Created: Apr 2017


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

#include "../../../map_handle.hpp"
#include "../../../utils.hpp"

#include <sys/mman.h>

AFIO_V2_NAMESPACE_BEGIN

section_handle::~section_handle()
{
  if(_v)
  {
    auto ret = section_handle::close();
    if(ret.has_error())
    {
      AFIO_LOG_FATAL(_v.h, "section_handle::~section_handle() close failed");
      abort();
    }
  }
}
result<void> section_handle::close() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(_v)
  {
    // We don't want ~handle() to close our handle borrowed from the backing file or _anonymous
    _v = native_handle_type();
    OUTCOME_TRYV(handle::close());
    OUTCOME_TRYV(_anonymous.close());
    _flag = flag::none;
  }
  return success();
}

result<section_handle> section_handle::section(file_handle &backing, extent_type maximum_size, flag _flag) noexcept
{
  OUTCOME_TRY(length, backing.length());
  if(maximum_size == 0)
  {
    maximum_size = length;
  }
  else if(maximum_size > length)
  {
    // For compatibility with Windows, disallow sections larger than the file
    return std::errc::value_too_large;
  }
  result<section_handle> ret(section_handle(native_handle_type(), &backing, file_handle(), _flag));
  native_handle_type &nativeh = ret.value()._v;
  nativeh.fd = backing.native_handle().fd;
  if(_flag & flag::read)
    nativeh.behaviour |= native_handle_type::disposition::readable;
  if(_flag & flag::write)
    nativeh.behaviour |= native_handle_type::disposition::writable;
  nativeh.behaviour |= native_handle_type::disposition::section;
  AFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

result<section_handle> section_handle::section(extent_type bytes, const path_handle &dirh, flag _flag) noexcept
{
  OUTCOME_TRY(_anonh, file_handle::temp_inode(dirh));
  OUTCOME_TRYV(_anonh.truncate(bytes));
  result<section_handle> ret(section_handle(native_handle_type(), nullptr, std::move(_anonh), _flag));
  native_handle_type &nativeh = ret.value()._v;
  file_handle &anonh = ret.value()._anonymous;
  nativeh.fd = anonh.native_handle().fd;
  if(_flag & flag::read)
    nativeh.behaviour |= native_handle_type::disposition::readable;
  if(_flag & flag::write)
    nativeh.behaviour |= native_handle_type::disposition::writable;
  nativeh.behaviour |= native_handle_type::disposition::section;
  AFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

result<section_handle::extent_type> section_handle::length() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  struct stat s;
  memset(&s, 0, sizeof(s));
  if(-1 == ::fstat(_v.fd, &s))
    return {errno, std::system_category()};
  return s.st_size;
}

result<section_handle::extent_type> section_handle::truncate(extent_type newsize) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(!_backing)
  {
    if(-1 == ::ftruncate(_anonymous.native_handle().fd, newsize))
      return {errno, std::system_category()};
  }
  return newsize;
}


/******************************************* map_handle *********************************************/


map_handle::~map_handle()
{
  if(_v)
  {
    // Unmap the view
    auto ret = map_handle::close();
    if(ret.has_error())
    {
      AFIO_LOG_FATAL(_v.fd, "map_handle::~map_handle() close failed");
      abort();
    }
  }
}

result<void> map_handle::close() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(_addr)
  {
    if(is_writable() && (_flag & section_handle::flag::barrier_on_close))
    {
      OUTCOME_TRYV(barrier({}, true, false));
    }
    // printf("%d munmap %p-%p\n", getpid(), _addr, _addr+_length);
    if(-1 == ::munmap(_addr, _length))
      return {errno, std::system_category()};
  }
  // We don't want ~handle() to close our borrowed handle
  _v = native_handle_type();
  _addr = nullptr;
  _length = 0;
  return success();
}

native_handle_type map_handle::release() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // We don't want ~handle() to close our borrowed handle
  _v = native_handle_type();
  _addr = nullptr;
  _length = 0;
  return native_handle_type();
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::barrier(map_handle::io_request<map_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  char *addr = _addr + reqs.offset;
  extent_type bytes = 0;
  for(const auto &req : reqs.buffers)
  {
    if(bytes + req.len < bytes)
      return std::errc::value_too_large;
    bytes += req.len;
  }
  if(reqs.buffers.empty())
  {
    bytes = _length;
  }
  int flags = (wait_for_device || and_metadata) ? MS_SYNC : MS_ASYNC;
  if(-1 == ::msync(addr, bytes, flags))
    return {errno, std::system_category()};
  if(_section->backing() && (wait_for_device || and_metadata))
  {
    reqs.offset += _offset;
    return _section->backing()->barrier(std::move(reqs), wait_for_device, and_metadata, d);
  }
  return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
}


static inline result<void *> do_mmap(native_handle_type &nativeh, void *ataddr, section_handle *section, map_handle::size_type &bytes, map_handle::extent_type offset, section_handle::flag _flag) noexcept
{
  bool have_backing = section ? (section->backing() != nullptr) : false;
  int prot = 0, flags = have_backing ? MAP_SHARED : (MAP_PRIVATE | MAP_ANONYMOUS);
  void *addr = nullptr;
  if(_flag == section_handle::flag::none)
  {
    prot |= PROT_NONE;
  }
  else if(_flag & section_handle::flag::cow)
  {
    prot |= PROT_READ | PROT_WRITE;
    flags &= ~MAP_SHARED;
    flags |= MAP_PRIVATE;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::write)
  {
    prot |= PROT_READ | PROT_WRITE;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::read)
  {
    prot |= PROT_READ;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
  }
  if(_flag & section_handle::flag::execute)
    prot |= PROT_EXEC;
#ifdef MAP_NORESERVE
  if(_flag & section_handle::flag::nocommit)
    flags |= MAP_NORESERVE;
#endif
#ifdef MAP_POPULATE
  if(_flag & section_handle::flag::prefault)
    flags |= MAP_POPULATE;
#endif
#ifdef MAP_PREFAULT_READ
  if(_flag & section_handle::flag::prefault)
    flags |= MAP_PREFAULT_READ;
#endif
#ifdef MAP_NOSYNC
  if(have_backing && (section->backing()->kernel_caching() == handle::caching::temporary))
    flags |= MAP_NOSYNC;
#endif
  // printf("mmap(%p, %u, %d, %d, %d, %u)\n", ataddr, (unsigned) bytes, prot, flags, have_backing ? section->native_handle().fd : -1, (unsigned) offset);
  addr = ::mmap(ataddr, bytes, prot, flags, have_backing ? section->native_handle().fd : -1, offset);
  // printf("%d mmap %p-%p\n", getpid(), addr, (char *) addr+bytes);
  if(MAP_FAILED == addr)
    return {errno, std::system_category()};
#if 0  // not implemented yet, not seen any benefit over setting this at the fd level
  if(have_backing && ((flags & map_handle::flag::disable_prefetching) || (flags & map_handle::flag::maximum_prefetching)))
  {
    int advice = (flags & map_handle::flag::disable_prefetching) ? MADV_RANDOM : MADV_SEQUENTIAL;
    if(-1 == ::madvise(addr, bytes, advice))
      return {errno, std::system_category()};
  }
#endif
  return addr;
}

result<map_handle> map_handle::map(size_type bytes, section_handle::flag _flag) noexcept
{
  if(!bytes)
    return std::errc::argument_out_of_domain;
  bytes = utils::round_up_to_page_size(bytes);
  result<map_handle> ret(map_handle(nullptr));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(addr, do_mmap(nativeh, nullptr, nullptr, bytes, 0, _flag));
  ret.value()._addr = (char *) addr;
  ret.value()._length = bytes;
  AFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

result<map_handle> map_handle::map(section_handle &section, size_type bytes, extent_type offset, section_handle::flag _flag) noexcept
{
  if(!bytes)
  {
    if(!section.backing())
      return std::errc::argument_out_of_domain;
    bytes = section.length().value();
  }
  result<map_handle> ret{map_handle(&section)};
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(addr, do_mmap(nativeh, nullptr, &section, bytes, offset, _flag));
  ret.value()._addr = (char *) addr;
  ret.value()._offset = offset;
  ret.value()._length = _bytes;
  // Make my handle borrow the native handle of my backing storage
  ret.value()._v.fd = section.native_handle().fd;
  AFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

result<map_handle::buffer_type> map_handle::commit(buffer_type region, section_handle::flag _flag) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(!region.data)
    return std::errc::invalid_argument;
  // Set permissions on the pages
  region = utils::round_to_page_size(region);
  extent_type offset = _offset + (region.data - _addr);
  size_type bytes = region.len;
  OUTCOME_TRYV(do_mmap(_v, region.data, _section, bytes, offset, _flag));
  // Tell the kernel we will be using these pages soon
  if(-1 == ::madvise(region.data, region.len, MADV_WILLNEED))
    return {errno, std::system_category()};
  return region;
}

result<map_handle::buffer_type> map_handle::decommit(buffer_type region) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(!region.data)
    return std::errc::invalid_argument;
  region = utils::round_to_page_size(region);
  // Tell the kernel to kick these pages into storage
  if(-1 == ::madvise(region.data, region.len, MADV_DONTNEED))
    return {errno, std::system_category()};
  // Set permissions on the pages to no access
  extent_type offset = _offset + (region.data - _addr);
  size_type bytes = region.len;
  OUTCOME_TRYV(do_mmap(_v, region.data, _section, bytes, offset, section_handle::flag::none));
  return region;
}

result<void> map_handle::zero_memory(buffer_type region) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(!region.data)
    return std::errc::invalid_argument;
#ifdef MADV_REMOVE
  buffer_type page_region{(char *) utils::round_up_to_page_size((uintptr_t) region.data), utils::round_down_to_page_size(region.len)};
  // Zero contents and punch a hole in any backing storage
  if(page_region.len && -1 != ::madvise(page_region.data, page_region.len, MADV_REMOVE))
  {
    memset(region.data, 0, page_region.data - region.data);
    memset(page_region.data + page_region.len, 0, (region.data + region.len) - (page_region.data + page_region.len));
    return success();
  }
#endif
  //! Only Linux implements syscall zero(), and it's covered by MADV_REMOVE already
  memset(region.data, 0, region.len);
  return success();
}

result<span<map_handle::buffer_type>> map_handle::prefetch(span<buffer_type> regions) noexcept
{
  AFIO_LOG_FUNCTION_CALL(0);
  for(const auto &region : regions)
  {
    if(-1 == ::madvise(region.data, region.len, MADV_WILLNEED))
      return {errno, std::system_category()};
  }
  return regions;
}

result<map_handle::buffer_type> map_handle::do_not_store(buffer_type region) noexcept
{
  AFIO_LOG_FUNCTION_CALL(0);
  region = utils::round_to_page_size(region);
  if(!region.data)
    return std::errc::invalid_argument;
#ifdef MADV_FREE
  // Lightweight unset of dirty bit for these pages. Needs FreeBSD or very recent Linux.
  if(-1 != ::madvise(region.data, region.len, MADV_FREE))
    return region;
#endif
#ifdef MADV_REMOVE
  // This is rather heavy weight in that it also punches a hole in any backing storage
  // but it works on Linux for donkey's years
  if(-1 != ::madvise(region.data, region.len, MADV_REMOVE))
    return region;
#endif
  // No support on this platform
  region.len = 0;
  return region;
}

map_handle::io_result<map_handle::buffers_type> map_handle::read(io_request<buffers_type> reqs, deadline) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  char *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? (size_type)(_length - reqs.offset) : 0;
  for(buffer_type &req : reqs.buffers)
  {
    if(togo)
    {
      req.data = addr;
      if(req.len > togo)
        req.len = togo;
      addr += req.len;
      togo -= req.len;
    }
    else
      req.len = 0;
  }
  return reqs.buffers;
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::write(io_request<const_buffers_type> reqs, deadline) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  char *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? (size_type)(_length - reqs.offset) : 0;
  for(const_buffer_type &req : reqs.buffers)
  {
    if(togo)
    {
      if(req.len > togo)
        req.len = togo;
      memcpy(addr, req.data, req.len);
      req.data = addr;
      addr += req.len;
      togo -= req.len;
    }
    else
      req.len = 0;
  }
  return reqs.buffers;
}

AFIO_V2_NAMESPACE_END
