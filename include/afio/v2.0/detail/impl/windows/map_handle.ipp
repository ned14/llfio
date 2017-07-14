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

AFIO_V2_NAMESPACE_BEGIN

result<section_handle> section_handle::section(file_handle &backing, extent_type maximum_size, flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  if(!maximum_size)
  {
    if(backing.is_valid())
    {
      OUTCOME_TRY(length, backing.length());
      maximum_size = length;
    }
    else
      return std::errc::invalid_argument;
  }
  // Do NOT round up to page size here if backed by a file, it causes STATUS_SECTION_TOO_BIG
  if(!backing.is_valid())
    maximum_size = utils::round_up_to_page_size(maximum_size);
  result<section_handle> ret(section_handle(native_handle_type(), backing.is_valid() ? &backing : nullptr, maximum_size, _flag));
  native_handle_type &nativeh = ret.value()._v;
  ULONG prot = 0, attribs = 0;
  if(_flag & flag::read)
    nativeh.behaviour |= native_handle_type::disposition::readable;
  if(_flag & flag::write)
    nativeh.behaviour |= native_handle_type::disposition::writable;
  if(_flag & flag::execute)
  {
  }
  if(!!(_flag & flag::cow) && !!(_flag & flag::execute))
    prot = PAGE_EXECUTE_WRITECOPY;
  else if(_flag & flag::execute)
    prot = PAGE_EXECUTE;
  else if(_flag & flag::cow)
    prot = PAGE_WRITECOPY;
  else if(_flag & flag::write)
    prot = PAGE_READWRITE;
  else
    prot = PAGE_READONLY;  // PAGE_NOACCESS is refused, so this is the next best
  attribs = SEC_COMMIT;
  if(!backing.is_valid())
  {
    // On Windows, asking for inaccessible memory from the swap file almost certainly
    // means the user is intending to change permissions later, so reserve read/write
    // memory of the size requested
    if(!_flag)
    {
      attribs = SEC_RESERVE;
      prot = PAGE_READWRITE;
    }
  }
  else if(PAGE_READONLY == prot)
  {
    // In the case where there is a backing file, asking for read perms or no perms
    // means "don't auto-expand the file to the nearest 4Kb multiple"
    attribs = SEC_RESERVE;
  }
  if(_flag & flag::executable)
    attribs = SEC_IMAGE;
  if(_flag & flag::prefault)
  {
    // Handled during view mapping below
  }
  nativeh.behaviour |= native_handle_type::disposition::section;
  // OBJECT_ATTRIBUTES ObjectAttributes;
  // InitializeObjectAttributes(&ObjectAttributes, &NULL, 0, NULL, NULL);
  LARGE_INTEGER _maximum_size;
  _maximum_size.QuadPart = maximum_size;
  HANDLE h;
  NTSTATUS ntstat = NtCreateSection(&h, SECTION_ALL_ACCESS, NULL, &_maximum_size, prot, attribs, backing.is_valid() ? backing.native_handle().h : NULL);
  if(STATUS_SUCCESS != ntstat)
  {
    AFIO_LOG_FUNCTION_CALL(0);
    return make_errored_result_nt<section_handle>(ntstat);
  }
  nativeh.h = h;
  AFIO_LOG_FUNCTION_CALL(nativeh.h);
  return ret;
}

result<section_handle::extent_type> section_handle::truncate(extent_type newsize) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  newsize = utils::round_up_to_page_size(newsize);
  LARGE_INTEGER _maximum_size;
  _maximum_size.QuadPart = newsize;
  NTSTATUS ntstat = NtExtendSection(_v.h, &_maximum_size);
  if(STATUS_SUCCESS != ntstat)
    return make_errored_result_nt<extent_type>(ntstat);
  _length = newsize;
  return newsize;
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
  AFIO_LOG_FUNCTION_CALL(_addr);
  if(_addr)
  {
    if(_section)
    {
      NTSTATUS ntstat = NtUnmapViewOfSection(GetCurrentProcess(), _addr);
      if(STATUS_SUCCESS != ntstat)
        return make_errored_result_nt<void>(ntstat);
    }
    else
    {
      if(!VirtualFree(_addr, 0, MEM_RELEASE))
        return make_errored_result<void>(GetLastError());
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
  AFIO_LOG_FUNCTION_CALL(_v.h);
  // We don't want ~handle() to close our borrowed handle
  _v = native_handle_type();
  _addr = nullptr;
  _length = 0;
  return native_handle_type();
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::barrier(map_handle::io_request<map_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  char *addr = _addr + reqs.offset;
  extent_type bytes = 0;
  for(const auto &req : reqs.buffers)
    bytes += req.second;
  if(!FlushViewOfFile(addr, (SIZE_T) bytes))
    return {GetLastError(), std::system_category()};
  if(_section && _section->backing() && (wait_for_device || and_metadata))
  {
    reqs.offset += _offset;
    return _section->backing()->barrier(std::move(reqs), wait_for_device, and_metadata, d);
  }
  return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
}


result<map_handle> map_handle::map(size_type bytes, section_handle::flag _flag) noexcept
{
  bytes = utils::round_up_to_page_size(bytes);
  result<map_handle> ret(map_handle(nullptr));
  native_handle_type &nativeh = ret.value()._v;
  DWORD allocation = MEM_RESERVE | MEM_COMMIT, prot = 0;
  PVOID addr = 0;
  if((_flag & section_handle::flag::nocommit) || (_flag == section_handle::flag::none))
  {
    allocation = MEM_RESERVE;
    prot = PAGE_NOACCESS;
  }
  else if(_flag & section_handle::flag::cow)
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
    prot = PAGE_EXECUTE_WRITECOPY;
  else if(_flag & section_handle::flag::execute)
    prot = PAGE_EXECUTE;
  addr = VirtualAlloc(nullptr, bytes, allocation, prot);
  if(!addr)
    return {GetLastError(), std::system_category()};
  ret.value()._addr = (char *) addr;
  ret.value()._length = bytes;
  AFIO_LOG_FUNCTION_CALL(ret.value()._v.h);

  // Windows has no way of getting the kernel to prefault maps on creation, so ...
  if(_flag & section_handle::flag::prefault)
  {
    using namespace windows_nt_kernel;
    // Start an asynchronous prefetch
    buffer_type b((char *) addr, bytes);
    (void) prefetch(span<buffer_type>(&b, 1));
    // If this kernel doesn't support that API, manually poke every page in the new map
    if(!PrefetchVirtualMemory_)
    {
      size_t pagesize = utils::page_size();
      volatile char *a = (volatile char *) addr;
      for(size_t n = 0; n < bytes; n += pagesize)
        a[n];
    }
  }
  return ret;
}

result<map_handle> map_handle::map(section_handle &section, size_type bytes, extent_type offset, section_handle::flag _flag) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  if(!section.backing())
  {
    // Do NOT round up bytes to the nearest page size for backed maps, it causes an attempt to extend the file
    bytes = utils::round_up_to_page_size(bytes);
  }
  result<map_handle> ret{map_handle(&section)};
  native_handle_type &nativeh = ret.value()._v;
  ULONG allocation = 0, prot = 0;
  PVOID addr = 0;
  size_t commitsize = bytes;
  LARGE_INTEGER _offset;
  _offset.QuadPart = offset;
  SIZE_T _bytes = bytes;
  if((_flag & section_handle::flag::nocommit) || (_flag == section_handle::flag::none))
  {
    // Perhaps this is only valid from kernel mode? Either way, any attempt to use MEM_RESERVE caused an invalid parameter error.
    // Weirdly, setting commitsize to 0 seems to do a reserve as we wanted
    // allocation = MEM_RESERVE;
    commitsize = 0;
    prot = PAGE_NOACCESS;
  }
  else if(_flag & section_handle::flag::cow)
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
    prot = PAGE_EXECUTE_WRITECOPY;
  else if(_flag & section_handle::flag::execute)
    prot = PAGE_EXECUTE;
  NTSTATUS ntstat = NtMapViewOfSection(section.native_handle().h, GetCurrentProcess(), &addr, 0, commitsize, &_offset, &_bytes, ViewUnmap, allocation, prot);
  if(STATUS_SUCCESS != ntstat)
  {
    AFIO_LOG_FUNCTION_CALL(0);
    return make_errored_result_nt<map_handle>(ntstat);
  }
  ret.value()._addr = (char *) addr;
  ret.value()._offset = offset;
  ret.value()._length = _bytes;
  // Make my handle borrow the native handle of my backing storage
  ret.value()._v.h = section.backing_native_handle().h;
  AFIO_LOG_FUNCTION_CALL(ret.value()._v.h);

  // Windows has no way of getting the kernel to prefault maps on creation, so ...
  if(_flag & section_handle::flag::prefault)
  {
    // Start an asynchronous prefetch
    buffer_type b((char *) addr, _bytes);
    (void) prefetch(span<buffer_type>(&b, 1));
    // If this kernel doesn't support that API, manually poke every page in the new map
    if(!PrefetchVirtualMemory_)
    {
      size_t pagesize = utils::page_size();
      volatile char *a = (volatile char *) addr;
      for(size_t n = 0; n < _bytes; n += pagesize)
        a[n];
    }
  }
  return ret;
}

result<map_handle::buffer_type> map_handle::commit(buffer_type region, section_handle::flag _flag) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  if(!region.first)
    return std::errc::invalid_argument;
  DWORD prot = 0;
  if(_flag == section_handle::flag::none)
  {
    DWORD _ = 0;
    if(!VirtualProtect(region.first, region.second, PAGE_NOACCESS, &_))
      return make_errored_result<buffer_type>(GetLastError());
    return region;
  }
  if(_flag & section_handle::flag::cow)
  {
    prot = PAGE_WRITECOPY;
    _v.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::write)
  {
    prot = PAGE_READWRITE;
    _v.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
  }
  else if(_flag & section_handle::flag::read)
  {
    prot = PAGE_READONLY;
    _v.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
  }
  if(_flag & section_handle::flag::execute)
    prot = PAGE_EXECUTE;
  region = utils::round_to_page_size(region);
  if(!VirtualAlloc(region.first, region.second, MEM_COMMIT, prot))
    return make_errored_result<buffer_type>(GetLastError());
  return region;
}

result<map_handle::buffer_type> map_handle::decommit(buffer_type region) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  if(!region.first)
    return std::errc::invalid_argument;
  region = utils::round_to_page_size(region);
  if(!VirtualFree(region.first, region.second, MEM_DECOMMIT))
    return make_errored_result<buffer_type>(GetLastError());
  return region;
}

result<void> map_handle::zero_memory(buffer_type region) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  if(!region.first)
    return std::errc::invalid_argument;
  //! \todo Once you implement file_handle::zero(), please implement map_handle::zero()
  // buffer_type page_region { (char *) utils::round_up_to_page_size((uintptr_t) region.first), utils::round_down_to_page_size(region.second); };
  memset(region.first, 0, region.second);
  return success();
}

result<span<map_handle::buffer_type>> map_handle::prefetch(span<buffer_type> regions) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(0);
  if(!PrefetchVirtualMemory_)
    return span<map_handle::buffer_type>();
  PWIN32_MEMORY_RANGE_ENTRY wmre = (PWIN32_MEMORY_RANGE_ENTRY) regions.data();
  if(!PrefetchVirtualMemory_(GetCurrentProcess(), regions.size(), wmre, 0))
    return make_errored_result<span<map_handle::buffer_type>>(GetLastError());
  return regions;
}

result<map_handle::buffer_type> map_handle::do_not_store(buffer_type region) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(0);
  region = utils::round_to_page_size(region);
  if(!region.first)
    return std::errc::invalid_argument;
  // Windows does not support throwing away dirty pages on file backed maps
  if(!_section || !_section->backing())
  {
    // Win8's DiscardVirtualMemory is much faster if it's available
    if(DiscardVirtualMemory_)
    {
      if(!DiscardVirtualMemory_(region.first, region.second))
        return make_errored_result<buffer_type>(GetLastError());
      return region;
    }
    // Else MEM_RESET will do
    if(!VirtualAlloc(region.first, region.second, MEM_RESET, 0))
      return make_errored_result<buffer_type>(GetLastError());
    return region;
  }
  // We did nothing
  region.second = 0;
  return region;
}

map_handle::io_result<map_handle::buffers_type> map_handle::read(io_request<buffers_type> reqs, deadline) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  char *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? (size_type)(_length - reqs.offset) : 0;
  for(buffer_type &req : reqs.buffers)
  {
    if(togo)
    {
      req.first = addr;
      if(req.second > togo)
        req.second = togo;
      addr += req.second;
      togo -= req.second;
    }
    else
      req.second = 0;
  }
  return reqs.buffers;
}

map_handle::io_result<map_handle::const_buffers_type> map_handle::write(io_request<const_buffers_type> reqs, deadline) noexcept
{
  AFIO_LOG_FUNCTION_CALL(_v.h);
  char *addr = _addr + reqs.offset;
  size_type togo = reqs.offset < _length ? (size_type)(_length - reqs.offset) : 0;
  for(const_buffer_type &req : reqs.buffers)
  {
    if(togo)
    {
      if(req.second > togo)
        req.second = togo;
      memcpy(addr, req.first, req.second);
      req.first = addr;
      addr += req.second;
      togo -= req.second;
    }
    else
      req.second = 0;
  }
  return reqs.buffers;
}

AFIO_V2_NAMESPACE_END
