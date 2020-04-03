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

#include "quickcpplib/signal_guard.hpp"

#include <sys/mman.h>

//#define LLFIO_DEBUG_LINUX_MUNMAP

#ifdef LLFIO_DEBUG_LINUX_MUNMAP
#include <unistd.h>
static struct llfio_linux_munmap_debug_t
{
  int smaps_fd, dumpfile_fd;

  llfio_linux_munmap_debug_t()
  {
    smaps_fd = ::open("/proc/self/smaps", O_RDONLY);
    dumpfile_fd = ::open("/tmp/llfio_unmap_debug_smaps.txt", O_WRONLY | O_CREAT | O_APPEND, 0x1b0 /*660*/);
    if(-1 == smaps_fd || -1 == dumpfile_fd)
    {
      puts("llfio_linux_munmap_debug: Failed to open one of the files\n");
      abort();
    }
  }
} llfio_linux_munmap_debug;
#endif

LLFIO_V2_NAMESPACE_BEGIN

section_handle::~section_handle()
{
  if(_v)
  {
    auto ret = section_handle::close();
    if(ret.has_error())
    {
      LLFIO_LOG_FATAL(_v.h, "section_handle::~section_handle() close failed");
      abort();
    }
  }
}
result<void> section_handle::close() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
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

result<section_handle> section_handle::section(file_handle &backing, extent_type /* unused */, flag _flag) noexcept
{
  result<section_handle> ret(section_handle(native_handle_type(), &backing, file_handle(), _flag));
  native_handle_type &nativeh = ret.value()._v;
  nativeh.fd = backing.native_handle().fd;
  if(_flag & flag::read)
  {
    nativeh.behaviour |= native_handle_type::disposition::readable;
  }
  if(_flag & flag::write)
  {
    nativeh.behaviour |= native_handle_type::disposition::writable;
  }
  nativeh.behaviour |= native_handle_type::disposition::section;
  LLFIO_LOG_FUNCTION_CALL(&ret);
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
  {
    nativeh.behaviour |= native_handle_type::disposition::readable;
  }
  if(_flag & flag::write)
  {
    nativeh.behaviour |= native_handle_type::disposition::writable;
  }
  nativeh.behaviour |= native_handle_type::disposition::section;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

result<section_handle::extent_type> section_handle::length() const noexcept
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

result<section_handle::extent_type> section_handle::truncate(extent_type newsize) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if((_backing == nullptr) && newsize > 0)
  {
    if(-1 == ::ftruncate(_anonymous.native_handle().fd, newsize))
    {
      return posix_error();
    }
  }
  return newsize;
}


/******************************************* map_handle *********************************************/


map_handle::~map_handle()
{
  if(_addr != nullptr)
  {
    // Unmap the view
    auto ret = map_handle::close();
    if(ret.has_error())
    {
      LLFIO_LOG_FATAL(_v.fd, "map_handle::~map_handle() close failed. Cause is typically other code modifying mapped regions. If on Linux, you may have exceeded the 64k VMA process limit, set the LLFIO_DEBUG_LINUX_MUNMAP macro at the top of posix/map_handle.ipp to cause dumping of VMAs to "
                             "/tmp/llfio_unmap_debug_smaps.txt, and combine with strace to figure it out.");
      abort();
    }
  }
}

result<void> map_handle::close() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_addr != nullptr)
  {
    if(is_writable() && (_flag & section_handle::flag::barrier_on_close))
    {
      OUTCOME_TRYV(map_handle::barrier({}, barrier_kind::wait_all));
    }
    // printf("%d munmap %p-%p\n", getpid(), _addr, _addr+_reservation);
    if(-1 == ::munmap(_addr, _reservation))
    {
#ifdef LLFIO_DEBUG_LINUX_MUNMAP
      int olderrno = errno;
      ssize_t bytesread;
      // Refresh the /proc file
      (void) ::lseek(llfio_linux_munmap_debug.smaps_fd, 0, SEEK_END);
      (void) ::lseek(llfio_linux_munmap_debug.smaps_fd, 0, SEEK_SET);
      char buffer[4096];
      (void) ::write(llfio_linux_munmap_debug.dumpfile_fd, buffer, sprintf(buffer, "\n---\nCause of munmap failure by process %d: %d (%s)\n\n", getpid(), olderrno, strerror(olderrno)));
      do
      {
        bytesread = ::read(llfio_linux_munmap_debug.smaps_fd, buffer, sizeof(buffer));
        if(bytesread > 0)
        {
          (void) ::write(llfio_linux_munmap_debug.dumpfile_fd, buffer, bytesread);
        }
      } while(bytesread > 0);
      errno = olderrno;
#endif
      return posix_error();
    }
  }
  // We don't want ~handle() to close our borrowed handle
  _v = native_handle_type();
  _addr = nullptr;
  _length = 0;
  return success();
}

native_handle_type map_handle::release() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  // We don't want ~handle() to close our borrowed handle
  _v = native_handle_type();
  _addr = nullptr;
  _length = 0;
  return {};
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::_do_barrier(map_handle::io_request<map_handle::const_buffers_type> reqs, barrier_kind kind, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  byte *addr = _addr + reqs.offset;
  size_type bytes = 0;
  // Check for overflow
  for(const auto &req : reqs.buffers)
  {
    if(bytes + req.size() < bytes)
    {
      return errc::value_too_large;
    }
    bytes += req.size();
  }
  // If empty, do the whole file
  if(reqs.buffers.empty())
  {
    bytes = _length;
  }
  // If nvram and not syncing metadata, use lightweight barrier
  if((kind == barrier_kind::nowait_data_only || kind == barrier_kind::wait_data_only) && is_nvram())
  {
    auto synced = nvram_barrier({addr, bytes});
    if(synced.size() >= bytes)
    {
      return {reqs.buffers};
    }
  }
  int flags = (kind != barrier_kind::nowait_data_only) ? MS_SYNC : MS_ASYNC;
  if(-1 == ::msync(addr, bytes, flags))
  {
    return posix_error();
  }
  // Don't fsync temporary inodes
  if(_section != nullptr && (_section->backing() != nullptr) && kind != barrier_kind::nowait_data_only)
  {
    reqs.offset += _offset;
    return _section->backing()->barrier(reqs, kind, d);
  }
  return {reqs.buffers};
}


static inline result<void *> do_mmap(native_handle_type &nativeh, void *ataddr, int extra_flags, section_handle *section, map_handle::size_type pagesize, map_handle::size_type &bytes, map_handle::extent_type offset, section_handle::flag _flag) noexcept
{
  bool have_backing = (section != nullptr);
  int prot = 0, flags = have_backing ? MAP_SHARED : (MAP_PRIVATE | MAP_ANONYMOUS);
  void *addr = nullptr;
  if(_flag == section_handle::flag::none)
  {
    prot |= PROT_NONE;
#ifdef MAP_GUARD
    if(_flag & section_handle::flag::nocommit)
    {
      flags |= MAP_GUARD;
    }
#endif
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
    prot = (_flag & section_handle::flag::write_via_syscall) ? PROT_READ : (PROT_READ | PROT_WRITE);
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::read)
  {
    prot |= PROT_READ;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
  }
  if(_flag & section_handle::flag::execute)
  {
    prot |= PROT_EXEC;
  }
#ifdef MAP_NORESERVE
  if(_flag & section_handle::flag::nocommit)
  {
    flags |= MAP_NORESERVE;
  }
#endif
#ifdef MAP_POPULATE
  if(_flag & section_handle::flag::prefault)
  {
    flags |= MAP_POPULATE;
  }
#endif
#ifdef MAP_PREFAULT_READ
  if(_flag & section_handle::flag::prefault)
    flags |= MAP_PREFAULT_READ;
#endif
#ifdef MAP_NOSYNC
  if(have_backing && section->backing() != nullptr && (section->backing()->kernel_caching() == handle::caching::temporary))
    flags |= MAP_NOSYNC;
#endif
  flags |= extra_flags;
  int fd_to_use = have_backing ? section->native_handle().fd : -1;
  if(pagesize != utils::page_size())
  {
    static const auto &pagesizes = utils::page_sizes();  // can't throw, as guaranteed called before now
#ifdef __linux__
    flags |= MAP_HUGETLB;  // gets me pagesizes[1]
    if(pagesize > pagesizes[1])
    {
#ifdef MAP_HUGE_SHIFT
      // Ask for page size requested
      size_t topbitset = (__CHAR_BIT__ * sizeof(unsigned long)) - __builtin_clzl((unsigned long) pagesize);
      flags |= topbitset << MAP_HUGE_SHIFT;
#else
      return errc::invalid_argument;
#endif
    }
#elif defined(__FreeBSD__)
    size_t topbitset = (__CHAR_BIT__ * sizeof(unsigned long)) - __builtin_clzl((unsigned long) pagesize);
    flags |= MAP_ALIGNED(topbitset);
#elif defined(__APPLE__)
    size_t topbitset = (__CHAR_BIT__ * sizeof(unsigned long)) - __builtin_clzl((unsigned long) pagesize);
    if(have_backing || topbitset < 21)
    {
      (void) pagesizes;
      return errc::invalid_argument;
    }
    fd_to_use = ((topbitset - 20) << VM_FLAGS_SUPERPAGE_SHIFT);
#else
#error Do not know how to specify large/huge/super pages on this platform
#endif
  }
// printf("mmap(%p, %u, %d, %d, %d, %u)\n", ataddr, (unsigned) bytes, prot, flags, have_backing ? section->native_handle().fd : -1, (unsigned) offset);
#ifdef MAP_SYNC  // Linux kernel 4.15 or later only
  // If backed by a file into persistent shared memory, ask the kernel to use persistent memory safe semantics
  if(have_backing && (_flag & section_handle::flag::nvram) && (flags & MAP_SHARED) != 0)
  {
    int flagscopy = flags & ~MAP_SHARED;
    flagscopy |= MAP_SHARED_VALIDATE | MAP_SYNC;
    addr = ::mmap(ataddr, bytes, prot, flagscopy, fd_to_use, offset);
  }
#endif
  if(addr == nullptr)
  {
    addr = ::mmap(ataddr, bytes, prot, flags, fd_to_use, offset);
  }
  // printf("%d mmap %p-%p\n", getpid(), addr, (char *) addr+bytes);
  if(MAP_FAILED == addr)  // NOLINT
  {
    return posix_error();
  }
#ifdef MADV_FREE_REUSABLE
  if((prot & PROT_WRITE) != 0 && (_flag & section_handle::flag::nocommit))
  {
    // On Mac OS, you cannot ask for reserved memory, but you can ask
    // for committed memory first and then decommit it. Which can potentially
    // blow out the commit limit for really large reservations,
    // but this is literally the only game in town on Mac OS.
    if(-1 == ::madvise(addr, bytes, MADV_FREE_REUSABLE))
    {
      return posix_error();
    }
  }
#endif
#if 0  // not implemented yet, not seen any benefit over setting this at the fd level
  if(have_backing && ((flags & map_handle::flag::disable_prefetching) || (flags & map_handle::flag::maximum_prefetching)))
  {
    int advice = (flags & map_handle::flag::disable_prefetching) ? MADV_RANDOM : MADV_SEQUENTIAL;
    if(-1 == ::madvise(addr, bytes, advice))
      return posix_error();
  }
#endif
  return addr;
}

result<map_handle> map_handle::map(size_type bytes, bool /*unused*/, section_handle::flag _flag) noexcept
{
  // TODO: Keep a cache of MADV_FREE pages deallocated
  if(bytes == 0u)
  {
    return errc::argument_out_of_domain;
  }
  bytes = utils::round_up_to_page_size(bytes, /*FIXME*/ utils::page_size());
  result<map_handle> ret(map_handle(nullptr, _flag));
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(pagesize, detail::pagesize_from_flags(ret.value()._flag));
  OUTCOME_TRY(addr, do_mmap(nativeh, nullptr, 0, nullptr, pagesize, bytes, 0, ret.value()._flag));
  ret.value()._addr = static_cast<byte *>(addr);
  ret.value()._reservation = bytes;
  ret.value()._length = bytes;
  ret.value()._pagesize = pagesize;
  nativeh._init = -2;  // otherwise appears closed
  nativeh.behaviour |= native_handle_type::disposition::allocation;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

result<map_handle> map_handle::map(section_handle &section, size_type bytes, extent_type offset, section_handle::flag _flag) noexcept
{
  OUTCOME_TRY(length, section.length());  // length of the backing file
  if(bytes == 0u)
  {
    bytes = length - offset;
  }
  result<map_handle> ret{map_handle(&section, _flag)};
  native_handle_type &nativeh = ret.value()._v;
  OUTCOME_TRY(pagesize, detail::pagesize_from_flags(ret.value()._flag));
  OUTCOME_TRY(addr, do_mmap(nativeh, nullptr, 0, &section, pagesize, bytes, offset, ret.value()._flag));
  ret.value()._addr = static_cast<byte *>(addr);
  ret.value()._offset = offset;
  ret.value()._reservation = utils::round_up_to_page_size(bytes, pagesize);
  ret.value()._length = (length - offset < bytes) ? (length - offset) : bytes;  // length of backing, not reservation
  ret.value()._pagesize = pagesize;
  // Make my handle borrow the native handle of my backing storage
  ret.value()._v.fd = section.native_handle().fd;
  nativeh.behaviour |= native_handle_type::disposition::allocation;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  return ret;
}

// Change the address reservation for this map
result<map_handle::size_type> map_handle::truncate(size_type newsize, bool permit_relocation) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  extent_type length = _length;
  if(_section != nullptr)
  {
    OUTCOME_TRY(length_, _section->length());  // length of the backing file
    length = length_;
  }
  auto _newsize = utils::round_up_to_page_size(newsize, _pagesize);
  if(_newsize == _reservation)
  {
    return success();
  }
  // If wiping the map ...
  if(newsize == 0)
  {
    if(-1 == ::munmap(_addr, _reservation))
    {
      return posix_error();
    }
    _addr = nullptr;
    _reservation = 0;
    _length = 0;
    return 0;
  }
  // If not mapped yet ...
  if(_addr == nullptr)
  {
    OUTCOME_TRY(addr, do_mmap(_v, nullptr, 0, _section, _pagesize, newsize, _offset, _flag));
    _addr = static_cast<byte *>(addr);
    _reservation = _newsize;
    _length = (length - _offset < newsize) ? (length - _offset) : newsize;  // length of backing, not reservation
    return newsize;
  }
#ifdef __linux__
  // Dead easy on Linux
  void *newaddr = ::mremap(_addr, _reservation, newsize, permit_relocation ? MREMAP_MAYMOVE : 0);
  if(MAP_FAILED == newaddr)
  {
    return posix_error();
  }
  _addr = static_cast<byte *>(newaddr);
  _reservation = _newsize;
  _length = (length - _offset < newsize) ? (length - _offset) : newsize;  // length of backing, not reservation
  return newsize;
#else
  (void) permit_relocation;
  // Try to expand reservation in place
  if(newsize > _reservation)
  {
#if defined(MAP_EXCL)  // BSD type systems
    byte *addrafter = _addr + _reservation;
    size_type bytes = newsize - _reservation;
    extent_type offset = _offset + _reservation;
    OUTCOME_TRY(addr, do_mmap(_v, addrafter, MAP_FIXED | MAP_EXCL, _section, _pagesize, bytes, offset, _flag));
    _reservation = _newsize;
    _length = (length - _offset < newsize) ? (length - _offset) : newsize;  // length of backing, not reservation
    return newsize;
#else                  // generic POSIX, inefficient
    byte *addrafter = _addr + _reservation;
    size_type bytes = newsize - _reservation;
    extent_type offset = _offset + _reservation;
    OUTCOME_TRY(addr, do_mmap(_v, addrafter, 0, _section, _pagesize, bytes, offset, _flag));
    if(addr != addrafter)
    {
      ::munmap(addr, bytes);
      return errc::not_enough_memory;
    }
    _reservation = _newsize;
    _length = (length - _offset < newsize) ? (length - _offset) : newsize;  // length of backing, not reservation
    return newsize;
#endif
  }
  // Shrink the map
  if(-1 == ::munmap(_addr + newsize, _reservation - newsize))
  {
    return posix_error();
  }
  _reservation = newsize;
  _length = (length - _offset < newsize) ? (length - _offset) : newsize;
  return newsize;
#endif
}

result<map_handle::buffer_type> map_handle::commit(buffer_type region, section_handle::flag flag) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
  // Set permissions on the pages
  region = utils::round_to_page_size(region, _pagesize);
  extent_type offset = _offset + (region.data() - _addr);
  size_type bytes = region.size();
  OUTCOME_TRYV(do_mmap(_v, region.data(), MAP_FIXED, _section, _pagesize, bytes, offset, flag));
  // Tell the kernel we will be using these pages soon
  if(-1 == ::madvise(region.data(), region.size(), MADV_WILLNEED))
  {
    return posix_error();
  }
  return region;
}

result<map_handle::buffer_type> map_handle::decommit(buffer_type region) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
  region = utils::round_to_page_size(region, _pagesize);
  // If decommitting a mapped file, tell the kernel to kick these pages back to storage
  if(_section != nullptr && -1 == ::madvise(region.data(), region.size(), MADV_DONTNEED))
  {
    return posix_error();
  }
  // Remap these pages with ones having no access, and remove from commit charge
  extent_type offset = _offset + (region.data() - _addr);
  size_type bytes = region.size();
  OUTCOME_TRYV(do_mmap(_v, region.data(), MAP_FIXED, _section, _pagesize, bytes, offset, section_handle::flag::none | section_handle::flag::nocommit));
  return region;
}

result<void> map_handle::zero_memory(buffer_type region) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
#ifdef MADV_FREE_REUSABLE
  /* Mac OS will reduce both the commit charge and MADV_FREE the pages
  if we use this flag.
  */
  memset(region.data(), 0, region.size());
  buffer_type page_region{utils::round_up_to_page_size(region.data(), _pagesize), utils::round_down_to_page_size(region.size(), _pagesize)};
  // Zero contents
  if((page_region.size() != 0u) && _section == nullptr)
  {
    (void) ::madvise(page_region.data(), page_region.size(), MADV_FREE_REUSABLE);
  }
  return success();
#endif
#ifdef MADV_REMOVE
  buffer_type page_region{utils::round_up_to_page_size(region.data(), _pagesize), utils::round_down_to_page_size(region.size(), _pagesize)};
  // Zero contents and punch a hole in any backing storage
  if((page_region.size() != 0u) && -1 != ::madvise(page_region.data(), page_region.size(), MADV_REMOVE))
  {
    memset(region.data(), 0, page_region.data() - region.data());
    memset(page_region.data() + page_region.size(), 0, (region.data() + region.size()) - (page_region.data() + page_region.size()));
    return success();
  }
#endif
  //! Only Linux implements syscall zero(), and it's covered by MADV_REMOVE already
  memset(region.data(), 0, region.size());
  return success();
}

result<span<map_handle::buffer_type>> map_handle::prefetch(span<buffer_type> regions) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(0);
  for(auto &region : regions)
  {
    if(-1 == ::madvise(region.data(), region.size(), MADV_WILLNEED))
    {
      return posix_error();
    }
  }
  return regions;
}

result<map_handle::buffer_type> map_handle::do_not_store(buffer_type region) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(0);
  region = utils::round_to_page_size(region, _pagesize);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
#ifdef MADV_FREE
  // Lightweight unset of dirty bit for these pages. Needs FreeBSD or very recent Linux.
  if(-1 != ::madvise(region.data(), region.size(), MADV_FREE))
    return region;
#endif
#ifdef MADV_REMOVE
  // This is rather heavy weight in that it also punches a hole in any backing storage
  // but it works on Linux for donkey's years
  if(-1 != ::madvise(region.data(), region.size(), MADV_REMOVE))
  {
    return region;
  }
#endif
  // No support on this platform
  region = {region.data(), 0};
  return region;
}

map_handle::io_result<map_handle::buffers_type> map_handle::_do_read(io_request<buffers_type> reqs, deadline /*d*/) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  byte *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? static_cast<size_type>(_length - reqs.offset) : 0;
  for(size_t i = 0; i < reqs.buffers.size(); i++)
  {
    buffer_type &req = reqs.buffers[i];
    req = {addr, req.size()};
    if(req.size() > togo)
    {
      req = {req.data(), togo};
      reqs.buffers = {reqs.buffers.data(), i + 1};
      break;
    }
    addr += req.size();
    togo -= req.size();
  }
  return reqs.buffers;
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::_do_write(io_request<const_buffers_type> reqs, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(!!(_flag & section_handle::flag::write_via_syscall) && _section != nullptr && _section->backing() != nullptr)
  {
    auto r = _section->backing()->write(reqs, d);
    if(!r)
    {
      return std::move(r).error();
    }
    if(reqs.offset + r.bytes_transferred() > _length)
    {
      OUTCOME_TRY(update_map());
    }
    return std::move(r).value();
  }
  byte *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? static_cast<size_type>(_length - reqs.offset) : 0;
  if(QUICKCPPLIB_NAMESPACE::signal_guard::signal_guard(
     QUICKCPPLIB_NAMESPACE::signal_guard::signalc_set::undefined_memory_access,
     [&] {
       for(size_t i = 0; i < reqs.buffers.size(); i++)
       {
         const_buffer_type &req = reqs.buffers[i];
         if(req.size() > togo)
         {
           memcpy(addr, req.data(), togo);
           req = {addr, togo};
           reqs.buffers = {reqs.buffers.data(), i + 1};
           return false;
         }
         else
         {
           memcpy(addr, req.data(), req.size());
           req = {addr, req.size()};
           addr += req.size();
           togo -= req.size();
         }
       }
       return false;
     },
     [&](const QUICKCPPLIB_NAMESPACE::signal_guard::raised_signal_info *info) {
       auto *causingaddr = (byte *) info->addr;
       if(causingaddr < _addr || causingaddr >= (_addr + _reservation))
       {
         // Not caused by this map
         thrd_raise_signal(info->signo, info->raw_info, info->raw_context);
         abort();
       }
       return true;
     }))
  {
    return errc::no_space_on_device;
  }
  return reqs.buffers;
}

LLFIO_V2_NAMESPACE_END
