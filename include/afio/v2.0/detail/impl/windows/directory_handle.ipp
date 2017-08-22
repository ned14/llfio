/* A handle to a directory
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Aug 2017


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

#include "../../../directory_handle.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

result<directory_handle> directory_handle::directory(const path_handle &base, path_view_type path, mode _mode, creation _creation, caching _caching, flag flags) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  // We don't implement unlink on close
  flags &= ~flag::unlink_on_close;
  result<directory_handle> ret(directory_handle(native_handle_type(), 0, 0, _caching, flags));
  native_handle_type &nativeh = ret.value()._v;
  AFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::directory;
  DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  // Trying to truncate a directory returns EISDIR rather than some internal Win32 error code uncomparable to std::errc
  if(_creation == creation::truncate)
    return std::errc::is_a_directory;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, _mode, flags));
  OUTCOME_TRY(attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
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
    ntflags |= 0x01 /*FILE_DIRECTORY_FILE*/;  // required to open a directory
    IO_STATUS_BLOCK isb = make_iostatus();

    path_view::c_str zpath(path, true);
    UNICODE_STRING _path;
    _path.Buffer = const_cast<wchar_t *>(zpath.buffer);
    _path.MaximumLength = (_path.Length = (USHORT)(zpath.length * sizeof(wchar_t))) + sizeof(wchar_t);
    if(zpath.length >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '!' && _path.Buffer[2] == '!' && _path.Buffer[3] == '\\')
    {
      _path.Buffer += 3;
      _path.Length -= 3 * sizeof(wchar_t);
      _path.MaximumLength -= 3 * sizeof(wchar_t);
    }

    OBJECT_ATTRIBUTES oa;
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
    // oa.Attributes = 0x40 /*OBJ_CASE_INSENSITIVE*/;
    // if(!!(flags & file_flags::int_opening_link))
    //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;

    LARGE_INTEGER AllocationSize;
    memset(&AllocationSize, 0, sizeof(AllocationSize));
    NTSTATUS ntstat = NtCreateFile(&nativeh.h, access, &oa, &isb, &AllocationSize, attribs, fileshare, creatdisp, ntflags, NULL, 0);
    if(STATUS_PENDING == ntstat)
      ntstat = ntwait(nativeh.h, isb, deadline());
    if(ntstat < 0)
    {
      return {(int) ntstat, ntkernel_category()};
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
    attribs |= FILE_FLAG_BACKUP_SEMANTICS;  // required to open a directory
    path_view::c_str zpath(path, false);
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateFileW_(zpath.buffer, access, fileshare, NULL, creation, attribs, NULL, true)))
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return {(int) errcode, std::system_category()};
    }
  }
  if(!(flags & flag::disable_safety_unlinks))
  {
    if(!ret.value()._fetch_inode())
    {
      // If fetching inode failed e.g. were opening device, disable safety unlinks
      ret.value()._flags &= ~flag::disable_safety_unlinks;
    }
  }
  if(flags & flag::unlink_on_close)
  {
    // Hide this item
    IO_STATUS_BLOCK isb = make_iostatus();
    FILE_BASIC_INFORMATION fbi;
    memset(&fbi, 0, sizeof(fbi));
    fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
    NtSetInformationFile(nativeh.h, &isb, &fbi, sizeof(fbi), FileBasicInformation);
    if(flags & flag::overlapped)
      ntwait(nativeh.h, isb, deadline());
  }
  return ret;
}

result<directory_handle> directory_handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  result<directory_handle> ret(directory_handle(native_handle_type(), _devid, _inode, _caching, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  if(!DuplicateHandle(GetCurrentProcess(), _v.h, GetCurrentProcess(), &ret.value()._v.h, 0, false, DUPLICATE_SAME_ACCESS))
    return {GetLastError(), std::system_category()};
  return ret;
}

result<directory_handle::enumerate_info> directory_handle::enumerate(buffers_type &&tofill, path_view_type glob, filter filtering, span<char> kernelbuffer) const noexcept
{
  static constexpr stat_t::want default_stat_contents = stat_t::want::ino | stat_t::want::type | stat_t::want::atim | stat_t::want::mtim | stat_t::want::ctim | stat_t::want::size | stat_t::want::allocated | stat_t::want::birthtim | stat_t::want::sparse | stat_t::want::compressed | stat_t::want::reparse_point;
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(tofill.empty())
    return enumerate_info{std::move(tofill), stat_t::want::none, false};
  UNICODE_STRING _glob;
  memset(&_glob, 0, sizeof(_glob));
  path_view_type::c_str zglob(glob, true);
  if(!glob.empty())
  {
    _glob.Buffer = const_cast<wchar_t *>(zglob.buffer);
    _glob.Length = zglob.length * sizeof(wchar_t);
    _glob.MaximumLength = _glob.Length + sizeof(wchar_t);
  }
  if(!tofill._kernel_buffer && kernelbuffer.empty())
  {
    // Let's assume the average leafname will be 64 characters long.
    size_t toallocate = (sizeof(FILE_ID_FULL_DIR_INFORMATION) + 64 * sizeof(wchar_t)) * tofill.size();
    char *mem = new(std::nothrow) char[toallocate];
    if(!mem)
    {
      return std::errc::not_enough_memory;
    }
    tofill._kernel_buffer = std::unique_ptr<char[]>(mem);
    tofill._kernel_buffer_size = toallocate;
  }
  FILE_ID_FULL_DIR_INFORMATION *buffer;
  ULONG bytes;
  bool done = false;
  do
  {
    buffer = kernelbuffer.empty() ? (FILE_ID_FULL_DIR_INFORMATION *) tofill._kernel_buffer.get() : (FILE_ID_FULL_DIR_INFORMATION *) kernelbuffer.data();
    bytes = kernelbuffer.empty() ? (ULONG)(tofill._kernel_buffer_size) : (ULONG) kernelbuffer.size();
    IO_STATUS_BLOCK isb = make_iostatus();
    NTSTATUS ntstat = NtQueryDirectoryFile(_v.h, NULL, NULL, NULL, &isb, buffer, bytes, FileIdFullDirectoryInformation, FALSE, glob.empty() ? NULL : &_glob, TRUE);
    if(STATUS_PENDING == ntstat)
      ntstat = ntwait(_v.h, isb, deadline());
    if(kernelbuffer.empty() && STATUS_BUFFER_OVERFLOW == ntstat)
    {
      tofill._kernel_buffer.reset();
      size_t toallocate = tofill._kernel_buffer_size * 2;
      char *mem = new(std::nothrow) char[toallocate];
      if(!mem)
      {
        return std::errc::not_enough_memory;
      }
      tofill._kernel_buffer = std::unique_ptr<char[]>(mem);
      tofill._kernel_buffer_size = toallocate;
    }
    else
    {
      if(ntstat < 0)
      {
        return {(int) ntstat, ntkernel_category()};
      }
      done = true;
    }
  } while(!done);
  size_t n = 0;
  for(FILE_ID_FULL_DIR_INFORMATION *ffdi = buffer;; ffdi = (FILE_ID_FULL_DIR_INFORMATION *) ((uintptr_t) ffdi + ffdi->NextEntryOffset))
  {
    size_t length = ffdi->FileNameLength / sizeof(wchar_t);
    if(length <= 2 && '.' == ffdi->FileName[0])
    {
      if(1 == length || '.' == ffdi->FileName[1])
        continue;
    }
    // Try to zero terminate leafnames where possible for later efficiency
    if((uintptr_t)(ffdi->FileName + length) + sizeof(wchar_t) <= (uintptr_t) ffdi + ffdi->NextEntryOffset)
    {
      ffdi->FileName[length] = 0;
    }
    directory_entry &item = tofill[n];
    item.leafname = path_view(wstring_view(ffdi->FileName, length));
    if(filtering == filter::fastdeleted && item.leafname.is_afio_deleted())
      continue;
    item.stat = stat_t(nullptr);
    item.stat.st_ino = ffdi->FileId.QuadPart;
    item.stat.st_type = to_st_type(ffdi->FileAttributes, ffdi->ReparsePointTag);
    item.stat.st_atim = to_timepoint(ffdi->LastAccessTime);
    item.stat.st_mtim = to_timepoint(ffdi->LastWriteTime);
    item.stat.st_ctim = to_timepoint(ffdi->ChangeTime);
    item.stat.st_size = ffdi->EndOfFile.QuadPart;
    item.stat.st_allocated = ffdi->AllocationSize.QuadPart;
    item.stat.st_birthtim = to_timepoint(ffdi->CreationTime);
    item.stat.st_sparse = !!(ffdi->FileAttributes & FILE_ATTRIBUTE_SPARSE_FILE);
    item.stat.st_compressed = !!(ffdi->FileAttributes & FILE_ATTRIBUTE_COMPRESSED);
    item.stat.st_reparse_point = !!(ffdi->FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    n++;
    if(!ffdi->NextEntryOffset)
    {
      // Fill is complete
      tofill._resize(n);
      return enumerate_info{std::move(tofill), default_stat_contents, true};
    }
    if(n >= tofill.size())
    {
      // Fill is incomplete
      return enumerate_info{std::move(tofill), default_stat_contents, false};
    }
  }
}

AFIO_V2_NAMESPACE_END
