/* A handle to a file
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (16 commits)
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
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  AFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, _mode, flags));
  OUTCOME_TRY(attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
  bool need_to_set_sparse = false;
  if(base.is_valid() || path.is_ntpath())
  {
    DWORD creatdisp = 0x00000001 /*FILE_OPEN*/;
    switch(_creation)
    {
    case creation::open_existing:
      break;
    case creation::only_if_not_exist:
      creatdisp = 0x00000002 /*FILE_CREATE*/;
      break;
    case creation::if_needed:
      creatdisp = 0x00000003 /*FILE_OPEN_IF*/;
      break;
    case creation::truncate:
      creatdisp = 0x00000004 /*FILE_OVERWRITE*/;
      break;
    }

    attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
    OUTCOME_TRY(ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
    ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
    IO_STATUS_BLOCK isb = make_iostatus();

    path_view::c_str zpath(path, true);
    UNICODE_STRING _path{};
    _path.Buffer = const_cast<wchar_t *>(zpath.buffer);
    _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.length * sizeof(wchar_t))) + sizeof(wchar_t);
    if(zpath.length >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '!' && _path.Buffer[2] == '!' && _path.Buffer[3] == '\\')
    {
      _path.Buffer += 3;
      _path.Length -= 3 * sizeof(wchar_t);
      _path.MaximumLength -= 3 * sizeof(wchar_t);
    }

    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
    oa.Attributes = 0x40 /*OBJ_CASE_INSENSITIVE*/;
    // if(!!(flags & file_flags::int_opening_link))
    //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;

    LARGE_INTEGER AllocationSize{};
    memset(&AllocationSize, 0, sizeof(AllocationSize));
    NTSTATUS ntstat = NtCreateFile(&nativeh.h, access, &oa, &isb, &AllocationSize, attribs, fileshare, creatdisp, ntflags, nullptr, 0);
    if(STATUS_PENDING == ntstat)
    {
      ntstat = ntwait(nativeh.h, isb, deadline());
    }
    if(ntstat < 0)
    {
      return {static_cast<int>(ntstat), ntkernel_category()};
    }
    switch(_creation)
    {
    case creation::open_existing:
      need_to_set_sparse = false;
      break;
    case creation::only_if_not_exist:
      need_to_set_sparse = true;
      break;
    case creation::if_needed:
      need_to_set_sparse = (isb.Information == 2 /*FILE_CREATED*/);
      break;
    case creation::truncate:
      need_to_set_sparse = true;
      break;
    }
  }
  else
  {
    DWORD creation = OPEN_EXISTING;
    switch(_creation)
    {
    case creation::open_existing:
      break;
    case creation::only_if_not_exist:
      creation = CREATE_NEW;
      break;
    case creation::if_needed:
      creation = OPEN_ALWAYS;
      break;
    case creation::truncate:
      creation = TRUNCATE_EXISTING;
      break;
    }
    path_view::c_str zpath(path, false);
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateFileW_(zpath.buffer, access, fileshare, nullptr, creation, attribs, nullptr)))  // NOLINT
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return {static_cast<int>(errcode), std::system_category()};
    }
    switch(_creation)
    {
    case creation::open_existing:
      need_to_set_sparse = false;
      break;
    case creation::only_if_not_exist:
      need_to_set_sparse = true;
      break;
    case creation::if_needed:
      need_to_set_sparse = (GetLastError() == ERROR_ALREADY_EXISTS);
      break;
    case creation::truncate:
      need_to_set_sparse = true;
      break;
    }
  }
  if(need_to_set_sparse && !(flags & flag::win_disable_sparse_file_creation))
  {
    DWORD bytesout = 0;
    FILE_SET_SPARSE_BUFFER fssb = {1u};
    if(DeviceIoControl(nativeh.h, FSCTL_SET_SPARSE, &fssb, sizeof(fssb), nullptr, 0, &bytesout, nullptr) == 0)
    {
#if AFIO_LOGGING_LEVEL >= 3
      DWORD errcode = GetLastError();
      AFIO_LOG_WARN(&ret, "Failed to set file to sparse");
      result<void> r{errcode, std::system_category()};
      (void) r;  // throw away
#endif
    }
  }
  if(flags & flag::unlink_on_close)
  {
    // Hide this item
    IO_STATUS_BLOCK isb = make_iostatus();
    FILE_BASIC_INFORMATION fbi{};
    memset(&fbi, 0, sizeof(fbi));
    fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    NtSetInformationFile(nativeh.h, &isb, &fbi, sizeof(fbi), FileBasicInformation);
    if(flags & flag::overlapped)
    {
      ntwait(nativeh.h, isb, deadline());
    }
  }
  if(_creation == creation::truncate && ret.value().are_safety_fsyncs_issued())
  {
    FlushFileBuffers(nativeh.h);
  }
  return ret;
}

result<file_handle> file_handle::temp_inode(const path_handle &dirh, mode _mode, flag flags) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  caching _caching = caching::temporary;
  // No need to rename to random on unlink or check inode before unlink
  flags |= flag::unlink_on_close | flag::disable_safety_unlinks | flag::win_disable_unlink_emulation;
  result<file_handle> ret(file_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  AFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  DWORD fileshare = /* no read nor write access for others */ FILE_SHARE_DELETE;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, _mode, flags));
  OUTCOME_TRY(attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
  DWORD creatdisp = 0x00000002 /*FILE_CREATE*/;

  attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
  OUTCOME_TRY(ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
  ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
  UNICODE_STRING _path{};
  _path.MaximumLength = (_path.Length = static_cast<USHORT>(68 * sizeof(wchar_t))) + sizeof(wchar_t);

  OBJECT_ATTRIBUTES oa{};
  memset(&oa, 0, sizeof(oa));
  oa.Length = sizeof(OBJECT_ATTRIBUTES);
  oa.ObjectName = &_path;
  oa.RootDirectory = dirh.native_handle().h;

  LARGE_INTEGER AllocationSize{};
  memset(&AllocationSize, 0, sizeof(AllocationSize));
  std::string _random;
  std::wstring random(68, 0);
  for(;;)
  {
    try
    {
      _random = utils::random_string(32) + ".tmp";
      for(size_t n = 0; n < _random.size(); n++)
      {
        random[n] = _random[n];
      }
    }
    catch(...)
    {
      return error_from_exception();
    }
    _path.Buffer = const_cast<wchar_t *>(random.c_str());
    {
      IO_STATUS_BLOCK isb = make_iostatus();
      NTSTATUS ntstat = NtCreateFile(&nativeh.h, access, &oa, &isb, &AllocationSize, attribs, fileshare, creatdisp, ntflags, nullptr, 0);
      if(STATUS_PENDING == ntstat)
      {
        ntstat = ntwait(nativeh.h, isb, deadline());
      }
      if(ntstat < 0 && ntstat != static_cast<NTSTATUS>(0xC0000035L) /*STATUS_OBJECT_NAME_COLLISION*/)
      {
        return {static_cast<int>(ntstat), ntkernel_category()};
      }
    }
    // std::cerr << random << std::endl;
    if(nativeh.h != nullptr)
    {
      // Hide this item
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_BASIC_INFORMATION fbi{};
      memset(&fbi, 0, sizeof(fbi));
      fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
      NtSetInformationFile(nativeh.h, &isb, &fbi, sizeof(fbi), FileBasicInformation);

      // Mark the item as delete on close
      isb = make_iostatus();
      FILE_DISPOSITION_INFORMATION fdi{};
      memset(&fdi, 0, sizeof(fdi));
      fdi._DeleteFile = 1u;
      NTSTATUS ntstat = NtSetInformationFile(nativeh.h, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
      if(ntstat >= 0)
      {
        // No need to delete it again on close
        ret.value()._flags &= ~flag::unlink_on_close;
      }
    }
    return ret;
  }
}

file_handle::io_result<file_handle::const_buffers_type> file_handle::barrier(file_handle::io_request<file_handle::const_buffers_type> reqs, bool wait_for_device, bool and_metadata, deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(d && !_v.is_overlapped())
  {
    return errc::not_supported;
  }
  AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  OVERLAPPED ol{};
  memset(&ol, 0, sizeof(ol));
  auto *isb = reinterpret_cast<IO_STATUS_BLOCK *>(&ol);
  *isb = make_iostatus();
  ULONG flags = 0;
  if(!wait_for_device)
  {
    flags |= 2 /*FLUSH_FLAGS_NO_SYNC*/;
  }
  if(!and_metadata)
  {
    flags |= 1 /*FLUSH_FLAGS_FILE_DATA_ONLY*/;
  }
  NTSTATUS ntstat = NtFlushBuffersFileEx(_v.h, flags, isb);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(_v.h, ol, d);
    if(STATUS_TIMEOUT == ntstat)
    {
      CancelIoEx(_v.h, &ol);
      AFIO_WIN_DEADLINE_TO_TIMEOUT(d);
    }
  }
  if(ntstat < 0)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  return {reqs.buffers};
}

result<file_handle> file_handle::clone(mode mode_, caching caching_, deadline /*unused*/) const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // Fast path
  if(mode_ == mode::unchanged && caching_ == caching::unchanged)
  {
    result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, caching_, _flags));
    ret.value()._service = _service;
    ret.value()._v.behaviour = _v.behaviour;
    if(DuplicateHandle(GetCurrentProcess(), _v.h, GetCurrentProcess(), &ret.value()._v.h, 0, 0, DUPLICATE_SAME_ACCESS) == 0)
    {
      return {GetLastError(), std::system_category()};
    }
    return ret;
  }
  // Slow path
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, caching_, _flags));
  native_handle_type &nativeh = ret.value()._v;
  nativeh.behaviour |= native_handle_type::disposition::file;
  DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, mode_, _flags));
  OUTCOME_TRYV(attributes_from_handle_caching_and_flags(nativeh, caching_, _flags));
  OUTCOME_TRY(ntflags, ntflags_from_handle_caching_and_flags(nativeh, caching_, _flags));
  ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
  OBJECT_ATTRIBUTES oa{};
  memset(&oa, 0, sizeof(oa));
  oa.Length = sizeof(OBJECT_ATTRIBUTES);
  // It is entirely undocumented that this is how you clone a file handle with new privs
  UNICODE_STRING _path{};
  memset(&_path, 0, sizeof(_path));
  oa.ObjectName = &_path;
  oa.RootDirectory = _v.h;
  IO_STATUS_BLOCK isb = make_iostatus();
  NTSTATUS ntstat = NtOpenFile(&nativeh.h, access, &oa, &isb, fileshare, ntflags);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(nativeh.h, isb, deadline());
  }
  if(ntstat < 0)
  {
    return {static_cast<int>(ntstat), ntkernel_category()};
  }
  return ret;
}

result<file_handle::extent_type> file_handle::maximum_extent() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
#if 0
  char buffer[1];
  DWORD read = 0;
  OVERLAPPED ol;
  memset(&ol, 0, sizeof(ol));
  ol.OffsetHigh = ol.Offset = 0xffffffff;
  WriteFile(_v.h, buffer, 0, &read, &ol);
#endif
  FILE_STANDARD_INFO fsi{};
  if(GetFileInformationByHandleEx(_v.h, FileStandardInfo, &fsi, sizeof(fsi)) == 0)
  {
    return {GetLastError(), std::system_category()};
  }
  return fsi.EndOfFile.QuadPart;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  FILE_END_OF_FILE_INFO feofi{};
  feofi.EndOfFile.QuadPart = newsize;
  if(SetFileInformationByHandle(_v.h, FileEndOfFileInfo, &feofi, sizeof(feofi)) == 0)
  {
    return {GetLastError(), std::system_category()};
  }
  if(are_safety_fsyncs_issued())
  {
    FlushFileBuffers(_v.h);
  }
  return newsize;
}

result<std::vector<std::pair<file_handle::extent_type, file_handle::extent_type>>> file_handle::extents() const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  try
  {
    static_assert(sizeof(std::pair<file_handle::extent_type, file_handle::extent_type>) == sizeof(FILE_ALLOCATED_RANGE_BUFFER), "FILE_ALLOCATED_RANGE_BUFFER is not equivalent to pair<extent_type, extent_type>!");
    std::vector<std::pair<file_handle::extent_type, file_handle::extent_type>> ret;
#ifdef NDEBUG
    ret.resize(64);
#else
    ret.resize(1);
#endif
    FILE_ALLOCATED_RANGE_BUFFER farb{};
    farb.FileOffset.QuadPart = 0;
    farb.Length.QuadPart = (static_cast<extent_type>(1) << 63) - 1;  // Microsoft claims this is 1<<64-1024 for NTFS, but I get bad parameter error with anything higher than 1<<63-1.
    DWORD bytesout = 0;
    OVERLAPPED ol{};
    memset(&ol, 0, sizeof(ol));
    ol.Internal = static_cast<ULONG_PTR>(-1);
    while(DeviceIoControl(_v.h, FSCTL_QUERY_ALLOCATED_RANGES, &farb, sizeof(farb), ret.data(), static_cast<DWORD>(ret.size() * sizeof(FILE_ALLOCATED_RANGE_BUFFER)), &bytesout, &ol) == 0)
    {
      if(ERROR_INSUFFICIENT_BUFFER == GetLastError() || ERROR_MORE_DATA == GetLastError())
      {
        ret.resize(ret.size() * 2);
        continue;
      }
      if(ERROR_SUCCESS != GetLastError())
      {
        return {GetLastError(), std::system_category()};
      }
    }
    return ret;
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<file_handle::extent_type> file_handle::zero(file_handle::extent_type offset, file_handle::extent_type bytes, deadline /*unused*/) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(offset + bytes < offset)
  {
    return errc::value_too_large;
  }
  FILE_ZERO_DATA_INFORMATION fzdi{};
  fzdi.FileOffset.QuadPart = offset;
  fzdi.BeyondFinalZero.QuadPart = offset + bytes;
  DWORD bytesout = 0;
  OVERLAPPED ol{};
  memset(&ol, 0, sizeof(ol));
  ol.Internal = static_cast<ULONG_PTR>(-1);
  if(DeviceIoControl(_v.h, FSCTL_SET_ZERO_DATA, &fzdi, sizeof(fzdi), nullptr, 0, &bytesout, &ol) == 0)
  {
    if(ERROR_IO_PENDING == GetLastError())
    {
      NTSTATUS ntstat = ntwait(_v.h, ol, deadline());
      if(ntstat != 0)
      {
        return {ntstat, ntkernel_category()};
      }
    }
    if(ERROR_SUCCESS != GetLastError())
    {
      return {GetLastError(), std::system_category()};
    }
  }
  return success();
}

AFIO_V2_NAMESPACE_END
