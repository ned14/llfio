/* A handle to a source of mapped memory
(C) 2016-2022 Niall Douglas <http://www.nedproductions.biz/> (17 commits)
File Created: August 2016


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
#include "import.hpp"

#include "quickcpplib/algorithm/hash.hpp"

#ifndef LLFIO_DISABLE_SIGNAL_GUARD
#include "quickcpplib/signal_guard.hpp"
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
#ifndef NDEBUG
    // Tell handle::close() that we have correctly executed
    _v.behaviour |= native_handle_type::disposition::_child_close_executed;
#endif
    OUTCOME_TRYV(handle::close());
    OUTCOME_TRYV(_anonymous.close());
    _flag = flag::none;
  }
  return success();
}

result<section_handle> section_handle::section(file_handle &backing, extent_type maximum_size, flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<section_handle> ret(section_handle(native_handle_type(), &backing, file_handle(), _flag));
  native_handle_type &nativeh = ret.value()._v;
  ULONG prot = 0, attribs = 0;
  if(_flag & flag::read)
  {
    nativeh.behaviour |= native_handle_type::disposition::readable;
  }
  if(_flag & flag::write)
  {
    nativeh.behaviour |= native_handle_type::disposition::writable;
  }
  if(_flag & flag::execute)
  {
  }
  if(!!(_flag & flag::cow) && !!(_flag & flag::execute))
  {
    prot = PAGE_EXECUTE_WRITECOPY;
  }
  else if(_flag & flag::execute)
  {
    prot = PAGE_EXECUTE;
  }
  else if(_flag & flag::cow)
  {
    prot = PAGE_WRITECOPY;
  }
  else if(_flag & flag::write)
  {
    prot = PAGE_READWRITE;
  }
  else
  {
    prot = PAGE_READONLY;  // PAGE_NOACCESS is refused, so this is the next best
  }
  attribs = SEC_COMMIT;
  if(_flag & flag::executable)
  {
    attribs = SEC_IMAGE;
  }
  if(_flag & flag::prefault)
  {
    // Handled during view mapping below
  }
  if(!(_flag & section_handle::flag::nvram))
  {
    // Windows supports large pages, and no larger
    if((_flag & section_handle::flag::page_sizes_3) == section_handle::flag::page_sizes_3)
    {
      return errc::invalid_argument;
    }
    else if((_flag & section_handle::flag::page_sizes_2) == section_handle::flag::page_sizes_2)
    {
      return errc::invalid_argument;
    }
    else if((_flag & section_handle::flag::page_sizes_1) == section_handle::flag::page_sizes_1)
    {
      attribs |= SEC_LARGE_PAGES;
    }
  }
  nativeh.behaviour |= native_handle_type::disposition::section | native_handle_type::disposition::kernel_handle;
  OBJECT_ATTRIBUTES oa{}, *poa = nullptr;
  UNICODE_STRING _path{};
  wchar_t *singleton_rwchar = nullptr;
  if(_flag & flag::singleton)
  {
    static thread_local const std::pair<wchar_t *, wchar_t *> buffer = []() -> std::pair<wchar_t *, wchar_t *>
    {
      static thread_local wchar_t buffer_[96];
      struct
      {
        UNICODE_STRING str;
        wchar_t buffer[96];
      } name;
      HANDLE dir;  // Should not be freed
      if(BaseGetNamedObjectDirectory(&dir) == 0 && NtQueryObject(dir, ObjectNameInformation, &name, sizeof(name), nullptr) == 0)
      {
        unsigned len = name.str.Length / sizeof(wchar_t);
        if(len + 7 + 34 >= std::size(buffer_))
        {
          abort();
        }
        memcpy(buffer_, name.str.Buffer, name.str.Length + sizeof(wchar_t));
        wcscpy_s(buffer_ + len, 8, L"\\llfio_");
        return {buffer_, buffer_ + len + 7};
      }
      DWORD sessionid = 0;
      if(ProcessIdToSessionId(GetCurrentProcessId(), &sessionid) == 0)
      {
        sessionid = 0;
      }
      wsprintfW(buffer_, L"\\Sessions\\%u\\BaseNamedObjects\\llfio_", sessionid);
      if(std::size(buffer_) - wcslen(buffer_) < 34)
      {
        abort();
      }
      auto *end = wcschr(buffer_, 0);
      return {buffer_, end};
    }();
    auto unique_id = backing.unique_id();
    singleton_rwchar = buffer.second;
    buffer.second[0] = 'w';
    QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(buffer.second + 1, sizeof(unique_id) * 4, reinterpret_cast<char *>(unique_id.as_bytes),
                                                            sizeof(unique_id));
    buffer.second[33] = 0;
#if 0
    {
      void *bt[32];
      auto btlen = backtrace(bt, 32);
      auto **strings = backtrace_symbols(bt, btlen);
      std::string disp;
      for(size_t n = 0; n < btlen; n++)
      {
        disp.append(strings[n]);
        disp.push_back(';');
      }
      free(strings);
      std::wcout << "For file with unique id " << std::hex << unique_id.as_longlongs[0] << unique_id.as_longlongs[1] << std::dec
                 << " I name the singleton section " << buffer.second << ". Path is " << backing.current_path().value()
                 << ". writable = " << !!(_flag & flag::write) << std::flush;
      std::cout << ". Backtrace: " << disp << std::endl;
    }
#endif
    _path.Buffer = buffer.first;
    _path.MaximumLength = (_path.Length = static_cast<USHORT>((33 + buffer.second - buffer.first) * sizeof(wchar_t))) + sizeof(wchar_t);
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.Attributes = 0x80 /*OBJ_OPENIF*/;
    poa = &oa;
  }
  LARGE_INTEGER _maximum_size{}, *pmaximum_size = &_maximum_size;
  if(maximum_size > 0)
  {
    _maximum_size.QuadPart = maximum_size;
  }
  else
  {
    pmaximum_size = nullptr;
  }
  LLFIO_LOG_FUNCTION_CALL(&ret);
  HANDLE h;
  // Try to open any existing object first. With luck, it'll have write privs allowing address space reservation.
  NTSTATUS ntstat = NtOpenSection(&h, SECTION_ALL_ACCESS, poa);
  if(ntstat < 0)
  {
    if(!(_flag & flag::write) && singleton_rwchar != nullptr)
    {
      // We always preferentially open the writable section if it exists so resizes of it by any one process automatically
      // update all other processes, which is much more efficient when multiple threads are reading and writing the same
      // file and some have write privs and some have read privs. If however we are in a situation where LLFIO processes
      // only ever read, try the read section too, and only create a read section if we have read only privs.
      *singleton_rwchar = 'r';
      ntstat = NtOpenSection(&h, SECTION_ALL_ACCESS, poa);
      if(ntstat >= 0)
      {
        goto successful;
      }
    }
    ntstat = NtCreateSection(&h, SECTION_ALL_ACCESS, poa, pmaximum_size, prot, attribs, backing.native_handle().h);
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
  }
successful:
  nativeh.h = h;
  return ret;
}

result<section_handle> section_handle::section(extent_type bytes, const path_handle &dirh, flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  OUTCOME_TRY(auto &&_anonh, file_handle::temp_inode(dirh));
  OUTCOME_TRYV(_anonh.truncate(bytes));
  result<section_handle> ret(section_handle(native_handle_type(), nullptr, std::move(_anonh), _flag));
  native_handle_type &nativeh = ret.value()._v;
  file_handle &anonh = ret.value()._anonymous;
  ULONG prot = 0, attribs = 0;
  if(_flag & flag::read)
  {
    nativeh.behaviour |= native_handle_type::disposition::readable;
  }
  if(_flag & flag::write)
  {
    nativeh.behaviour |= native_handle_type::disposition::writable;
  }
  if(_flag & flag::execute)
  {
  }
  if(!!(_flag & flag::cow) && !!(_flag & flag::execute))
  {
    prot = PAGE_EXECUTE_WRITECOPY;
  }
  else if(_flag & flag::execute)
  {
    prot = PAGE_EXECUTE;
  }
  else if(_flag & flag::cow)
  {
    prot = PAGE_WRITECOPY;
  }
  else if(_flag & flag::write)
  {
    prot = PAGE_READWRITE;
  }
  else
  {
    prot = PAGE_READONLY;  // PAGE_NOACCESS is refused, so this is the next best
  }
  attribs = SEC_COMMIT;
  // On Windows, asking for inaccessible memory from the swap file almost certainly
  // means the user is intending to change permissions later, so reserve read/write
  // memory of the size requested
  if(!_flag)
  {
    attribs = SEC_RESERVE;
    prot = PAGE_READWRITE;
  }
  if(_flag & flag::executable)
  {
    attribs = SEC_IMAGE;
  }
  if(_flag & flag::prefault)
  {
    // Handled during view mapping below
  }
  if(!(_flag & section_handle::flag::nvram))
  {
    // Windows supports large pages, and no larger
    if((_flag & section_handle::flag::page_sizes_3) == section_handle::flag::page_sizes_3)
    {
      return errc::invalid_argument;
    }
    else if((_flag & section_handle::flag::page_sizes_2) == section_handle::flag::page_sizes_2)
    {
      return errc::invalid_argument;
    }
    else if((_flag & section_handle::flag::page_sizes_1) == section_handle::flag::page_sizes_1)
    {
      attribs |= SEC_LARGE_PAGES;
    }
  }
  nativeh.behaviour |= native_handle_type::disposition::section | native_handle_type::disposition::kernel_handle;
  LARGE_INTEGER _maximum_size{}, *pmaximum_size = &_maximum_size;
  _maximum_size.QuadPart = bytes;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  HANDLE h;
  NTSTATUS ntstat = NtCreateSection(&h, SECTION_ALL_ACCESS, nullptr, pmaximum_size, prot, attribs, anonh.native_handle().h);
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  nativeh.h = h;
  return ret;
}

result<section_handle::extent_type> section_handle::length() const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  SECTION_BASIC_INFORMATION sbi{};
  NTSTATUS ntstat = NtQuerySection(_v.h, SectionBasicInformation, &sbi, sizeof(sbi), nullptr);
  if(STATUS_SUCCESS != ntstat)
  {
    return ntkernel_error(ntstat);
  }
  return sbi.MaximumSize.QuadPart;
}

result<section_handle::extent_type> section_handle::truncate(extent_type newsize) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  if(newsize == 0u)
  {
    if(_backing != nullptr)
    {
      OUTCOME_TRY(auto &&length, _backing->maximum_extent());
      newsize = length;
    }
    else
    {
      return errc::invalid_argument;
    }
  }
  LARGE_INTEGER _maximum_size{};
  _maximum_size.QuadPart = newsize;
  NTSTATUS ntstat = NtExtendSection(_v.h, &_maximum_size);
  if(STATUS_SUCCESS != ntstat)
  {
    return ntkernel_error(ntstat);
  }
  return newsize;
}


/******************************************* map_handle *********************************************/

template <class T> static inline T win32_round_up_to_allocation_size(T i) noexcept
{
  // Should we fetch the allocation granularity from Windows? I very much doubt it'll ever change from 64Kb
  i = (T) ((LLFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i) + 65535) & ~(65535));  // NOLINT
  return i;
}
static inline result<void> win32_map_flags(native_handle_type &nativeh, DWORD &allocation, DWORD &prot, size_t &commitsize, bool enable_reservation,
                                           section_handle::flag _flag)
{
  prot = PAGE_NOACCESS;
  if(enable_reservation && ((_flag & section_handle::flag::nocommit) || (_flag == section_handle::flag::none)))
  {
    allocation = MEM_RESERVE;
    prot = PAGE_NOACCESS;
    commitsize = 0;
  }
  if(_flag & section_handle::flag::cow)
  {
    prot = PAGE_WRITECOPY;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::write)
  {
    prot = (_flag & section_handle::flag::write_via_syscall) ? PAGE_READONLY : PAGE_READWRITE;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::read)
  {
    prot = PAGE_READONLY;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
  }
  if(!!(_flag & section_handle::flag::cow) && !!(_flag & section_handle::flag::execute))
  {
    prot = PAGE_EXECUTE_WRITECOPY;
  }
  else if(_flag & section_handle::flag::execute)
  {
    prot = PAGE_EXECUTE;
  }
  if(!(_flag & section_handle::flag::nvram))
  {
    // Windows supports large pages, and no larger
    if((_flag & section_handle::flag::page_sizes_3) == section_handle::flag::page_sizes_3)
    {
      return errc::invalid_argument;
    }
    else if((_flag & section_handle::flag::page_sizes_2) == section_handle::flag::page_sizes_2)
    {
      return errc::invalid_argument;
    }
    else if((_flag & section_handle::flag::page_sizes_1) == section_handle::flag::page_sizes_1)
    {
      // Windows does not permit address reservation with large pages
      if(_flag & section_handle::flag::nocommit)
      {
        return errc::invalid_argument;
      }
      // Windows seems to require MEM_RESERVE with large pages
      allocation |= MEM_RESERVE | MEM_LARGE_PAGES;
    }
  }
  return success();
}

QUICKCPPLIB_BITFIELD_BEGIN(win32_map_sought){
committed = 1U << 0U,  // Call F for committed pages
freed = 1U << 1U,      // Call F for free pages
reserved = 1U << 2U    // Call F for reserved pages
} QUICKCPPLIB_BITFIELD_END(win32_map_sought)
/* Used to apply an operation to all maps within a region matching certain
region state flags.

Ok, so we have to be super careful here, indeed this routine has been
completely rewritten three times now. Do NOT change it unless you are aware
of the following:

Version 1:
----------
We would call VirtualQuery() to find a matching region, perform
F on it, and then call VirtualQuery() on the address where the matching region
ended. Unfortunately, there is a big performance problem with mapped files
within large reservations: if you call VirtualQuery() on the front of the
reservation, you get back the current file length mapped; if you then call VirtualQuery()
on just after the end of the file i.e. the front of the remaining reservation,
VirtualQuery() performs a linear scan until the end of the reserved region is
found. If the reservation was say 2^42, that takes a LONG time. Users rightly
complained about crap performance.

Version 2:
----------
We would call VirtualQuery() to find a matching region, perform F on it, then query
the *same* block again to get the next point of change, query that, perhaps perform F on it,
and so on. This introduced twice the number of VirtualQuery() calls as ought to be necessary,
but because during file map unmap it nuked the entire reservation plus file map,
it worked very quickly. This version worked well for nearly two years before the now-obvious race
condition was discovered: if the region was freed, and another thread performs
an allocation whose address just happens to land within the region just freed, the
second VirtualQuery() will now free another thread's allocation!

Version 3:
----------
VirtualQuery() is clearly not fit for purpose for unmapping file views, and we don't have Win10's new
QueryVirtualMemoryInformation() to hand because we support older Windows. So, for unmapping file views,
we as always fall back onto the native NT API, using the genuinely completely
undocumented MemoryRegionInformation. This doesn't even appear in ReactOS, our
usual source, I had to go to ProcessHacker to figure this one out. This
appears to do the first half of VirtualQuery(), specifically just the VMA query. It
doesn't scan within the VMA for pages with equal flags. This ought to make it
fast, plus return the whole reservation's size from the beginning, rather than
just the portion of the reservation containing the mapped file. This ought to
be exactly what we need, plus it works on older Windows.

We retain however win32_maps_apply for operations requiring committed pages only.
Here is a quick map:

- Flush view of a file: only on mapped file content, not reservation.
- Committing pages with no access privileges, where we use VirtualProtect to
remove access as VirtualAlloc won't commit no access pages.
- Discarding contents of pages.

For decommitting or releasing pages, we use win32_release_nonfile_allocations().
It will fail if called on a mapped file, as per Windows semantics.

For releasing mapped files, we use win32_release_file_allocations().
*/
template <class F>
static inline result<void> win32_maps_apply(byte *addr, size_t bytes, win32_map_sought sought, F &&f)
{
  MEMORY_BASIC_INFORMATION mbi;
  while(bytes > 0)
  {
    if(!VirtualQuery(addr, &mbi, sizeof(mbi)))
    {
      LLFIO_LOG_FATAL(nullptr, "map_handle::win32_maps_apply VirtualQuery() failed");
      abort();
    }
    bool doit = false;
    doit = doit || ((MEM_COMMIT == mbi.State) && (sought & win32_map_sought::committed));
    doit = doit || ((MEM_FREE == mbi.State) && (sought & win32_map_sought::freed));
    doit = doit || ((MEM_RESERVE == mbi.State) && (sought & win32_map_sought::reserved));
    if(doit)
    {
      /* mbi.RegionSize will be that from mbi.BaseAddress to end of a reserved region,
      so clamp to region size originally requested.
      */
      OUTCOME_TRY(f(reinterpret_cast<byte *>(mbi.BaseAddress), std::min((size_t) mbi.RegionSize, bytes)));
    }
    addr += mbi.RegionSize;
    if(mbi.RegionSize < bytes)
    {
      bytes -= mbi.RegionSize;
    }
    else
    {
      bytes = 0;
    }
  }
  return success();
}
// Only for memory allocated with NtMapViewOfSection.
static inline result<void> win32_release_file_allocations(byte *addr, size_t bytes)
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  MEMORY_REGION_INFORMATION mri;
  while(bytes > 0)
  {
    SIZE_T written = 0;
    NTSTATUS ntstat = NtQueryVirtualMemory(GetCurrentProcess(), addr, MemoryRegionInformation, &mri, sizeof(mri), &written);
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
    assert(addr == mri.AllocationBase);
    ntstat = NtUnmapViewOfSection(GetCurrentProcess(), mri.AllocationBase);
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
    addr += mri.RegionSize;
    if(mri.RegionSize < bytes)
    {
      bytes -= mri.RegionSize;
    }
    else
    {
      bytes = 0;
    }
  }
  return success();
}
// Only for memory allocated with VirtualAlloc. We can special case decommitting or releasing
// memory because NtFreeVirtualMemory() tells us how much it freed.
static inline result<void> win32_release_nonfile_allocations(byte *addr, size_t bytes, ULONG op)
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  while(bytes > 0)
  {
    VOID *regionbase = addr;
    SIZE_T regionsize = (op == MEM_RELEASE) ? 0 : bytes;
    NTSTATUS ntstat = NtFreeVirtualMemory(GetCurrentProcess(), &regionbase, &regionsize, op);
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
    addr += regionsize;
    assert(regionsize <= bytes);
    bytes -= regionsize;
  }
  return success();
}

map_handle::~map_handle()
{
  if(_addr != nullptr)
  {
    // Unmap the view
    auto ret = map_handle::close();
    if(ret.has_error())
    {
      LLFIO_LOG_FATAL(_v.h, "map_handle::~map_handle() close failed");
      abort();
    }
  }
}

result<void> map_handle::close() noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_addr != nullptr)
  {
    if(_section != nullptr)
    {
      if(is_writable() && (_flag & section_handle::flag::barrier_on_close))
      {
        OUTCOME_TRYV(barrier(barrier_kind::wait_all));
      }
      OUTCOME_TRY(win32_release_file_allocations(_addr, _reservation));
    }
    else
    {
      if(!_recycle_map())
      {
        OUTCOME_TRYV(win32_release_nonfile_allocations(_addr, _reservation, MEM_RELEASE));
      }
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

map_handle::io_result<map_handle::const_buffers_type> map_handle::_do_barrier(map_handle::io_request<map_handle::const_buffers_type> reqs, barrier_kind kind,
                                                                              deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  byte *addr = _addr + reqs.offset;
  extent_type bytes = 0;
  // Check for overflow
  for(const auto &req : reqs.buffers)
  {
    if(bytes + req.size() < bytes)
    {
      return errc::value_too_large;
    }
    bytes += req.size();
  }
  // bytes = 0 means flush entire mapping
  if(bytes == 0)
  {
    bytes = _reservation - reqs.offset;
  }
  // If nvram and not syncing metadata, use lightweight barrier
  if(kind <= barrier_kind::wait_data_only && is_nvram())
  {
    auto synced = nvram_barrier({addr, (size_type) bytes});
    if(synced.size() >= bytes)
    {
      return {reqs.buffers};
    }
  }
  OUTCOME_TRYV(win32_maps_apply(addr, (size_type) bytes, win32_map_sought::committed,
                                [](byte *addr, size_t bytes) -> result<void>
                                {
                                  if(FlushViewOfFile(addr, static_cast<SIZE_T>(bytes)) == 0)
                                  {
                                    return win32_error();
                                  }
                                  return success();
                                }));
  if((_section != nullptr) && (_section->backing() != nullptr) && kind >= barrier_kind::nowait_all)
  {
    reqs.offset += _offset;
    return _section->backing()->barrier(reqs, kind, d);
  }
  return {reqs.buffers};
}


result<map_handle> map_handle::_new_map(size_type bytes, bool fallback, section_handle::flag _flag) noexcept
{
  if(bytes == 0u)
  {
    return errc::argument_out_of_domain;
  }
  result<map_handle> ret(map_handle(nullptr, _flag));
  native_handle_type &nativeh = ret.value()._v;
  DWORD allocation = MEM_RESERVE | MEM_COMMIT, prot;
  PVOID addr = nullptr;
  OUTCOME_TRY(auto &&pagesize, detail::pagesize_from_flags(ret.value()._flag));
  bytes = utils::round_up_to_page_size(bytes, pagesize);
  {
    size_t commitsize;
    OUTCOME_TRY(win32_map_flags(nativeh, allocation, prot, commitsize, true, ret.value()._flag));
  }
  LLFIO_LOG_FUNCTION_CALL(&ret);
  addr = VirtualAlloc(nullptr, bytes, allocation, prot);
  if(addr == nullptr)
  {
    auto err = win32_error();
    if(fallback && err == errc::not_enough_memory)
    {
      // Try the cache
      auto r = _recycled_map(bytes, _flag);
      if(r)
      {
        memset(r.assume_value().address(), 0, r.assume_value().length());
        return r;
      }
    }
    return err;
  }
  ret.value()._addr = static_cast<byte *>(addr);
  ret.value()._reservation = bytes;
  ret.value()._length = bytes;
  ret.value()._pagesize = pagesize;
  nativeh._init = -2;  // otherwise appears closed
  nativeh.behaviour |= native_handle_type::disposition::allocation;

  // Windows has no way of getting the kernel to prefault maps on creation, so ...
  if(_flag & section_handle::flag::prefault)
  {
    using namespace windows_nt_kernel;
    // Start an asynchronous prefetch, so it might fault the whole lot in at once
    buffer_type b{static_cast<byte *>(addr), bytes};
    (void) prefetch(span<buffer_type>(&b, 1));
    volatile auto *a = static_cast<volatile char *>(addr);
    for(size_t n = 0; n < bytes; n += pagesize)
    {
      a[n];
    }
  }
  return ret;
}

result<map_handle> map_handle::map(section_handle &section, size_type bytes, extent_type offset, section_handle::flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  OUTCOME_TRY(auto &&length, section.length());  // length of the backing file
  if(length <= offset)
  {
    length = 0;
  }
  else
  {
    length -= offset;
  }
  if(bytes == 0u)
  {
    bytes = (size_type) length;
  }
  else if(length > bytes)
  {
    length = bytes;
  }
  result<map_handle> ret{map_handle(&section, _flag)};
  native_handle_type &nativeh = ret.value()._v;
  ULONG allocation = 0, prot;
  PVOID addr = nullptr;
  size_t commitsize = bytes + (offset & 65535);
  LARGE_INTEGER _offset{};
  _offset.QuadPart = offset & ~65535;
  OUTCOME_TRY(auto &&pagesize, detail::pagesize_from_flags(ret.value()._flag));
  SIZE_T _bytes = bytes + (offset & 65535);
  OUTCOME_TRY(win32_map_flags(nativeh, allocation, prot, commitsize, section.backing() != nullptr, ret.value()._flag));
  LLFIO_LOG_FUNCTION_CALL(&ret);
  NTSTATUS ntstat = NtMapViewOfSection(section.native_handle().h, GetCurrentProcess(), &addr, 0, commitsize, &_offset, &_bytes, ViewUnmap, allocation, prot);
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  ret.value()._addr = static_cast<byte *>(addr);
  ret.value()._offset = offset;
  ret.value()._reservation = _bytes - (offset & 65535);
  ret.value()._length = (size_type) length;
  ret.value()._pagesize = pagesize;
  // Make my handle borrow the native handle of my backing storage
  ret.value()._v.h = section.backing_native_handle().h;
  nativeh.behaviour |= native_handle_type::disposition::allocation | native_handle_type::disposition::kernel_handle;

  // Windows has no way of getting the kernel to prefault maps on creation, so ...
  if(ret.value()._flag & section_handle::flag::prefault)
  {
    // Start an asynchronous prefetch, so it might fault the whole lot in at once
    buffer_type b{static_cast<byte *>(addr), _bytes};
    (void) prefetch(span<buffer_type>(&b, 1));
    volatile auto *a = static_cast<volatile char *>(addr);
    for(size_t n = 0; n < _bytes; n += pagesize)
    {
      a[n];
    }
  }
  return ret;
}

result<map_handle::size_type> map_handle::truncate(size_type newsize, bool /* unused */) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  newsize = utils::round_up_to_page_size(newsize, _pagesize);
  if(newsize == _reservation)
  {
    return success();
  }
  // Is this VirtualAlloc() allocated memory?
  if(_section == nullptr)
  {
    if(newsize == 0)
    {
      OUTCOME_TRYV(win32_release_nonfile_allocations(_addr, _reservation, MEM_RELEASE));
      _addr = nullptr;
      _reservation = 0;
      _length = 0;
      return success();
    }
    if(newsize < _reservation)
    {
      // VirtualAlloc doesn't let us shrink regions, only free the whole thing. So if he tries
      // to free any part of a region, it'll fail.
      OUTCOME_TRYV(win32_release_nonfile_allocations(_addr + newsize, _reservation - newsize, MEM_RELEASE));
      _reservation = _length = newsize;
      return _reservation;
    }
    // Try to allocate another region directly after this one
    native_handle_type nativeh;
    DWORD allocation = MEM_RESERVE | MEM_COMMIT, prot;
    size_t commitsize;
    OUTCOME_TRY(win32_map_flags(nativeh, allocation, prot, commitsize, true, _flag));
    if(!VirtualAlloc(_addr + _reservation, newsize - _reservation, allocation, prot))
    {
      return win32_error();
    }
    _reservation = _length = newsize;
    return _reservation;
  }

  // So this must be file backed memory. Totally different APIs for that :)
  OUTCOME_TRY(auto &&length, _section->length());  // length of the backing file
  if(length <= _offset)
  {
    length = 0;
  }
  else
  {
    length -= _offset;
  }
  if(length > _reservation)
  {
    length = _reservation;
  }
  if(newsize < _reservation)
  {
    // If newsize isn't exactly a previous extension, this will fail, same as for the VirtualAlloc case
    OUTCOME_TRY(win32_release_file_allocations(_addr + newsize, _reservation - newsize));
    _reservation = newsize;
    _length = (size_type) length;
    return _reservation;
  }
  // Try to map an additional part of the section directly after this map
  ULONG allocation = MEM_RESERVE, prot;
  PVOID addr = _addr + _reservation;
  size_t commitsize = newsize - _reservation;
  LARGE_INTEGER offset{};
  offset.QuadPart = (_offset + _reservation) & ~65535;
  SIZE_T _bytes = newsize - _reservation;
  native_handle_type nativeh;
  OUTCOME_TRY(win32_map_flags(nativeh, allocation, prot, commitsize, _section->backing() != nullptr, _flag));
  NTSTATUS ntstat = NtMapViewOfSection(_section->native_handle().h, GetCurrentProcess(), &addr, 0, commitsize, &offset, &_bytes, ViewUnmap, allocation, prot);
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  _reservation += _bytes;
  _length = (size_type) length;
  return _reservation;
}

result<map_handle::buffer_type> map_handle::commit(buffer_type region, section_handle::flag flag) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
  DWORD prot = 0;
  if(flag == section_handle::flag::none || flag == section_handle::flag::nocommit)
  {
    OUTCOME_TRYV(win32_maps_apply(region.data(), region.size(), win32_map_sought::committed,
                                  [](byte *addr, size_t bytes) -> result<void>
                                  {
                                    DWORD _ = 0;
                                    if(VirtualProtect(addr, bytes, PAGE_NOACCESS, &_) == 0)
                                    {
                                      return win32_error();
                                    }
                                    return success();
                                  }));
    return region;
  }
  if(flag & section_handle::flag::cow)
  {
    prot = PAGE_WRITECOPY;
  }
  else if(flag & section_handle::flag::write)
  {
    prot = PAGE_READWRITE;
  }
  else if(flag & section_handle::flag::read)
  {
    prot = PAGE_READONLY;
  }
  if(flag & section_handle::flag::execute)
  {
    prot = PAGE_EXECUTE;
  }
  region = utils::round_to_page_size_larger(region, _pagesize);
  if(VirtualAlloc(region.data(), region.size(), MEM_COMMIT, prot) == nullptr)
  {
    return win32_error();
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
  region = utils::round_to_page_size_larger(region, _pagesize);
  OUTCOME_TRYV(win32_release_nonfile_allocations(region.data(), region.size(), MEM_DECOMMIT));
  return region;
}

result<void> map_handle::zero_memory(buffer_type region) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
  // Alas, zero() will not work on mapped views on Windows :(, so memset to zero and call discard if available
  memset(region.data(), 0, region.size());
  if(region.size() >= utils::page_size())
  {
    region = utils::round_to_page_size_smaller(region, _pagesize);
    if(region.size() > 0)
    {
      OUTCOME_TRYV(win32_maps_apply(region.data(), region.size(), win32_map_sought::committed,
                                    [](byte *addr, size_t bytes) -> result<void>
                                    {
                                      if(DiscardVirtualMemory_ != nullptr)
                                      {
                                        if(DiscardVirtualMemory_(addr, bytes) == 0)
                                        {
                                          // It seems DiscardVirtualMemory() behaves like VirtualUnlock() sometimes.
                                          if(ERROR_NOT_LOCKED != GetLastError())
                                          {
                                            return win32_error();
                                          }
                                        }
                                      }
                                      else
                                      {
                                        // This will always fail, but has the side effect of discarding the pages anyway.
                                        if(VirtualUnlock(addr, bytes) == 0)
                                        {
                                          if(ERROR_NOT_LOCKED != GetLastError())
                                          {
                                            return win32_error();
                                          }
                                        }
                                      }
                                      return success();
                                    }));
    }
  }
  return success();
}

result<span<map_handle::buffer_type>> map_handle::prefetch(span<buffer_type> regions) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(0);
  if(PrefetchVirtualMemory_ == nullptr)
  {
    return span<map_handle::buffer_type>();
  }
  static_assert(sizeof(WIN32_MEMORY_RANGE_ENTRY) == sizeof(buffer_type), "WIN32_MEMORY_RANGE_ENTRY is not the same size as a buffer_type!");
  auto wmre = reinterpret_cast<PWIN32_MEMORY_RANGE_ENTRY>(regions.data());
  if(PrefetchVirtualMemory_(GetCurrentProcess(), regions.size(), wmre, 0) == 0)
  {
    return win32_error();
  }
  return regions;
}

result<map_handle::buffer_type> map_handle::do_not_store(buffer_type region) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(0);
  region = utils::round_to_page_size_larger(region, _pagesize);
  if(region.data() == nullptr)
  {
    return errc::invalid_argument;
  }
  // Windows does not support throwing away dirty pages on file backed maps
  if((_section == nullptr) || (_section->backing() == nullptr))
  {
    // Win8's DiscardVirtualMemory is much faster if it's available
    if(DiscardVirtualMemory_ != nullptr)
    {
      OUTCOME_TRYV(win32_maps_apply(region.data(), region.size(), win32_map_sought::committed,
                                    [](byte *addr, size_t bytes) -> result<void>
                                    {
                                      if(DiscardVirtualMemory_(addr, bytes) == 0)
                                      {
                                        if(ERROR_NOT_LOCKED != GetLastError())
                                        {
                                          return win32_error();
                                        }
                                      }
                                      return success();
                                    }));
      return region;
    }
    // Else VirtualUnlock will do
    OUTCOME_TRYV(win32_maps_apply(region.data(), region.size(), win32_map_sought::committed,
                                  [](byte *addr, size_t bytes) -> result<void>
                                  {
                                    // This will always fail, but has the side effect of discarding the pages anyway.
                                    if(VirtualUnlock(addr, bytes) == 0)
                                    {
                                      if(ERROR_NOT_LOCKED != GetLastError())
                                      {
                                        return win32_error();
                                      }
                                    }
                                    return success();
                                  }));
    return region;
  }
  // We did nothing
  region = {region.data(), 0};
  return region;
}

map_handle::io_result<map_handle::buffers_type> map_handle::_do_read(io_request<buffers_type> reqs, deadline /*d*/) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  byte *addr = _addr + reqs.offset + (_offset & 65535);
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
    reqs.offset += _offset;
    auto r = _section->backing()->write(reqs, d);
    if(!r)
    {
      return std::move(r).error();
    }
    reqs.offset -= _offset;
    if(reqs.offset + r.bytes_transferred() > _length)
    {
      OUTCOME_TRY(update_map());
    }
    return std::move(r).value();
  }
  byte *addr = _addr + reqs.offset + (_offset & 65535);
  size_type togo = reqs.offset < _length ? static_cast<size_type>(_length - reqs.offset) : 0;
#ifndef LLFIO_DISABLE_SIGNAL_GUARD
  if(QUICKCPPLIB_NAMESPACE::signal_guard::signal_guard(
     QUICKCPPLIB_NAMESPACE::signal_guard::signalc_set::undefined_memory_access | QUICKCPPLIB_NAMESPACE::signal_guard::signalc_set::segmentation_fault,
#endif
     [&]
     {
    for(size_t i = 0; i < reqs.buffers.size(); i++)
    {
      const_buffer_type &req = reqs.buffers[i];
      if(req.size() > togo)
      {
        assert(req.data() != nullptr);
        memcpy(addr, req.data(), togo);
        req = {addr, togo};
        reqs.buffers = {reqs.buffers.data(), i + 1};
        return false;
      }
      else
      {
        assert(req.data() != nullptr);
        memcpy(addr, req.data(), req.size());
        req = {addr, req.size()};
        addr += req.size();
        togo -= req.size();
      }
    }
    return false;
#ifdef LLFIO_DISABLE_SIGNAL_GUARD
       }();
#else
     },
     [&](const QUICKCPPLIB_NAMESPACE::signal_guard::raised_signal_info *info)
     {
    auto *causingaddr = (byte *) info->addr;
    if(causingaddr < _addr || causingaddr >= (_addr + _length))
    {
      // Not caused by this map
      thrd_raise_signal(info->signo, info->raw_info, info->raw_context);
    }
    return true;
     }))
     {
       return errc::no_space_on_device;
     }
#endif
  return reqs.buffers;
}

LLFIO_V2_NAMESPACE_END
