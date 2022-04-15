/* A filing system handle
(C) 2017-2022 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#include "../../../fs_handle.hpp"
#include "../../../stat.hpp"
#include "../../../utils.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

result<void> fs_handle::_fetch_inode() const noexcept
{
  stat_t s;
  OUTCOME_TRYV(s.fill(_get_handle(), stat_t::want::dev | stat_t::want::ino));
  _devid = s.st_dev;
  _inode = s.st_ino;
  return success();
}

result<path_handle> fs_handle::parent_path_handle(deadline d) const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  LLFIO_WIN_DEADLINE_TO_SLEEP_INIT(d);
  if(_devid == 0 && _inode == 0)
  {
    OUTCOME_TRY(_fetch_inode());
  }
  try
  {
    for(;;)
    {
      // Get current path for handle and open its containing dir
      OUTCOME_TRY(auto &&currentpath, h.current_path());
      // If current path is empty, it's been deleted
      if(currentpath.empty())
      {
        return errc::no_such_file_or_directory;
      }
      // Split the path into root and leafname
      filesystem::path filename = currentpath.filename();
      currentpath = currentpath.remove_filename();
      /* We have to be super careful here because \Device\HarddiskVolume4 != \Device\HarddiskVolume4\!
      The former opens the device, the latter the root directory of the device.
      */
      if(currentpath.native().back() != '\\')
      {
        const_cast<filesystem::path::string_type &>(currentpath.native()).push_back('\\');
      }
      auto currentdirh_ = path_handle::path(currentpath);
      if(!currentdirh_)
      {
        continue;
      }
      path_handle currentdirh = std::move(currentdirh_.value());
      if((h.flags() & handle::flag::disable_safety_unlinks) != 0 || h.is_symlink())
      {
        return success(std::move(currentdirh));
      }

      DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
      IO_STATUS_BLOCK isb = make_iostatus();
      path_view::not_zero_terminated_rendered_path<> zpath(filename);
      UNICODE_STRING _path{};
      _path.Buffer = const_cast<wchar_t *>(zpath.data());
      _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);
      OBJECT_ATTRIBUTES oa{};
      memset(&oa, 0, sizeof(oa));
      oa.Length = sizeof(OBJECT_ATTRIBUTES);
      oa.ObjectName = &_path;
      oa.RootDirectory = currentdirh.native_handle().h;
      LARGE_INTEGER AllocationSize{};
      memset(&AllocationSize, 0, sizeof(AllocationSize));
      HANDLE nh = nullptr;
      NTSTATUS ntstat =
      NtCreateFile(&nh, SYNCHRONIZE, &oa, &isb, &AllocationSize, 0, fileshare, 0x00000001 /*FILE_OPEN*/, 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/, nullptr, 0);
      if(STATUS_SUCCESS != ntstat)
      {
        if(static_cast<NTSTATUS>(0xC000000F) /*STATUS_NO_SUCH_FILE*/ == ntstat || static_cast<NTSTATUS>(0xC0000034) /*STATUS_OBJECT_NAME_NOT_FOUND*/ == ntstat)
        {
          continue;
        }
        return ntkernel_error(ntstat);
      }
      auto unnh = make_scope_exit([nh]() noexcept { CloseHandle(nh); });
      (void) unnh;
      isb.Status = -1;
      FILE_INTERNAL_INFORMATION fii{};
      ntstat = NtQueryInformationFile(nh, &isb, &fii, sizeof(fii), FileInternalInformation);
      if(STATUS_SUCCESS != ntstat)
      {
        continue;
      }
      // If the same, we know for a fact that this is the correct containing dir for now at least
      // FIXME: We are not comparing device number, that's faked as the volume number in stat_t
      if(static_cast<ino_t>(fii.IndexNumber.QuadPart) == _inode)
      {
        return success(std::move(currentdirh));
      }
      LLFIO_WIN_DEADLINE_TO_TIMEOUT_LOOP(d);
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<void> fs_handle::relink(const path_handle &base, path_view_type path, bool atomic_replace, deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);

  // If the target is a win32 path, we need to convert to NT path and call ourselves
  if(!base.is_valid() && !path.is_ntpath())
  {
    path_view::zero_terminated_rendered_path<> zpath(path);
    UNICODE_STRING NtPath{};
    if(RtlDosPathNameToNtPathName_U(zpath.data(), &NtPath, nullptr, nullptr) == 0u)
    {
      return win32_error(ERROR_FILE_NOT_FOUND);
    }
    auto unntpath = make_scope_exit(
    [&NtPath]() noexcept
    {
      if(HeapFree(GetProcessHeap(), 0, NtPath.Buffer) == 0)
      {
        abort();
      }
    });
    // RtlDosPathNameToNtPathName_U outputs \??\path, so path.is_ntpath() will be false.
    return relink(base, path_view_type(NtPath.Buffer, NtPath.Length / sizeof(wchar_t), path_view::zero_terminated), atomic_replace, d);
  }

  HANDLE duph = INVALID_HANDLE_VALUE;
#if 0  // Seems to not be necessary for directories to have DELETE privs to rename them?
  // Get my DELETE privs if possible
  if(h.is_directory())
  {
    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    // It is entirely undocumented that this is how you clone a file handle with new privs
    UNICODE_STRING _path{};
    memset(&_path, 0, sizeof(_path));
    oa.ObjectName = &_path;
    oa.RootDirectory = h.native_handle().h;
    IO_STATUS_BLOCK isb = make_iostatus();
    DWORD ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/;
    if(h.is_symlink())
      ntflags |= 0x00200000 /*FILE_OPEN_REPARSE_POINT*/;
    NTSTATUS ntstat = NtOpenFile(&duph, SYNCHRONIZE | DELETE, &oa, &isb, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, ntflags);
    // Can't duplicate the handle of a pipe not in the listening state
    if(ntstat < 0 && !h.is_pipe())
    {
      return ntkernel_error(ntstat);
    }
  }
#endif
  auto unduph = make_scope_exit([&duph]() noexcept { CloseHandle(duph); });
  // If we failed to duplicate the handle, try using the original handle
  if(duph == INVALID_HANDLE_VALUE)
  {
    unduph.release();
    duph = h.native_handle().h;
  }

  path_view::not_zero_terminated_rendered_path<> zpath(path);
  UNICODE_STRING _path{};
  _path.Buffer = const_cast<wchar_t *>(zpath.data());
  _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);
  if(zpath.size() >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '!' && _path.Buffer[2] == '!' && _path.Buffer[3] == '\\')
  {
    _path.Buffer += 3;
    _path.Length -= 3 * sizeof(wchar_t);
    _path.MaximumLength -= 3 * sizeof(wchar_t);
  }
  IO_STATUS_BLOCK isb = make_iostatus();
  alignas(8) char buffer[sizeof(FILE_RENAME_INFORMATION) + 65536];
  auto *fni = reinterpret_cast<FILE_RENAME_INFORMATION *>(buffer);
  fni->Flags = atomic_replace ? (0x1 /*FILE_RENAME_REPLACE_IF_EXISTS*/ | 0x2 /*FILE_RENAME_POSIX_SEMANTICS*/) : 0;
  fni->RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
  fni->FileNameLength = _path.Length;
  memcpy(fni->FileName, _path.Buffer, fni->FileNameLength);
  NTSTATUS ntstat = NtSetInformationFile(duph, &isb, fni, sizeof(FILE_RENAME_INFORMATION) + fni->FileNameLength, FileRenameInformation);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(duph, isb, d);
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  return success();
}

result<void> fs_handle::link(const path_handle &base, path_view_type path, deadline d) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);

  // If the target is a win32 path, we need to convert to NT path and call ourselves
  if(!base.is_valid() && !path.is_ntpath())
  {
    path_view::zero_terminated_rendered_path<> zpath(path);
    UNICODE_STRING NtPath{};
    if(RtlDosPathNameToNtPathName_U(zpath.data(), &NtPath, nullptr, nullptr) == 0u)
    {
      return win32_error(ERROR_FILE_NOT_FOUND);
    }
    auto unntpath = make_scope_exit(
    [&NtPath]() noexcept
    {
      if(HeapFree(GetProcessHeap(), 0, NtPath.Buffer) == 0)
      {
        abort();
      }
    });
    // RtlDosPathNameToNtPathName_U outputs \??\path, so path.is_ntpath() will be false.
    return link(base, path_view_type(NtPath.Buffer, NtPath.Length / sizeof(wchar_t), path_view::zero_terminated), d);
  }

  HANDLE duph = h.native_handle().h;

  path_view::not_zero_terminated_rendered_path<> zpath(path);
  UNICODE_STRING _path{};
  _path.Buffer = const_cast<wchar_t *>(zpath.data());
  _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);
  if(zpath.size() >= 4 && _path.Buffer[0] == '\\' && _path.Buffer[1] == '!' && _path.Buffer[2] == '!' && _path.Buffer[3] == '\\')
  {
    _path.Buffer += 3;
    _path.Length -= 3 * sizeof(wchar_t);
    _path.MaximumLength -= 3 * sizeof(wchar_t);
  }
  IO_STATUS_BLOCK isb = make_iostatus();
  alignas(8) char buffer[sizeof(FILE_LINK_INFORMATION) + 65536];
  auto *fni = reinterpret_cast<FILE_LINK_INFORMATION *>(buffer);
  fni->ReplaceIfExists = 0;
  fni->RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
  fni->FileNameLength = _path.Length;
  memcpy(fni->FileName, _path.Buffer, fni->FileNameLength);
  NTSTATUS ntstat = NtSetInformationFile(duph, &isb, fni, sizeof(FILE_LINK_INFORMATION) + fni->FileNameLength, FileLinkInformation);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(duph, isb, d);
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  return success();
}

result<void> fs_handle::unlink(deadline d) noexcept
{
  using flag = handle::flag;
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  HANDLE duph = INVALID_HANDLE_VALUE;
  // Get myself DELETE privs if possible
  if(h.is_directory())
  {
    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    // It is entirely undocumented that this is how you clone a file handle with new privs
    UNICODE_STRING _path{};
    memset(&_path, 0, sizeof(_path));
    oa.ObjectName = &_path;
    oa.RootDirectory = h.native_handle().h;
    IO_STATUS_BLOCK isb = make_iostatus();
    DWORD ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/;
    if(h.is_symlink())
      ntflags |= 0x00200000 /*FILE_OPEN_REPARSE_POINT*/;
    NTSTATUS ntstat = NtOpenFile(&duph, SYNCHRONIZE | DELETE, &oa, &isb, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, ntflags);
    // Note that Windows appears to not permit renaming nor deletion of pipe handles right now,
    // so do nothing about the STATUS_PIPE_NOT_AVAILABLE failure here
    if(ntstat < 0)
    {
      return ntkernel_error(ntstat);
    }
  }
  auto unduph = make_scope_exit([&duph]() noexcept { CloseHandle(duph); });
  // If we failed to duplicate the handle, try using the original handle
  if(duph == INVALID_HANDLE_VALUE)
  {
    unduph.release();
    duph = h.native_handle().h;
  }
  bool failed = true;
  // Try POSIX delete first, this will fail on Windows 10 before 1709, or if not NTFS
  {
    IO_STATUS_BLOCK isb = make_iostatus();
    FILE_DISPOSITION_INFORMATION_EX fdie{};
    memset(&fdie, 0, sizeof(fdie));
    fdie.Flags = 0x1 /*FILE_DISPOSITION_DELETE*/ | 0x2 /*FILE_DISPOSITION_POSIX_SEMANTICS*/;
    NTSTATUS ntstat = NtSetInformationFile(duph, &isb, &fdie, sizeof(fdie), FileDispositionInformationEx);
    if(ntstat >= 0)
    {
      failed = false;
    }
  }
  if(failed)
  {
    if((h.is_regular() || h.is_symlink()) && !(h.flags() & flag::win_disable_unlink_emulation))
    {
      // Rename it to something random to emulate immediate unlinking
      std::string randomname;
      try
      {
        randomname = utils::random_string(32);
        randomname.append(".deleted");
      }
      catch(...)
      {
        return error_from_exception();
      }
      OUTCOME_TRY(auto &&dirh, parent_path_handle(d));
      result<void> out = relink(dirh, randomname);
      if(!out)
      {
        // If something else is using it, we may not be able to rename
        // This error also annoyingly appears if the file has delete on close set on it already
        if(out.error().value() == static_cast<int>(0xC0000043) /*STATUS_SHARING_VIOLATION*/)
        {
          LLFIO_LOG_WARN(this, "Failed to rename entry to random name to simulate immediate unlinking due to STATUS_SHARING_VIOLATION, skipping");
        }
        else
        {
          return std::move(out).error();
        }
      }
    }
    // No point marking it for deletion if it's already been so
    if(!(h.flags() & flag::unlink_on_first_close))
    {
      // Hide the item in Explorer and the command line
      {
        IO_STATUS_BLOCK isb = make_iostatus();
        FILE_BASIC_INFORMATION fbi{};
        memset(&fbi, 0, sizeof(fbi));
        fbi.FileAttributes = FILE_ATTRIBUTE_HIDDEN;
        NTSTATUS ntstat = NtSetInformationFile(h.native_handle().h, &isb, &fbi, sizeof(fbi), FileBasicInformation);
        if(STATUS_PENDING == ntstat)
        {
          ntstat = ntwait(h.native_handle().h, isb, d);
        }
        (void) ntstat;
      }
      // Mark the item as delete on close
      IO_STATUS_BLOCK isb = make_iostatus();
      FILE_DISPOSITION_INFORMATION fdi{};
      memset(&fdi, 0, sizeof(fdi));
      fdi._DeleteFile = 1u;
      NTSTATUS ntstat = NtSetInformationFile(duph, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
      if(STATUS_PENDING == ntstat)
      {
        ntstat = ntwait(duph, isb, d);
      }
      if(ntstat < 0)
      {
        return ntkernel_error(ntstat);
      }
    }
  }
  return success();
}

namespace detail
{
  static inline result<handle> create_alternate_stream(const handle &base, path_view path, handle::mode _mode, handle::creation _creation) noexcept
  {
    using namespace windows_nt_kernel;
    const handle::caching _caching = handle::caching::all;
    const handle::flag flags = handle::flag::none;
    native_handle_type nativeh;
    DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    OUTCOME_TRY(auto &&access, access_mask_from_handle_mode(nativeh, _mode, flags));
    OUTCOME_TRY(auto &&attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
    DWORD creatdisp = 0x00000001 /*FILE_OPEN*/;
    switch(_creation)
    {
    case handle::creation::open_existing:
      break;
    case handle::creation::only_if_not_exist:
      creatdisp = 0x00000002 /*FILE_CREATE*/;
      break;
    case handle::creation::if_needed:
      creatdisp = 0x00000003 /*FILE_OPEN_IF*/;
      break;
    case handle::creation::truncate_existing:
      creatdisp = 0x00000004 /*FILE_OVERWRITE*/;
      break;
    case handle::creation::always_new:
      creatdisp = 0x00000000 /*FILE_SUPERSEDE*/;
      break;
    }

    attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
    OUTCOME_TRY(auto &&ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
    ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
    IO_STATUS_BLOCK isb = make_iostatus();

    // We need to prepend a ':', so this gets handled a little different
    path_view::zero_terminated_rendered_path<> zpath(
    [&]() -> path_view::zero_terminated_rendered_path<>
    {
      return visit(path,
                   [&](auto sv) -> path_view::zero_terminated_rendered_path<>
                   {
                     using sv_type = std::decay_t<decltype(sv)>;
                     using value_type = typename sv_type::value_type;
                     auto *buffer = (value_type *) alloca((sv.size() + 1) * sizeof(value_type));
                     buffer[0] = ':';
                     memcpy(buffer + 1, sv.data(), sv.size() * sizeof(value_type));
                     // Force an immediate render into the internal buffer
                     return path_view::zero_terminated_rendered_path<>(path_view(buffer, sv.size() + 1, path_view::not_zero_terminated));
                   });
    }());
    UNICODE_STRING _path{};
    _path.Buffer = const_cast<wchar_t *>(zpath.data());
    _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);

    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.RootDirectory = base.native_handle().h;
    oa.Attributes = 0;  // 0x40 /*OBJ_CASE_INSENSITIVE*/;
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
      return ntkernel_error(ntstat);
    }
    return handle(nativeh, flags);
  }
}  // namespace detail

result<span<path_view_component>> fs_handle::list_extended_attributes(span<byte> tofill) const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  IO_STATUS_BLOCK isb = make_iostatus();
  NTSTATUS ntstat = NtQueryInformationFile(h.native_handle().h, &isb, tofill.data(), (ULONG) tofill.size(), FileStreamInformation);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(h.native_handle().h, isb, deadline());
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  span<path_view_component> filled;
  {
    auto *p = tofill.data();
    bool done = false;
    size_t count = 0;
    while(!done && p < tofill.data() + isb.Information)
    {
      auto *i = (_FILE_STREAM_INFORMATION *) p;
      if(i->StreamNameLength > 1 && i->StreamName[0] == ':' && i->StreamName[1] != ':')
      {
        count++;
      }
      if(i->NextEntryOffset == 0)
      {
        done = true;
      }
      else
      {
        p += i->NextEntryOffset;
      }
    }
    if(count == 0)
    {
      return filled;
    }
    const auto offset = (isb.Information + 7) & ~7;
    auto tofillremaining = tofill.size() - offset;
    if(count * sizeof(path_view_component) > tofillremaining)
    {
      return errc::no_buffer_space;
    }
    filled = {(path_view_component *) (p + offset), count};
  }
  {
    auto *p = tofill.data();
    bool done = false;
    size_t count = 0;
    while(!done && p < tofill.data() + isb.Information)
    {
      auto *i = (_FILE_STREAM_INFORMATION *) p;
      // Strip out all :: prefixed streams, which are internal ones
      if(i->StreamNameLength > 1 && i->StreamName[0] == ':' && i->StreamName[1] != ':')
      {
        // Alternate data streams can themselves have alternate data streams, so filter out any starting with '$' as those are internal
        auto length = i->StreamNameLength / sizeof(wchar_t);
        for(auto *c = i->StreamName + length; c > i->StreamName; c--)
        {
          if(c[0] == '$' && c > i->StreamName && c[-1] == ':')
          {
            length = c - i->StreamName - 1;
            break;
          }
        }
        filled[count] = path_view_component(i->StreamName + 1, length - 1, path_view_component::not_zero_terminated);
        count++;
      }
      if(i->NextEntryOffset == 0)
      {
        done = true;
      }
      else
      {
        p += i->NextEntryOffset;
      }
    }
  }
  return filled;
}

result<span<byte>> fs_handle::get_extended_attribute(span<byte> tofill, path_view_component name) const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  OUTCOME_TRY(auto &&attribh, detail::create_alternate_stream(h, name, handle::mode::read, handle::creation::open_existing));
  IO_STATUS_BLOCK isb = make_iostatus();
  NTSTATUS ntstat = NtReadFile(attribh.native_handle().h, nullptr, nullptr, nullptr, &isb, tofill.data(), (ULONG) tofill.size(), nullptr, nullptr);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(attribh.native_handle().h, isb, deadline());
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  return {tofill.data(), isb.Information};
}

result<void> fs_handle::set_extended_attribute(path_view_component name, span<const byte> value) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);

  /* To correctly emulate POSIX extended attributes, we must prevent any chance of torn reads, so
  we firstly write the attribute's value into a randomly named alternate stream, and then atomically
  rename it over the destination attribute. From the perspective of any concurrent attribute queryers,
  the value will atomically update.
  */
  handle attribh;
  try
  {
    for(;;)
    {
      auto randomname = utils::random_string(32);
      result<handle> ret = detail::create_alternate_stream(h, randomname, handle::mode::write, handle::creation::only_if_not_exist);
      if(ret)
      {
        attribh = std::move(ret).assume_value();
        break;
      }
      else if(ret.error() != errc::file_exists)
      {
        OUTCOME_TRY(std::move(ret));
      }
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
  auto unattribh = QUICKCPPLIB_NAMESPACE::scope::make_scope_exit(
  [&]() noexcept
  {
    // Mark the item as delete on close
    IO_STATUS_BLOCK isb = make_iostatus();
    FILE_DISPOSITION_INFORMATION fdi{};
    memset(&fdi, 0, sizeof(fdi));
    fdi._DeleteFile = 1u;
    (void) NtSetInformationFile(attribh.native_handle().h, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
  });

  IO_STATUS_BLOCK isb = make_iostatus();
  NTSTATUS ntstat = NtWriteFile(attribh.native_handle().h, nullptr, nullptr, nullptr, &isb, (PVOID) value.data(), (ULONG) value.size(), nullptr, nullptr);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(attribh.native_handle().h, isb, deadline());
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  if(isb.Information < value.size())
  {
    return errc::no_space_on_device;
  }

  // We need to prepend a ':', so this gets handled a little different
  path_view::zero_terminated_rendered_path<> zpath(
  [&]() -> path_view::zero_terminated_rendered_path<>
  {
    return visit(name,
                 [&](auto sv) -> path_view::zero_terminated_rendered_path<>
                 {
                   using sv_type = std::decay_t<decltype(sv)>;
                   using value_type = typename sv_type::value_type;
                   auto *buffer = (value_type *) alloca((sv.size() + 1) * sizeof(value_type));
                   buffer[0] = ':';
                   memcpy(buffer + 1, sv.data(), sv.size() * sizeof(value_type));
                   // Force an immediate render into the internal buffer
                   return path_view::zero_terminated_rendered_path<>(path_view(buffer, sv.size() + 1, path_view::not_zero_terminated));
                 });
  }());
  UNICODE_STRING _path{};
  _path.Buffer = const_cast<wchar_t *>(zpath.data());
  _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);
  isb = make_iostatus();
  alignas(8) char buffer[sizeof(FILE_RENAME_INFORMATION) + 65536];
  auto *fni = reinterpret_cast<FILE_RENAME_INFORMATION *>(buffer);
  fni->Flags = (0x1 /*FILE_RENAME_REPLACE_IF_EXISTS*/ | 0x2 /*FILE_RENAME_POSIX_SEMANTICS*/);
  fni->RootDirectory = nullptr;  // Apparently this must be nullptr, not h.native_handle().h
  fni->FileNameLength = _path.Length;
  memcpy(fni->FileName, _path.Buffer, fni->FileNameLength);
  ntstat = NtSetInformationFile(attribh.native_handle().h, &isb, fni, sizeof(FILE_RENAME_INFORMATION) + fni->FileNameLength, FileRenameInformation);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(attribh.native_handle().h, isb, deadline());
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  unattribh.release();
  return success();
}

result<void> fs_handle::remove_extended_attribute(path_view_component name) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  OUTCOME_TRY(auto &&attribh, detail::create_alternate_stream(h, name, handle::mode::attr_write, handle::creation::open_existing));
  // Mark the item as delete on close
  IO_STATUS_BLOCK isb = make_iostatus();
  FILE_DISPOSITION_INFORMATION fdi{};
  memset(&fdi, 0, sizeof(fdi));
  fdi._DeleteFile = 1u;
  NTSTATUS ntstat = NtSetInformationFile(attribh.native_handle().h, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(attribh.native_handle().h, isb, deadline());
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  return success();
}


/************************************************ NTFS extended attributes *********************************************************/

namespace detail
{
  struct win_extended_attributes_return_t
  {
    span<std::pair<path_view_component, span<byte>>> filled;
    size_t tofillremaining;
  };
  static inline result<win_extended_attributes_return_t> win_extended_attributes(span<byte> tofill, const handle &h,
                                                                                 span<const path_view_component> names) noexcept
  {
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    IO_STATUS_BLOCK isb = make_iostatus();
    _FILE_GET_EA_INFORMATION *ealist = nullptr;
    ULONG ealistlen = 0;
    for(auto name : names)
    {
      if(name.native_size() > 255)
      {
        return errc::invalid_argument;
      }
      if(!visit(name, [](auto sv) { return sizeof(sv[0]) == 1; }))
      {
        return errc::invalid_argument;
      }
      ealistlen += (ULONG) ((sizeof(ULONG) + 1 + name.native_size() + 3) & ~3);
    }
    if(ealistlen > 0)
    {
      ealist = (_FILE_GET_EA_INFORMATION *) alloca(ealistlen);
      if(ealist == nullptr)
      {
        return errc::not_enough_memory;
      }
      auto *p = (byte *) ealist;
      auto *i = (_FILE_GET_EA_INFORMATION *) p;
      for(auto name : names)
      {
        i = (_FILE_GET_EA_INFORMATION *) p;
        i->NextEntryOffset = (ULONG) ((sizeof(ULONG) + 1 + name.native_size() + 3) & ~3);
        i->EaNameLength = (UCHAR) name.native_size();
        visit(name,
              [&](auto sv)
              {
                memcpy(i->EaName, sv.data(), i->EaNameLength);
                i->EaName[i->EaNameLength] = 0;
              });
        p += i->NextEntryOffset;
      }
      i->NextEntryOffset = 0;
    }
    NTSTATUS ntstat = NtQueryEaFile(h.native_handle().h, &isb, tofill.data(), (ULONG) tofill.size(), false, ealist, ealistlen, nullptr, false);
    if(STATUS_PENDING == ntstat)
    {
      ntstat = ntwait(h.native_handle().h, isb, {});
    }
    win_extended_attributes_return_t ret;
    ret.tofillremaining = tofill.size();
    if(ntstat < 0)
    {
      if(ntstat == 0xC0000052 /*STATUS_NO_EAS_ON_FILE*/)
      {
        return ret;
      }
      return ntkernel_error(ntstat);
    }
    {
      auto *p = tofill.data();
      bool done = false;
      size_t count = 0;
      while(!done && p < tofill.data() + isb.Information)
      {
        auto *i = (_FILE_FULL_EA_INFORMATION *) p;
        if(i->EaNameLength > 0)
        {
          count++;
        }
        if(i->NextEntryOffset == 0)
        {
          done = true;
        }
        else
        {
          p += i->NextEntryOffset;
        }
      }
      const auto offset = (isb.Information + 7) & ~7;
      ret.tofillremaining = tofill.size() - offset;
      if(count * sizeof(std::pair<path_view_component, span<byte>>) > ret.tofillremaining)
      {
        return errc::no_buffer_space;
      }
      ret.filled = {(std::pair<path_view_component, span<byte>> *) (p + offset), count};
      ret.tofillremaining -= count * sizeof(std::pair<path_view_component, span<byte>>);
    }
    {
      auto *p = tofill.data();
      bool done = false;
      size_t count = 0;
      while(!done && p < tofill.data() + isb.Information)
      {
        auto *i = (_FILE_FULL_EA_INFORMATION *) p;
        if(i->EaNameLength > 0)
        {
          ret.filled[count] = {path_view_component(i->EaName, i->EaNameLength, path_view_component::zero_terminated),
                               {(byte *) i->EaName + i->EaNameLength + 1, i->EaValueLength}};
          count++;
        }
        if(i->NextEntryOffset == 0)
        {
          done = true;
        }
        else
        {
          p += i->NextEntryOffset;
        }
      }
    }
    return ret;
  }
}  // namespace detail

result<span<std::pair<path_view_component, span<byte>>>> fs_handle::win_list_extended_attributes(span<byte> tofill) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  OUTCOME_TRY(auto &&ret, detail::win_extended_attributes(tofill, h, {}));
  return ret.filled;
}

result<span<std::pair<path_view_component, span<byte>>>> fs_handle::win_get_extended_attributes(span<byte> tofill,
                                                                                                span<const path_view_component> names) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  OUTCOME_TRY(auto &&ret, detail::win_extended_attributes(tofill, h, names));
  return ret.filled;
}

result<void> fs_handle::win_set_extended_attributes(span<std::pair<const path_view_component, span<const byte>>> toset) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  auto &h = _get_handle();
  LLFIO_LOG_FUNCTION_CALL(&h);
  _FILE_FULL_EA_INFORMATION *ealist = nullptr;
  ULONG ealistlen = 0;
  for(auto item : toset)
  {
    if(item.first.native_size() > 255 || item.second.size() > 65535)
    {
      return errc::invalid_argument;
    }
    if(!visit(item.first, [](auto sv) { return sizeof(sv[0]) == 1; }))
    {
      return errc::invalid_argument;
    }
    ealistlen += (ULONG) ((sizeof(ULONG) + 4 + item.first.native_size() + 1 + item.second.size() + 3) & ~3);
  }
  if(ealistlen > 0)
  {
    ealist = (_FILE_FULL_EA_INFORMATION *) alloca(ealistlen);
    if(ealist == nullptr)
    {
      return errc::not_enough_memory;
    }
    auto *p = (byte *) ealist;
    auto *i = (_FILE_FULL_EA_INFORMATION *) p;
    for(auto item : toset)
    {
      i = (_FILE_FULL_EA_INFORMATION *) p;
      i->NextEntryOffset = (ULONG) ((sizeof(ULONG) + 4 + item.first.native_size() + 1 + item.second.size() + 3) & ~3);
      i->Flags = 0;
      i->EaNameLength = (UCHAR) item.first.native_size();
      visit(item.first,
            [&](auto sv)
            {
              memcpy(i->EaName, sv.data(), i->EaNameLength);
              i->EaName[i->EaNameLength] = 0;
            });
      i->EaValueLength = (USHORT) item.second.size();
      memcpy(i->EaName + i->EaNameLength + 1, item.second.data(), item.second.size());
      p += i->NextEntryOffset;
    }
    i->NextEntryOffset = 0;
  }
  IO_STATUS_BLOCK isb = make_iostatus();
  NTSTATUS ntstat = NtSetEaFile(h.native_handle().h, &isb, ealist, ealistlen);
  if(STATUS_PENDING == ntstat)
  {
    ntstat = ntwait(h.native_handle().h, isb, {});
  }
  if(ntstat < 0)
  {
    return ntkernel_error(ntstat);
  }
  return success();
}


/**************************************** to_win32_path() *******************************************/

LLFIO_HEADERS_ONLY_FUNC_SPEC result<filesystem::path> to_win32_path(const fs_handle &_h, win32_path_namespace mapping) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  HANDLE h = _h._get_handle().native_handle().h;
  LLFIO_LOG_FUNCTION_CALL(&h);
  // Most efficient, least memory copying method is direct fill of a wstring which is moved into filesystem::path
  filesystem::path::string_type buffer;
  buffer.resize(32769);
  auto *_buffer = const_cast<wchar_t *>(buffer.data());
  // Unlike h.current_path() which uses FILE_NAME_NORMALIZED, we shall use here FILE_NAME_OPENED
  DWORD flags = FILE_NAME_OPENED;
  switch(mapping)
  {
  case win32_path_namespace::device:
    flags |= VOLUME_NAME_NT;
    break;
  case win32_path_namespace::dos:
    flags |= VOLUME_NAME_DOS;
    break;
  case win32_path_namespace::any:          // fallthrough
  case win32_path_namespace::guid_volume:  // fallthrough
                                           // case win32_path_namespace::guid_all:
    flags |= VOLUME_NAME_GUID;
    break;
  }
  {
    // Before Vista, I had to do this by hand, now I have a nice simple API. Thank you Microsoft!
    DWORD len = GetFinalPathNameByHandleW(h, _buffer, (DWORD) buffer.size(), flags);  // NOLINT
    if(len == 0)
    {
      if(win32_path_namespace::any == mapping)
      {
        len = GetFinalPathNameByHandleW(h, _buffer, (DWORD) buffer.size(), FILE_NAME_OPENED | VOLUME_NAME_DOS);
        if(len == 0)
        {
          return win32_error();
        }
        mapping = win32_path_namespace::dos;
      }
      else
      {
        return win32_error();
      }
    }
    if(win32_path_namespace::any == mapping)
    {
      mapping = win32_path_namespace::guid_volume;
    }
    buffer.resize(len);
  }
  if(win32_path_namespace::guid_volume == mapping)
  {
    return filesystem::path(std::move(buffer));
  }
#if 0
  if(win32_path_namespace::guid_all == mapping)
  {
    FILE_OBJECTID_BUFFER fob;
    DWORD out;
    if(!DeviceIoControl(h, FSCTL_CREATE_OR_GET_OBJECT_ID, nullptr, 0, &fob, sizeof(fob), &out, nullptr))
    {
      return win32_error();
    }
    GUID *guid = (GUID *) &fob.ObjectId;
    /* Form is `\\?\Volume{9f9bd10e-9003-4da5-b146-70584e30854a}\{5a13b46c-44b9-40f3-9303-23cf7d918708}`
     */
    buffer.resize(87);
    swprintf_s(_buffer + 49, 64, L"{%08x-%04hx-%04hx-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid->Data1, guid->Data2, guid->Data3, guid->Data4[0], guid->Data4[1],
               guid->Data4[2], guid->Data4[3], guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
    return filesystem::path(std::move(buffer));
  }
#endif
  if(win32_path_namespace::device == mapping)
  {
    /* Paths of form \Device\... => \\.\ */
    if(0 == buffer.compare(0, 8, L"\\Device\\"))
    {
      buffer[1] = '\\';
      buffer[2] = '.';
      buffer[3] = '\\';
      memmove(&buffer[4], &buffer[8], (buffer.size() - 8) * sizeof(wchar_t));
      buffer.resize(buffer.size() - 4);
    }
    else
    {
      return errc::no_such_file_or_directory;
    }
  }
  OUTCOME_TRY(_h._fetch_inode());
  // Ensure the mapped path exists and is the same file as our source
  handle checkh(native_handle_type(native_handle_type::disposition::file | native_handle_type::disposition::_child_close_executed,
                                   CreateFileW(buffer.c_str(), SYNCHRONIZE | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                               nullptr, OPEN_ALWAYS, FILE_FLAG_BACKUP_SEMANTICS, nullptr)));
  if(INVALID_HANDLE_VALUE != checkh.native_handle().h)
  {
    stat_t scheck;
    OUTCOME_TRYV(scheck.fill(checkh, stat_t::want::dev | stat_t::want::ino));
    if(_h.st_dev() == scheck.st_dev && _h.st_ino() == scheck.st_ino)
    {
      if(win32_path_namespace::dos == mapping)
      {
        // Can we remove the \\?\ prefix safely from this DOS path?
        bool needsExtendedPrefix = (buffer.size() > 260);
        if(!needsExtendedPrefix)
        {
          // Are there any illegal Win32 characters in here?
          static constexpr char reserved_chars[] = "\"*/:<>?|";
          for(size_t n = 7; !needsExtendedPrefix && n < buffer.size(); n++)
          {
            if(buffer[n] >= 1 && buffer[n] <= 31)
            {
              needsExtendedPrefix = true;
              break;
            }
            for(size_t x = 0; x < sizeof(reserved_chars); x++)
            {
              if(buffer[n] == reserved_chars[x])
              {
                needsExtendedPrefix = true;
                break;
              }
            }
          }
        }
        if(!needsExtendedPrefix)
        {
          // Are any segments of the filename a reserved name?
          static
#if(_HAS_CXX17 || __cplusplus >= 201700) && (!defined(__GLIBCXX__) || __GLIBCXX__ > 20170519)  // libstdc++'s string_view is missing constexpr
          constexpr
#endif
          const wstring_view reserved_names[] = {L"\\CON\\",  L"\\PRN\\",  L"\\AUX\\",  L"\\NUL\\",  L"\\COM1\\", L"\\COM2\\", L"\\COM3\\", L"\\COM4\\",
                                                 L"\\COM5\\", L"\\COM6\\", L"\\COM7\\", L"\\COM8\\", L"\\COM9\\", L"\\LPT1\\", L"\\LPT2\\", L"\\LPT3\\",
                                                 L"\\LPT4\\", L"\\LPT5\\", L"\\LPT6\\", L"\\LPT7\\", L"\\LPT8\\", L"\\LPT9\\"};
          wstring_view _buffer_(buffer);
          for(auto name : reserved_names)
          {
            if(_buffer_.npos != _buffer_.find(name))
            {
              needsExtendedPrefix = true;
              break;
            }
            auto idx = _buffer_.find(name.substr(0, name.size() - 1));
            if(_buffer_.npos != idx && idx + name.size() - 1 == _buffer_.size())
            {
              needsExtendedPrefix = true;
              break;
            }
          }
        }
        if(!needsExtendedPrefix)
        {
          // This is safe to deprefix
          memmove(&buffer[0], &buffer[4], (buffer.size() - 4) * sizeof(wchar_t));
          buffer.resize(buffer.size() - 4);
        }
      }
      return filesystem::path(std::move(buffer));
    }
  }
  return errc::no_such_file_or_directory;
}

LLFIO_V2_NAMESPACE_END
