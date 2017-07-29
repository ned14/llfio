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
#include "../../../stat.hpp"
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

// allocate at process start to ensure later failure to allocate won't cause failure
static filesystem::path temporary_files_directory_("C:\\no_temporary_directories_accessible");
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
          if(buffer[len - 1] == '\\')
            buffer.resize(--len);
          buffer.append(L"\\afio_tempfile_probe_file.tmp");
          HANDLE h = CreateFile(buffer.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
          if(INVALID_HANDLE_VALUE != h)
          {
            CloseHandle(h);
            buffer.resize(len);
            temporary_files_directory_ = std::move(buffer);
            return true;
          }
          return false;
        };
        {
          error_code ec;
          // Try filesystem::temp_directory_path() before all else. This probably just calls GetTempPath().
          buffer = filesystem::temp_directory_path(ec);
          if(!ec && testpath())
            return;
        }
        // GetTempPath() returns one of (in order): %TMP%, %TEMP%, %USERPROFILE%, GetWindowsDirectory()\Temp
        static const wchar_t *variables[] = {L"TMP", L"TEMP", L"USERPROFILE"};
        for(size_t n = 0; n < sizeof(variables) / sizeof(variables[0]); n++)
        {
          buffer.resize(32768);
          DWORD len = GetEnvironmentVariable(variables[n], (LPWSTR) buffer.data(), (DWORD) buffer.size());
          if(len && len < buffer.size())
          {
            buffer.resize(len);
            if(variables[n][0] == 'U')
            {
              buffer.append(L"\\AppData\\Local\\Temp");
              if(testpath())
                return;
              buffer.resize(len);
              buffer.append(L"\\Local Settings\\Temp");
              if(testpath())
                return;
              buffer.resize(len);
            }
            if(testpath())
              return;
          }
        }
        // Finally if everything earlier failed e.g. if our environment block is zeroed,
        // fall back to Win3.1 era "the Windows directory" which definitely won't be
        // C:\Windows nowadays
        buffer.resize(32768);
        DWORD len = GetWindowsDirectory((LPWSTR) buffer.data(), (UINT) buffer.size());
        if(len && len < buffer.size())
        {
          buffer.resize(len);
          buffer.append(L"\\Temp");
          if(testpath())
            return;
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
      creatdisp = 0x00000005 /*FILE_OVERWRITE_IF*/;
      break;
    }

    attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
    OUTCOME_TRY(ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
    ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
    IO_STATUS_BLOCK isb = make_iostatus();

    path_view::c_str zpath(path);
    UNICODE_STRING _path;
    _path.Buffer = const_cast<wchar_t *>(zpath.buffer);
    _path.MaximumLength = (_path.Length = (USHORT)(zpath.length * sizeof(wchar_t))) + sizeof(wchar_t);
    if(zpath.length >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '\\' && _path.Buffer[2] == '.' && _path.Buffer[3] == '\\')
    {
      _path.Buffer += 3;
      _path.Length -= 3;
      _path.MaximumLength -= 3;
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
    path_view::c_str zpath(path);
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateFileW_(zpath.buffer, access, fileshare, NULL, creation, attribs, NULL)))
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return {(int) errcode, std::system_category()};
    }
  }
  if(!(flags & flag::disable_safety_unlinks))
  {
    OUTCOME_TRYV(ret.value()._fetch_inode());
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
  if(_creation == creation::truncate && ret.value().are_safety_fsyncs_issued())
    FlushFileBuffers(nativeh.h);
  return ret;
}

result<file_handle> file_handle::temp_inode(file_handle::path_view_type dirpath, mode _mode, flag flags) noexcept
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
  OUTCOME_TRY(dirh, path_handle::path(dirpath));
  DWORD creatdisp = 0x00000002 /*FILE_CREATE*/;

  attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
  OUTCOME_TRY(ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
  ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
  UNICODE_STRING _path;
  _path.MaximumLength = (_path.Length = (USHORT)(32 * sizeof(wchar_t))) + sizeof(wchar_t);

  OBJECT_ATTRIBUTES oa;
  memset(&oa, 0, sizeof(oa));
  oa.Length = sizeof(OBJECT_ATTRIBUTES);
  oa.ObjectName = &_path;
  oa.RootDirectory = dirh.native_handle().h;

  LARGE_INTEGER AllocationSize;
  memset(&AllocationSize, 0, sizeof(AllocationSize));
  std::string _random;
  std::wstring random(32, 0);
  for(;;)
  {
    try
    {
      _random = utils::random_string(32) + ".tmp";
      for(size_t n = 0; n < _random.size(); n++)
        random[n] = _random[n];
    }
    catch(...)
    {
      return error_from_exception();
    }
    _path.Buffer = const_cast<wchar_t *>(random.c_str());
    IO_STATUS_BLOCK isb = make_iostatus();
    NTSTATUS ntstat = NtCreateFile(&nativeh.h, access, &oa, &isb, &AllocationSize, attribs, fileshare, creatdisp, ntflags, NULL, 0);
    if(STATUS_PENDING == ntstat)
      ntstat = ntwait(nativeh.h, isb, deadline());
    if(ntstat < 0 && ntstat != (NTSTATUS) 0xC0000035L /*STATUS_OBJECT_NAME_COLLISION*/)
    {
      return {(int) ntstat, ntkernel_category()};
    }
    OUTCOME_TRYV(ret.value()._fetch_inode());  // It can be useful to know the inode of temporary inodes
    if(nativeh.h)
    {
      // Hide this item
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_BASIC_INFORMATION fbi;
      memset(&fbi, 0, sizeof(fbi));
      fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
      NtSetInformationFile(nativeh.h, &isb, &fbi, sizeof(fbi), FileBasicInformation);
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
    return std::errc::not_supported;
  AFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  OVERLAPPED ol;
  memset(&ol, 0, sizeof(ol));
  IO_STATUS_BLOCK *isb = (IO_STATUS_BLOCK *) &ol;
  *isb = make_iostatus();
  ULONG flags = 0;
  if(!wait_for_device)
    flags |= 2 /*FLUSH_FLAGS_NO_SYNC*/;
  if(!and_metadata)
    flags |= 1 /*FLUSH_FLAGS_FILE_DATA_ONLY*/;
  NtFlushBuffersFileEx(_v.h, flags, isb);
  if(_v.is_overlapped())
  {
    if(STATUS_TIMEOUT == ntwait(_v.h, ol, d))
    {
      CancelIoEx(_v.h, &ol);
      AFIO_WIN_DEADLINE_TO_TIMEOUT(d);
    }
  }
  if(STATUS_SUCCESS != isb->Status)
    return {(int) isb->Status, ntkernel_category()};
  return io_handle::io_result<const_buffers_type>(std::move(reqs.buffers));
}


result<file_handle> file_handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  result<file_handle> ret(file_handle(native_handle_type(), _devid, _inode, _caching, _flags));
  ret.value()._service = _service;
  ret.value()._v.behaviour = _v.behaviour;
  if(!DuplicateHandle(GetCurrentProcess(), _v.h, GetCurrentProcess(), &ret.value()._v.h, 0, false, DUPLICATE_SAME_ACCESS))
    return {GetLastError(), std::system_category()};
  return ret;
}

result<void> file_handle::relink(const path_handle &base, path_view_type path, deadline /*unused*/) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);

  path_view::c_str zpath(path);
  UNICODE_STRING _path;
  _path.Buffer = const_cast<wchar_t *>(zpath.buffer);
  _path.MaximumLength = (_path.Length = (USHORT)(zpath.length * sizeof(wchar_t))) + sizeof(wchar_t);
  if(zpath.length >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '\\' && _path.Buffer[2] == '.' && _path.Buffer[3] == '\\')
  {
    _path.Buffer += 3;
    _path.Length -= 3;
    _path.MaximumLength -= 3;
  }

  IO_STATUS_BLOCK isb = make_iostatus();
  alignas(8) char buffer[sizeof(FILE_RENAME_INFORMATION) + 65536];
  FILE_RENAME_INFORMATION *fni = (FILE_RENAME_INFORMATION *) buffer;
  fni->ReplaceIfExists = true;
  fni->RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
  fni->FileNameLength = _path.Length;
  memcpy(fni->FileName, _path.Buffer, fni->FileNameLength);
  NtSetInformationFile(_v.h, &isb, fni, sizeof(FILE_RENAME_INFORMATION) + fni->FileNameLength, FileRenameInformation);
  if(_flags & flag::overlapped)
    ntwait(_v.h, isb, deadline());
  if(STATUS_SUCCESS != isb.Status)
    return {(int) isb.Status, ntkernel_category()};
  return success();
}

result<void> file_handle::unlink(deadline /*unused*/) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  AFIO_LOG_FUNCTION_CALL(this);
  if(!(_flags & flag::win_disable_unlink_emulation))
  {
    // Rename it to something random to emulate immediate unlinking
    auto randomname = utils::random_string(32);
    randomname.append(".deleted");
    OUTCOME_TRY(cpath, current_path());
    cpath.remove_filename();
    OUTCOME_TRY(dirh, path_handle::path(cpath));
    OUTCOME_TRYV(relink(dirh, randomname));
  }
  // No point marking it for deletion if it's already been so
  if(!(_flags & flag::unlink_on_close))
  {
    // Hide the item in Explorer and the command line
    {
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_BASIC_INFORMATION fbi;
      memset(&fbi, 0, sizeof(fbi));
      fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
      NtSetInformationFile(_v.h, &isb, &fbi, sizeof(fbi), FileBasicInformation);
      if(_flags & flag::overlapped)
        ntwait(_v.h, isb, deadline());
    }
    // Mark the item as delete on close
    IO_STATUS_BLOCK isb = make_iostatus();
    FILE_DISPOSITION_INFORMATION fdi;
    memset(&fdi, 0, sizeof(fdi));
    fdi._DeleteFile = true;
    NtSetInformationFile(_v.h, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
    if(_flags & flag::overlapped)
      ntwait(_v.h, isb, deadline());
    if(STATUS_SUCCESS != isb.Status)
      return {(int) isb.Status, ntkernel_category()};
  }
  return success();
}

result<file_handle::extent_type> file_handle::length() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  FILE_STANDARD_INFO fsi;
  if(!GetFileInformationByHandleEx(_v.h, FileStandardInfo, &fsi, sizeof(fsi)))
    return {GetLastError(), std::system_category()};
  return fsi.EndOfFile.QuadPart;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  FILE_END_OF_FILE_INFO feofi;
  feofi.EndOfFile.QuadPart = newsize;
  if(!SetFileInformationByHandle(_v.h, FileEndOfFileInfo, &feofi, sizeof(feofi)))
    return {GetLastError(), std::system_category()};
  if(are_safety_fsyncs_issued())
  {
    FlushFileBuffers(_v.h);
  }
  return newsize;
}

AFIO_V2_NAMESPACE_END
