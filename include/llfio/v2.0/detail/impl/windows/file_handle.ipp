/* A handle to a file
(C) 2015-2021 Niall Douglas <http://www.nedproductions.biz/> (16 commits)
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

#include "../../../statfs.hpp"

#include <mutex>
#include <vector>

LLFIO_V2_NAMESPACE_BEGIN

result<file_handle> file_handle::file(const path_handle &base, file_handle::path_view_type path, file_handle::mode _mode, file_handle::creation _creation,
                                      file_handle::caching _caching, file_handle::flag flags) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  result<file_handle> ret(in_place_type<file_handle>, native_handle_type(), _caching, flags, nullptr);
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  OUTCOME_TRY(auto &&access, access_mask_from_handle_mode(nativeh, _mode, flags));
  OUTCOME_TRY(auto &&attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
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
    case creation::truncate_existing:
      creatdisp = 0x00000004 /*FILE_OVERWRITE*/;
      break;
    case creation::always_new:
      creatdisp = 0x00000000 /*FILE_SUPERSEDE*/;
      break;
    }

    attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
    OUTCOME_TRY(auto &&ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
    ntflags |= 0x040 /*FILE_NON_DIRECTORY_FILE*/;  // do not open a directory
    IO_STATUS_BLOCK isb = make_iostatus();

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

    OBJECT_ATTRIBUTES oa{};
    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &_path;
    oa.RootDirectory = base.is_valid() ? base.native_handle().h : nullptr;
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
    case creation::truncate_existing:
    case creation::always_new:
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
    case creation::truncate_existing:
      creation = TRUNCATE_EXISTING;
      break;
    case creation::always_new:
      creation = CREATE_ALWAYS;
      break;
    }
    path_view::zero_terminated_rendered_path<> zpath(path);
    if(INVALID_HANDLE_VALUE == (nativeh.h = CreateFileW_(zpath.data(), access, fileshare, nullptr, creation, attribs, nullptr)))  // NOLINT
    {
      DWORD errcode = GetLastError();
      // assert(false);
      return win32_error(errcode);
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
      need_to_set_sparse = (GetLastError() != ERROR_ALREADY_EXISTS);  // new inode created
      break;
    case creation::truncate_existing:
    case creation::always_new:
      need_to_set_sparse = true;
      break;
    }
  }
  if(need_to_set_sparse && !(flags & flag::win_disable_sparse_file_creation))
  {
    DWORD bytesout = 0;
    FILE_SET_SPARSE_BUFFER fssb;
    memset(&fssb, 0, sizeof(fssb));
    fssb.SetSparse = 1u;
    if(DeviceIoControl(nativeh.h, FSCTL_SET_SPARSE, &fssb, sizeof(fssb), nullptr, 0, &bytesout, nullptr) == 0)
    {
#if 1 || LLFIO_LOGGING_LEVEL >= 3
      DWORD errcode = GetLastError();
      LLFIO_LOG_WARN(&ret, "Failed to set file to sparse");
      result<void> r = win32_error(errcode);
      (void) r;  // throw away
#endif
    }
  }
  if(_creation == creation::truncate_existing && ret.value().are_safety_barriers_issued())
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
  flags |= flag::disable_safety_unlinks | flag::win_disable_unlink_emulation;
  result<file_handle> ret(in_place_type<file_handle>, native_handle_type(), _caching, flags, nullptr);
  native_handle_type &nativeh = ret.value()._v;
  LLFIO_LOG_FUNCTION_CALL(&ret);
  nativeh.behaviour |= native_handle_type::disposition::file;
  DWORD fileshare = /* no read nor write access for others */ FILE_SHARE_DELETE;
  OUTCOME_TRY(auto &&access, access_mask_from_handle_mode(nativeh, _mode, flags));
  OUTCOME_TRY(auto &&attribs, attributes_from_handle_caching_and_flags(nativeh, _caching, flags));
  DWORD creatdisp = 0x00000002 /*FILE_CREATE*/;

  attribs &= 0x00ffffff;  // the real attributes only, not the win32 flags
  OUTCOME_TRY(auto &&ntflags, ntflags_from_handle_caching_and_flags(nativeh, _caching, flags));
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
        return ntkernel_error(ntstat);
      }
    }
    // std::cerr << random << std::endl;
    if(nativeh.h != nullptr)
    {
      if(!(flags & flag::win_disable_sparse_file_creation))
      {
        DWORD bytesout = 0;
        FILE_SET_SPARSE_BUFFER fssb;
        memset(&fssb, 0, sizeof(fssb));
        fssb.SetSparse = 1u;
        if(DeviceIoControl(nativeh.h, FSCTL_SET_SPARSE, &fssb, sizeof(fssb), nullptr, 0, &bytesout, nullptr) == 0)
        {
#if LLFIO_LOGGING_LEVEL >= 3
          DWORD errcode = GetLastError();
          LLFIO_LOG_WARN(&ret, "Failed to set file to sparse");
          result<void> r = win32_error(errcode);
          (void) r;  // throw away
#endif
        }
      }
      HANDLE duph;
      {
        // It is entirely undocumented that this is how you clone a file handle with new privs
        memset(&_path, 0, sizeof(_path));
        oa.ObjectName = &_path;
        oa.RootDirectory = nativeh.h;
        IO_STATUS_BLOCK isb = make_iostatus();
        NTSTATUS ntstat =
        NtOpenFile(&duph, SYNCHRONIZE | DELETE, &oa, &isb, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/);
        if(ntstat < 0)
        {
          return ntkernel_error(ntstat);
        }
      }
      auto unduph = make_scope_exit([&duph]() noexcept { CloseHandle(duph); });
      (void) unduph;
      bool failed = true;
      // Immediately delete, try by POSIX delete first
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
        NTSTATUS ntstat = NtSetInformationFile(duph, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
        if(ntstat >= 0)
        {
          // No need to delete it again on close
          ret.value()._flags &= ~flag::unlink_on_first_close;
        }
      }
    }
    return ret;
  }
}

result<file_handle> file_handle::reopen(mode mode_, caching caching_, deadline /*unused*/) const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  result<file_handle> ret(in_place_type<file_handle>, native_handle_type(), caching_, _flags, nullptr);
  ret.value()._ctx = _ctx;
  OUTCOME_TRY(do_clone_handle(ret.value()._v, _v, mode_, caching_, _flags));
  return ret;
}

result<file_handle::extent_type> file_handle::maximum_extent() const noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
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
    return win32_error();
  }
  return fsi.EndOfFile.QuadPart;
}

result<file_handle::extent_type> file_handle::truncate(file_handle::extent_type newsize) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  FILE_END_OF_FILE_INFO feofi{};
  feofi.EndOfFile.QuadPart = newsize;
  if(SetFileInformationByHandle(_v.h, FileEndOfFileInfo, &feofi, sizeof(feofi)) == 0)
  {
    return win32_error();
  }
  if(are_safety_barriers_issued())
  {
    FlushFileBuffers(_v.h);
  }
  return newsize;
}

result<std::vector<file_handle::extent_pair>> file_handle::extents() const noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  try
  {
    static_assert(sizeof(file_handle::extent_pair) == sizeof(FILE_ALLOCATED_RANGE_BUFFER),
                  "FILE_ALLOCATED_RANGE_BUFFER is not equivalent to pair<extent_type, extent_type>!");
    std::vector<file_handle::extent_pair> ret;
#ifdef NDEBUG
    ret.resize(64);
#else
    ret.resize(1);
#endif
    FILE_ALLOCATED_RANGE_BUFFER farb{};
    farb.FileOffset.QuadPart = 0;
    farb.Length.QuadPart =
    (static_cast<extent_type>(1) << 63) - 1;  // Microsoft claims this is 1<<64-1024 for NTFS, but I get bad parameter error with anything higher than 1<<63-1.
    DWORD bytesout = 0;
    OVERLAPPED ol{};
    memset(&ol, 0, sizeof(ol));
    ol.Internal = static_cast<ULONG_PTR>(-1);
    while(DeviceIoControl(_v.h, FSCTL_QUERY_ALLOCATED_RANGES, &farb, sizeof(farb), ret.data(),
                          static_cast<DWORD>(ret.size() * sizeof(FILE_ALLOCATED_RANGE_BUFFER)), &bytesout, &ol) == 0)
    {
      if(ERROR_INSUFFICIENT_BUFFER == GetLastError() || ERROR_MORE_DATA == GetLastError())
      {
        ret.resize(ret.size() * 2);
        continue;
      }
      if(ERROR_SUCCESS != GetLastError())
      {
        return win32_error();
      }
    }
    return ret;
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
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
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
    LLFIO_DEADLINE_TO_SLEEP_INIT(d);
    const size_type blocksize = utils::file_buffer_default_size();
    byte *buffer = nullptr;
    auto unbufferh = make_scope_exit([&]() noexcept {
      if(buffer != nullptr)
        utils::page_allocator<byte>().deallocate(buffer, blocksize);
    });
    (void) unbufferh;
    extent_pair ret(extent.offset, 0);
    if(!dest_.is_regular())
    {
      // TODO: Use TransmitFile() here when we implement socket_handle.
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
    const auto page_size = utils::page_size();
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
    for(auto offset = extent.offset; offset < extent.offset + extent.length;)
    {
      FILE_ALLOCATED_RANGE_BUFFER farb_query{}, farb_allocated[8];
      farb_query.FileOffset.QuadPart = offset;
      farb_query.Length.QuadPart = extent.offset + extent.length - offset;
      DWORD bytesout = 0;
      OVERLAPPED ol{};
      memset(&ol, 0, sizeof(ol));
      ol.Internal = static_cast<ULONG_PTR>(-1);
      if(DeviceIoControl(_v.h, FSCTL_QUERY_ALLOCATED_RANGES, &farb_query, sizeof(farb_query), &farb_allocated, sizeof(farb_allocated), &bytesout, &ol) == 0)
      {
        if(ERROR_MORE_DATA != GetLastError())
        {
          return win32_error();
        }
      }
      if(bytesout == 0)
      {
        break;
      }
      for(size_t n = 0; n < bytesout / sizeof(farb_allocated[0]); n++)
      {
        if(!todo.empty())
        {
          auto endoflastregion = todo.back().src.offset + todo.back().src.length;
          if(endoflastregion != (extent_type) farb_allocated[n].FileOffset.QuadPart)
          {
            // Insert a delete region
            todo.push_back(workitem{extent_pair(endoflastregion, farb_allocated[n].FileOffset.QuadPart - endoflastregion), workitem::delete_extents});
          }
        }
        todo.push_back(workitem{extent_pair(farb_allocated[n].FileOffset.QuadPart, farb_allocated[n].Length.QuadPart), workitem::clone_extents});
        offset = farb_allocated[n].FileOffset.QuadPart + farb_allocated[n].Length.QuadPart;
      }
    }
    if(!todo.empty())
    {
      // The list of extents will spill outside initially requested range. Fill it in.
      if(todo.front().src.offset < extent.offset)
      {
        auto diff = (extent.offset - todo.front().src.offset + page_size - 1) & ~(page_size - 1);
        todo.front().src.length -= diff;
        todo.front().src.offset += diff;
        if(todo.front().src.offset != extent.offset)
        {
          assert(todo.front().src.offset > extent.offset);
          todo.insert(todo.begin(), workitem{{extent.offset, todo.front().src.offset - extent.offset},
                                             (todo.front().op == workitem::clone_extents) ? workitem::copy_bytes : workitem::zero_bytes});
        }
      }
      else if(todo.front().src.offset > extent.offset)
      {
        todo.insert(todo.begin(), workitem{{extent.offset, todo.front().src.offset - extent.offset}, workitem::delete_extents});
      }
      if(todo.back().src.offset + todo.back().src.length > extent.offset + extent.length)
      {
        auto todoend = todo.back().src.offset + todo.back().src.length;
        auto extentend = extent.offset + extent.length;
        auto diff = (todoend + page_size - 1 - extentend) & ~(page_size - 1);
        todo.back().src.length -= diff;
        if(todoend != extentend)
        {
          assert(todoend < extentend);
          todo.push_back(workitem{{todoend, extentend - todoend}, (todo.back().op == workitem::clone_extents) ? workitem::copy_bytes : workitem::zero_bytes});
        }
      }
      else if(todo.back().src.offset + todo.back().src.length < extent.offset + extent.length)
      {
        todo.push_back(
        workitem{{todo.back().src.offset + todo.back().src.length, extent.offset + extent.length - (todo.back().src.offset + todo.back().src.length)},
                 workitem::delete_extents});
      }
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
    for(const workitem &item : todo)
    {
      for(extent_type thisoffset = 0; thisoffset < item.src.length; thisoffset += blocksize)
      {
        bool done = false;
        const auto thisblock = std::min((extent_type) blocksize, item.src.length - thisoffset);
        if(duplicate_extents && item.op == workitem::clone_extents)
        {
          typedef struct _DUPLICATE_EXTENTS_DATA
          {
            HANDLE FileHandle;
            LARGE_INTEGER SourceFileOffset;
            LARGE_INTEGER TargetFileOffset;
            LARGE_INTEGER ByteCount;
          } DUPLICATE_EXTENTS_DATA, *PDUPLICATE_EXTENTS_DATA;
          DUPLICATE_EXTENTS_DATA ded;
          memset(&ded, 0, sizeof(ded));
          ded.FileHandle = _v.h;
          ded.SourceFileOffset.QuadPart = item.src.offset + thisoffset;
          ded.TargetFileOffset.QuadPart = item.src.offset + thisoffset + destoffsetdiff;
          ded.ByteCount.QuadPart = thisblock;
          DWORD bytesout = 0;
          OVERLAPPED ol{};
          memset(&ol, 0, sizeof(ol));
          ol.Internal = static_cast<ULONG_PTR>(-1);
          if(DeviceIoControl(dest.native_handle().h, CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 209, METHOD_BUFFERED, FILE_WRITE_DATA) /*FSCTL_DUPLICATE_EXTENTS*/, &ded,
                             sizeof(ded), nullptr, 0, &bytesout, &ol) == 0)
          {
            DWORD errcode = GetLastError();
            if(ERROR_IO_PENDING == errcode)
            {
              NTSTATUS ntstat = ntwait(dest.native_handle().h, ol, deadline());
              if(ntstat != 0)
              {
                return ntkernel_error(ntstat);
              }
            }
            if(ERROR_SUCCESS != errcode && !emulate_if_unsupported)
            {
              return win32_error(errcode);
            }
            duplicate_extents = false;  // emulate using copy of bytes
          }
          else
          {
            done = true;
          }
        }
        if(!done && (item.op == workitem::copy_bytes || (!duplicate_extents && item.op == workitem::clone_extents)))
        {
          if(buffer == nullptr)
          {
            buffer = utils::page_allocator<byte>().allocate(blocksize);
          }
          deadline nd;
          buffer_type b(buffer, utils::round_up_to_page_size((size_t) thisblock, 4096) /* to allow aligned i/o files */);
          LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
          OUTCOME_TRY(auto &&readed, read({{&b, 1}, item.src.offset + thisoffset}, nd));
          buffer_dirty = true;
          if(readed.front().size() < thisblock)
          {
            return errc::resource_unavailable_try_again;  // something is wrong
          }
          readed.front() = {readed.front().data(), (size_t) thisblock};
          LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
          const_buffer_type cb(readed.front().data(), (size_t) thisblock);
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
          FILE_ZERO_DATA_INFORMATION fzdi{};
          fzdi.FileOffset.QuadPart = item.src.offset + thisoffset + destoffsetdiff;
          fzdi.BeyondFinalZero.QuadPart = item.src.offset + thisoffset + destoffsetdiff + thisblock;
          DWORD bytesout = 0;
          OVERLAPPED ol{};
          memset(&ol, 0, sizeof(ol));
          ol.Internal = static_cast<ULONG_PTR>(-1);
          if(DeviceIoControl(dest.native_handle().h, FSCTL_SET_ZERO_DATA, &fzdi, sizeof(fzdi), nullptr, 0, &bytesout, &ol) == 0)
          {
            if(ERROR_IO_PENDING == GetLastError())
            {
              NTSTATUS ntstat = ntwait(dest.native_handle().h, ol, deadline());
              if(ntstat != 0)
              {
                return ntkernel_error(ntstat);
              }
            }
            if(ERROR_SUCCESS != GetLastError() && !emulate_if_unsupported)
            {
              return win32_error();
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
          const_buffer_type cb(buffer, (size_type) thisblock);
          if(buffer_dirty)
          {
            memset(buffer, 0, (size_type) thisblock);
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
        // assert(done);
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

result<file_handle::extent_type> file_handle::zero(file_handle::extent_pair extent, deadline /*unused*/) noexcept
{
  windows_nt_kernel::init();
  using namespace windows_nt_kernel;
  LLFIO_LOG_FUNCTION_CALL(this);
  if(extent.offset + extent.length < extent.offset)
  {
    return errc::value_too_large;
  }
  FILE_ZERO_DATA_INFORMATION fzdi{};
  fzdi.FileOffset.QuadPart = extent.offset;
  fzdi.BeyondFinalZero.QuadPart = extent.offset + extent.length;
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
        return ntkernel_error(ntstat);
      }
    }
    if(ERROR_SUCCESS != GetLastError())
    {
      return win32_error();
    }
  }
  return success();
}


/******************************************* statfs_t ************************************************/

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<std::pair<uint32_t, float>> statfs_t::_fill_ios(const handle & /*unused*/, const std::string &mntfromname) noexcept
{
  try
  {
    alignas(8) wchar_t buffer[32769];
    // Firstly open a handle to the volume
    OUTCOME_TRY(auto &&volumeh, file_handle::file({}, mntfromname, handle::mode::none, handle::creation::open_existing, handle::caching::only_metadata));
    // Now ask the volume what physical disks it spans
    auto *vde = reinterpret_cast<VOLUME_DISK_EXTENTS *>(buffer);
    OVERLAPPED ol{};
    memset(&ol, 0, sizeof(ol));
    ol.Internal = static_cast<ULONG_PTR>(-1);
    if(DeviceIoControl(volumeh.native_handle().h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, vde, sizeof(buffer), nullptr, &ol) == 0)
    {
      if(ERROR_IO_PENDING == GetLastError())
      {
        NTSTATUS ntstat = ntwait(volumeh.native_handle().h, ol, deadline());
        if(ntstat != 0)
        {
          return ntkernel_error(ntstat);
        }
      }
      if(ERROR_SUCCESS != GetLastError())
      {
        return win32_error();
      }
    }
    static struct last_reading_t
    {
      struct item
      {
        int64_t ReadTime{0}, WriteTime{0}, IdleTime{0};
      };
      std::mutex lock;
      std::vector<item> items;
    } last_reading;

    uint32_t iosinprogress = 0;
    float iosbusytime = 0;
    DWORD disk_extents = vde->NumberOfDiskExtents;
    for(DWORD disk_extent = 0; disk_extent < disk_extents; disk_extent++)
    {
      alignas(8) wchar_t physicaldrivename[32] = L"\\\\.\\PhysicalDrive", *e = physicaldrivename + 17;
      const auto DiskNumber = vde->Extents[disk_extent].DiskNumber;
      if(DiskNumber >= 100)
      {
        *e++ = '0' + ((DiskNumber / 100) % 10);
      }
      if(DiskNumber >= 10)
      {
        *e++ = '0' + ((DiskNumber / 10) % 10);
      }
      *e++ = '0' + (DiskNumber % 10);
      *e = 0;
      OUTCOME_TRY(auto &&diskh, file_handle::file({}, path_view(physicaldrivename, e - physicaldrivename, path_view::zero_terminated), handle::mode::none,
                                                handle::creation::open_existing, handle::caching::only_metadata));
      ol.Internal = static_cast<ULONG_PTR>(-1);
      auto *dp = reinterpret_cast<DISK_PERFORMANCE *>(buffer);
      if(DeviceIoControl(diskh.native_handle().h, IOCTL_DISK_PERFORMANCE, nullptr, 0, dp, sizeof(buffer), nullptr, &ol) == 0)
      {
        if(ERROR_IO_PENDING == GetLastError())
        {
          NTSTATUS ntstat = ntwait(diskh.native_handle().h, ol, deadline());
          if(ntstat != 0)
          {
            return ntkernel_error(ntstat);
          }
        }
        if(ERROR_SUCCESS != GetLastError())
        {
          return win32_error();
        }
      }
      //printf("%llu,%llu,%llu\n", dp->ReadTime.QuadPart, dp->WriteTime.QuadPart, dp->IdleTime.QuadPart);
      iosinprogress += dp->QueueDepth;
      std::lock_guard<std::mutex> g(last_reading.lock);
      if(last_reading.items.size() < DiskNumber + 1)
      {
        last_reading.items.resize(DiskNumber + 1);
      }
      else
      {
        uint64_t rd = (uint64_t) dp->ReadTime.QuadPart - (uint64_t) last_reading.items[DiskNumber].ReadTime;
        uint64_t wd = (uint64_t) dp->WriteTime.QuadPart - (uint64_t) last_reading.items[DiskNumber].WriteTime;
        uint64_t id = (uint64_t) dp->IdleTime.QuadPart - (uint64_t) last_reading.items[DiskNumber].IdleTime;
        iosbusytime += 1 - (float) ((double) id / (rd + wd + id));
      }
      last_reading.items[DiskNumber].ReadTime = dp->ReadTime.QuadPart;
      last_reading.items[DiskNumber].WriteTime = dp->WriteTime.QuadPart;
      last_reading.items[DiskNumber].IdleTime = dp->IdleTime.QuadPart;
    }
    iosinprogress /= disk_extents;
    iosbusytime /= disk_extents;
    return {iosinprogress, std::min(iosbusytime, 1.0f)};
  }
  catch(...)
  {
    return error_from_exception();
  }
}

LLFIO_V2_NAMESPACE_END
