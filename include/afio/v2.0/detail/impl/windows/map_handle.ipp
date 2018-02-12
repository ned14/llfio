/* A handle to a source of mapped memory
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (17 commits)
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

#include "../../../quickcpplib/include/algorithm/hash.hpp"


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
  nativeh.behaviour |= native_handle_type::disposition::section;
  OBJECT_ATTRIBUTES oa{}, *poa = nullptr;
  UNICODE_STRING _path{};
  static wchar_t *buffer = []() -> wchar_t * {
    static wchar_t buffer_[96] = L"\\Sessions\\0\\BaseNamedObjects\\";
    DWORD sessionid = 0;
    if(ProcessIdToSessionId(GetCurrentProcessId(), &sessionid) != 0)
    {
      wsprintf(buffer_, L"\\Sessions\\%u\\BaseNamedObjects\\", sessionid);
    }
    return buffer_;
  }();
  static wchar_t *bufferid = wcschr(buffer, 0);
  if(_flag & flag::singleton)
  {
    OUTCOME_TRY(currentpath, backing.current_path());
    auto hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash(reinterpret_cast<const char *>(currentpath.native().data()), currentpath.native().size() * sizeof(wchar_t));
    auto *_buffer = reinterpret_cast<char *>(bufferid);
    QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(_buffer, 96 * sizeof(wchar_t), reinterpret_cast<const char *>(hash.as_bytes), sizeof(hash.as_bytes));
    for(size_t n = 31; n <= 31; n--)
    {
      bufferid[n] = _buffer[n];
    }
    bufferid[32] = 0;
    _path.Buffer = buffer;
    _path.MaximumLength = (_path.Length = static_cast<USHORT>((32 + bufferid - buffer) * sizeof(wchar_t))) + sizeof(wchar_t);
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
  AFIO_LOG_FUNCTION_CALL(&ret);
  HANDLE h;
  NTSTATUS ntstat = NtCreateSection(&h, SECTION_ALL_ACCESS, poa, pmaximum_size, prot, attribs, backing.native_handle().h);
  if(ntstat < 0)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  nativeh.h = h;
  return ret;
}

result<section_handle> section_handle::section(extent_type bytes, const path_handle &dirh, flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  OUTCOME_TRY(_anonh, file_handle::temp_inode(dirh));
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
  nativeh.behaviour |= native_handle_type::disposition::section;
  LARGE_INTEGER _maximum_size{}, *pmaximum_size = &_maximum_size;
  _maximum_size.QuadPart = bytes;
  AFIO_LOG_FUNCTION_CALL(&ret);
  HANDLE h;
  NTSTATUS ntstat = NtCreateSection(&h, SECTION_ALL_ACCESS, nullptr, pmaximum_size, prot, attribs, anonh.native_handle().h);
  if(ntstat < 0)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  nativeh.h = h;
  return ret;
}

result<section_handle::extent_type> section_handle::length() const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  SECTION_BASIC_INFORMATION sbi{};
  NTSTATUS ntstat = NtQuerySection(_v.h, SectionBasicInformation, &sbi, sizeof(sbi), nullptr);
  if(STATUS_SUCCESS != ntstat)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  return sbi.MaximumSize.QuadPart;
}

result<section_handle::extent_type> section_handle::truncate(extent_type newsize) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(newsize == 0u)
  {
    if(_backing != nullptr)
    {
      OUTCOME_TRY(length, _backing->length());
      newsize = length;
    }
    else
    {
      return std::errc::invalid_argument;
    }
  }
  LARGE_INTEGER _maximum_size{};
  _maximum_size.QuadPart = newsize;
  NTSTATUS ntstat = NtExtendSection(_v.h, &_maximum_size);
  if(STATUS_SUCCESS != ntstat)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  return newsize;
}


/******************************************* map_handle *********************************************/

template <class T> static inline T win32_round_up_to_allocation_size(T i) noexcept
{
  // Should we fetch the allocation granularity from Windows? I very much doubt it'll ever change from 64Kb
  i = (T)((AFIO_V2_NAMESPACE::detail::unsigned_integer_cast<uintptr_t>(i) + 65535) & ~(65535));  // NOLINT
  return i;
}
static inline void win32_map_flags(native_handle_type &nativeh, DWORD &allocation, DWORD &prot, size_t &commitsize, bool enable_reservation, section_handle::flag _flag)
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
    prot = PAGE_READWRITE;
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
}
// Used to apply an operation to all maps within a region
template <class F> static inline result<void> win32_maps_apply(char *addr, size_t bytes, F &&f)
{
  while(bytes > 0)
  {
    MEMORY_BASIC_INFORMATION mbi;
    char *thisregion = addr;
    // Need to iterate until AllocationBase changes
    for(;;)
    {
      if(!VirtualQuery(addr, &mbi, sizeof(mbi)) || mbi.AllocationBase != thisregion)
      {
        break;
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
    // Address passed in originally must match an allocation
    if(addr == thisregion)
    {
      return std::errc::invalid_argument;
    }
    OUTCOME_TRYV(f(thisregion, addr - thisregion));
  }
  return success();
}
// Only for memory allocated with VirtualAlloc. We can special case decommitting or releasing
// memory because NtFreeVirtualMemory() tells us how much it freed.
static inline result<void> win32_release_allocations(char *addr, size_t bytes, ULONG op)
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
      return {ntstat, ntkernel_category()};
    }
    addr += regionsize;
    assert(regionsize <= bytes);
    bytes -= regionsize;
  }
  return success();
}

map_handle::~map_handle()
{
  if(_v)
  {
    // Unmap the view
    auto ret = map_handle::close();
    if(ret.has_error())
    {
      AFIO_LOG_FATAL(_v.h, "map_handle::~map_handle() close failed");
      abort();
    }
  }
}

result<void> map_handle::close() noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(_addr != nullptr)
  {
    if(_section != nullptr)
    {
      if(is_writable() && (_flag & section_handle::flag::barrier_on_close))
      {
        OUTCOME_TRYV(barrier({}, true, false));
      }
      OUTCOME_TRYV(win32_maps_apply(_addr, _reservation, [](char *addr, size_t /* unused */) -> result<void> {
        NTSTATUS ntstat = NtUnmapViewOfSection(GetCurrentProcess(), addr);
        if(ntstat < 0)
        {
          return {ntstat, ntkernel_category()};
        }
        return success();
      }));
    }
    else
    {
      OUTCOME_TRYV(win32_release_allocations(_addr, _reservation, MEM_RELEASE));
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
  AFIO_LOG_FUNCTION_CALL(this);
  // We don't want ~handle() to close our borrowed handle
  _v = native_handle_type();
  _addr = nullptr;
  _length = 0;
  return {};
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::barrier(map_handle::io_request<map_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  char *addr = _addr + reqs.offset;
  extent_type bytes = 0;
  for(const auto &req : reqs.buffers)
  {
    if(bytes + req.len < bytes)
    {
      return std::errc::value_too_large;
    }
    bytes += req.len;
  }
  // bytes = 0 means flush entire mapping
  if(bytes == 0)
  {
    bytes = _reservation - reqs.offset;
  }
  OUTCOME_TRYV(win32_maps_apply(addr, bytes, [](char *addr, size_t bytes) -> result<void> {
    if(FlushViewOfFile(addr, static_cast<SIZE_T>(bytes)) == 0)
    {
      return {GetLastError(), std::system_category()};
    }
    return success();
  }));
  if((_section != nullptr) && (_section->backing() != nullptr) && (wait_for_device || and_metadata))
  {
    reqs.offset += _offset;
    return _section->backing()->barrier(reqs, wait_for_device, and_metadata, d);
  }
  return {reqs.buffers};
}


result<map_handle> map_handle::map(size_type bytes, section_handle::flag _flag) noexcept
{
  bytes = win32_round_up_to_allocation_size(bytes);
  result<map_handle> ret(map_handle(nullptr));
  native_handle_type &nativeh = ret.value()._v;
  DWORD allocation = MEM_RESERVE | MEM_COMMIT, prot;
  PVOID addr = nullptr;
  {
    size_t commitsize;
    win32_map_flags(nativeh, allocation, prot, commitsize, true, _flag);
  }
  AFIO_LOG_FUNCTION_CALL(&ret);
  addr = VirtualAlloc(nullptr, bytes, allocation, prot);
  if(addr == nullptr)
  {
    return {GetLastError(), std::system_category()};
  }
  ret.value()._addr = static_cast<char *>(addr);
  ret.value()._reservation = bytes;
  ret.value()._length = bytes;

  // Windows has no way of getting the kernel to prefault maps on creation, so ...
  if(_flag & section_handle::flag::prefault)
  {
    using namespace windows_nt_kernel;
    // Start an asynchronous prefetch
    buffer_type b{static_cast<char *>(addr), bytes};
    (void) prefetch(span<buffer_type>(&b, 1));
    // If this kernel doesn't support that API, manually poke every page in the new map
    if(PrefetchVirtualMemory_ == nullptr)
    {
      size_t pagesize = utils::page_size();
      volatile auto *a = static_cast<volatile char *>(addr);
      for(size_t n = 0; n < bytes; n += pagesize)
      {
        a[n];
      }
    }
  }
  return ret;
}

result<map_handle> map_handle::map(section_handle &section, size_type bytes, extent_type offset, section_handle::flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<map_handle> ret{map_handle(&section)};
  native_handle_type &nativeh = ret.value()._v;
  ULONG allocation = MEM_RESERVE, prot;
  PVOID addr = nullptr;
  size_t commitsize = bytes;
  LARGE_INTEGER _offset{};
  _offset.QuadPart = offset;
  SIZE_T _bytes = win32_round_up_to_allocation_size(bytes);  // reserve to next 64Kb boundary
  win32_map_flags(nativeh, allocation, prot, commitsize, section.backing() != nullptr, _flag);
  AFIO_LOG_FUNCTION_CALL(&ret);
  NTSTATUS ntstat = NtMapViewOfSection(section.native_handle().h, GetCurrentProcess(), &addr, 0, commitsize, &_offset, &_bytes, ViewUnmap, allocation, prot);
  if(ntstat < 0)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  ret.value()._addr = static_cast<char *>(addr);
  ret.value()._offset = offset;
  ret.value()._reservation = _bytes;
  ret.value()._length = section.length().value() - offset;
  // Make my handle borrow the native handle of my backing storage
  ret.value()._v.h = section.backing_native_handle().h;

  // Windows has no way of getting the kernel to prefault maps on creation, so ...
  if(_flag & section_handle::flag::prefault)
  {
    // Start an asynchronous prefetch
    buffer_type b{static_cast<char *>(addr), _bytes};
    (void) prefetch(span<buffer_type>(&b, 1));
    // If this kernel doesn't support that API, manually poke every page in the new map
    if(PrefetchVirtualMemory_ == nullptr)
    {
      size_t pagesize = utils::page_size();
      volatile auto *a = static_cast<volatile char *>(addr);
      for(size_t n = 0; n < _bytes; n += pagesize)
      {
        a[n];
      }
    }
  }
  return ret;
}

result<map_handle::size_type> map_handle::truncate(size_type newsize, bool /* unused */) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  newsize = win32_round_up_to_allocation_size(newsize);
  if(newsize == _reservation)
  {
    return success();
  }
  // Is this VirtualAlloc() allocated memory?
  if(_section == nullptr)
  {
    if(newsize == 0)
    {
      OUTCOME_TRYV(win32_release_allocations(_addr, _reservation, MEM_RELEASE));
      _addr = nullptr;
      _reservation = 0;
      _length = 0;
      return success();
    }
    if(newsize < _reservation)
    {
      // VirtualAlloc doesn't let us shrink regions, only free the whole thing. So if he tries
      // to free any part of a region, it'll fail.
      OUTCOME_TRYV(win32_release_allocations(_addr + newsize, _reservation - newsize, MEM_RELEASE));
      _reservation = _length = newsize;
      return _reservation;
    }
    // Try to allocate another region directly after this one
    native_handle_type nativeh;
    DWORD allocation = MEM_RESERVE | MEM_COMMIT, prot;
    size_t commitsize;
    win32_map_flags(nativeh, allocation, prot, commitsize, true, _flag);
    if(!VirtualAlloc(_addr + _reservation, newsize - _reservation, allocation, prot))
    {
      return {GetLastError(), std::system_category()};
    }
    _reservation = _length = newsize;
    return _reservation;
  }

  // So this must be file backed memory. Totally different APIs for that :)
  OUTCOME_TRY(length, _section->length());  // length of the backing file
  if(newsize < _reservation)
  {
    // If newsize isn't exactly a previous extension, this will fail, same as for the VirtualAlloc case
    OUTCOME_TRYV(win32_maps_apply(_addr + newsize, _reservation - newsize, [](char *addr, size_t /* unused */) -> result<void> {
      NTSTATUS ntstat = NtUnmapViewOfSection(GetCurrentProcess(), addr);
      if(ntstat < 0)
      {
        return {ntstat, ntkernel_category()};
      }
      return success();
    }));
    _reservation = newsize;
    _length = (length - _offset < newsize) ? (length - _offset) : newsize;  // length of backing, not reservation
    return _reservation;
  }
  // Try to map an additional part of the section directly after this map
  ULONG allocation = MEM_RESERVE, prot;
  PVOID addr = _addr + _reservation;
  size_t commitsize = newsize - _reservation;
  LARGE_INTEGER offset{};
  offset.QuadPart = _offset + _reservation;
  SIZE_T _bytes = newsize - _reservation;
  native_handle_type nativeh;
  win32_map_flags(nativeh, allocation, prot, commitsize, _section->backing() != nullptr, _flag);
  NTSTATUS ntstat = NtMapViewOfSection(_section->native_handle().h, GetCurrentProcess(), &addr, 0, commitsize, &offset, &_bytes, ViewUnmap, allocation, prot);
  if(ntstat < 0)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  _reservation += _bytes;
  _length = (length - _offset < newsize) ? (length - _offset) : newsize;  // length of backing, not reservation
  return _reservation;
}

result<map_handle::buffer_type> map_handle::commit(buffer_type region, section_handle::flag flag) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(region.data == nullptr)
  {
    return std::errc::invalid_argument;
  }
  DWORD prot = 0;
  if(flag == section_handle::flag::none)
  {
    OUTCOME_TRYV(win32_maps_apply(region.data, region.len, [](char *addr, size_t bytes) -> result<void> {
      DWORD _ = 0;
      if(VirtualProtect(addr, bytes, PAGE_NOACCESS, &_) == 0)
      {
        return {GetLastError(), std::system_category()};
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
  region = utils::round_to_page_size(region);
  OUTCOME_TRYV(win32_maps_apply(region.data, region.len, [prot](char *addr, size_t bytes) -> result<void> {
    if(VirtualAlloc(addr, bytes, MEM_COMMIT, prot) == nullptr)
    {
      return {GetLastError(), std::system_category()};
    }
    return success();
  }));
  return region;
}

result<map_handle::buffer_type> map_handle::decommit(buffer_type region) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(region.data == nullptr)
  {
    return std::errc::invalid_argument;
  }
  region = utils::round_to_page_size(region);
  OUTCOME_TRYV(win32_maps_apply(region.data, region.len, [](char *addr, size_t bytes) -> result<void> {
    if(VirtualFree(addr, bytes, MEM_DECOMMIT) == 0)
    {
      return {GetLastError(), std::system_category()};
    }
    return success();
  }));
  return region;
}

result<void> map_handle::zero_memory(buffer_type region) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(region.data == nullptr)
  {
    return std::errc::invalid_argument;
  }
  // Alas, zero() will not work on mapped views on Windows :(, so memset to zero and call discard if available
  memset(region.data, 0, region.len);
  if((DiscardVirtualMemory_ != nullptr) && region.len >= utils::page_size())
  {
    region = utils::round_to_page_size(region);
    if(region.len > 0)
    {
      OUTCOME_TRYV(win32_maps_apply(region.data, region.len, [](char *addr, size_t bytes) -> result<void> {
        if(DiscardVirtualMemory_(addr, bytes) == 0)
        {
          return {GetLastError(), std::system_category()};
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
  AFIO_LOG_FUNCTION_CALL(0);
  if(PrefetchVirtualMemory_ == nullptr)
  {
    return span<map_handle::buffer_type>();
  }
  auto wmre = reinterpret_cast<PWIN32_MEMORY_RANGE_ENTRY>(regions.data());
  if(PrefetchVirtualMemory_(GetCurrentProcess(), regions.size(), wmre, 0) == 0)
  {
    return {GetLastError(), std::system_category()};
  }
  return regions;
}

result<map_handle::buffer_type> map_handle::do_not_store(buffer_type region) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(0);
  region = utils::round_to_page_size(region);
  if(region.data == nullptr)
  {
    return std::errc::invalid_argument;
  }
  // Windows does not support throwing away dirty pages on file backed maps
  if((_section == nullptr) || (_section->backing() == nullptr))
  {
    // Win8's DiscardVirtualMemory is much faster if it's available
    if(DiscardVirtualMemory_ != nullptr)
    {
      OUTCOME_TRYV(win32_maps_apply(region.data, region.len, [](char *addr, size_t bytes) -> result<void> {
        if(DiscardVirtualMemory_(addr, bytes) == 0)
        {
          return {GetLastError(), std::system_category()};
        }
        return success();
      }));
      return region;
    }
    // Else MEM_RESET will do
    OUTCOME_TRYV(win32_maps_apply(region.data, region.len, [](char *addr, size_t bytes) -> result<void> {
      if(VirtualAlloc(addr, bytes, MEM_RESET, 0) == nullptr)
      {
        return {GetLastError(), std::system_category()};
      }
      return success();
    }));
    return region;
  }
  // We did nothing
  region.len = 0;
  return region;
}

map_handle::io_result<map_handle::buffers_type> map_handle::read(io_request<buffers_type> reqs, deadline /*d*/) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  char *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? static_cast<size_type>(_length - reqs.offset) : 0;
  for(buffer_type &req : reqs.buffers)
  {
    if(togo != 0u)
    {
      req.data = addr;
      if(req.len > togo)
      {
        req.len = togo;
      }
      addr += req.len;
      togo -= req.len;
    }
    else
    {
      req.len = 0;
    }
  }
  return reqs.buffers;
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::write(io_request<const_buffers_type> reqs, deadline /*d*/) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  char *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? static_cast<size_type>(_length - reqs.offset) : 0;
  for(const_buffer_type &req : reqs.buffers)
  {
    if(togo != 0u)
    {
      if(req.len > togo)
      {
        req.len = togo;
      }
      memcpy(addr, req.data, req.len);
      req.data = addr;
      addr += req.len;
      togo -= req.len;
    }
    else
    {
      req.len = 0;
    }
  }
  return reqs.buffers;
}

AFIO_V2_NAMESPACE_END
