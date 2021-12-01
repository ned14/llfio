/* A filesystem algorithm which removes a directory tree
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: Mar 2020


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

#include "../../algorithm/reduce.hpp"

#ifdef _WIN32
#include "windows/import.hpp"
#else
#include "posix/import.hpp"
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace detail
  {
    inline result<void> remove(const directory_handle &dirh, path_view leafname, bool is_dir) noexcept
    {
#ifdef _WIN32
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      /* We have a custom implementation for Microsoft Windows, because internally to Windows
      file entry removal works by opening a new HANDLE to the file entry, setting its
      delete-on-close flag, and closing the HANDLE. As everybody knows, opening
      new HANDLEs is hideously slow on Windows, however it is less awful if you
      open a HANDLE with only DELETE privileges and nothing else. In this situation,
      Windows appears to have a less slow code path, resulting a noticeable
      performance improvement for really large directory tree removals.

      Before anyone asks why we don't use FILE_DISPOSITION_POSIX_SEMANTICS and are
      instead doing this "the old fashioned way", it's because opening handles with
      delete on close semantics from the beginning has much better performance.
      The NT kernel will delay actually removing the file entries until some point
      later, which is normally problematic, hence FILE_DISPOSITION_POSIX_SEMANTICS
      existing at all. However if you are removing all the entries in a directory
      at once, the delay is beneficial, because NTFS can bulk remove all the entries
      in a single operation.
      */
      const DWORD access = SYNCHRONIZE | DELETE;
      const DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
      const DWORD deletefile_ntflags =
      0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x00001000 /*FILE_DELETE_ON_CLOSE*/ | 0x040 /*FILE_NON_DIRECTORY_FILE*/;
      const DWORD deletedir_ntflags =
      0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x00001000 /*FILE_DELETE_ON_CLOSE*/ | 0x01 /*FILE_DIRECTORY_FILE*/;
      IO_STATUS_BLOCK isb = make_iostatus();
      path_view::zero_terminated_rendered_path<> zpath(leafname);
      UNICODE_STRING _path{};
      _path.Buffer = const_cast<wchar_t *>(zpath.data());
      _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);

      OBJECT_ATTRIBUTES oa{};
      memset(&oa, 0, sizeof(oa));
      oa.Length = sizeof(OBJECT_ATTRIBUTES);
      oa.ObjectName = &_path;
      oa.RootDirectory = dirh.native_handle().h;
      oa.Attributes = 0;  // 0x40 /*OBJ_CASE_INSENSITIVE*/;
      // if(!!(flags & file_flags::int_opening_link))
      //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;

      HANDLE h = INVALID_HANDLE_VALUE;
      NTSTATUS ntstat = 0;
      if(!is_dir)
      {
        ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, deletefile_ntflags);
        if(STATUS_PENDING == ntstat)
        {
          ntstat = ntwait(h, isb, deadline());
        }
        if(0xC0000056 /*STATUS_DELETE_PENDING*/ == ntstat)
        {
          // Can skip this fellow
          return success();
        }
      }
      else
      {
        isb = make_iostatus();
        ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, deletedir_ntflags);
        if(STATUS_PENDING == ntstat)
        {
          ntstat = ntwait(h, isb, deadline());
        }
        if(0xC0000056 /*STATUS_DELETE_PENDING*/ == ntstat)
        {
          // Can skip this fellow
          return success();
        }
        if(ntstat >= 0)
        {
          // FILE_DELETE_ON_CLOSE open flag only works on non-directories.
          FILE_DISPOSITION_INFORMATION fdi{};
          memset(&fdi, 0, sizeof(fdi));
          fdi._DeleteFile = 1u;
          isb = make_iostatus();
          ntstat = NtSetInformationFile(h, &isb, &fdi, sizeof(fdi), FileDispositionInformation);
          if(STATUS_PENDING == ntstat)
          {
            ntstat = ntwait(h, isb, deadline());
          }
          if(ntstat < 0)
          {
            assert(ntstat == 0xC0000101 /*STATUS_DIRECTORY_NOT_EMPTY*/);
            // If cannot set the delete-on-close flag, retry this later.
            NtClose(h);
            h = INVALID_HANDLE_VALUE;
          }
          else
          {
            // std::cout << "Removed quickly " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
          }
        }
      }
      if(h != INVALID_HANDLE_VALUE)
      {
        NtClose(h);  // removes the entry
        // std::cout << "Removed " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
        return success();
      }
      return ntkernel_error(ntstat);
#else
      path_view::zero_terminated_rendered_path<> zpath(leafname);
      errno = 0;
      if(is_dir || -1 == ::unlinkat(dirh.native_handle().fd, zpath.data(), 0))
      {
        if(ENOENT == errno)
        {
          // Somebody else removed it
          return success();
        }
        if(is_dir || EISDIR == errno)
        {
          // Try to remove it as a directory, if it's empty we've saved a recurse
          if(-1 != ::unlinkat(dirh.native_handle().fd, zpath.data(), AT_REMOVEDIR))
          {
            // std::cout << "Removed quickly " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
            return success();
          }
        }
        return posix_error();
      }
      return success();
#endif
    }
    inline result<void> rename(const directory_handle &dirh, path_view leafname, bool is_dir, const directory_handle &topdirh) noexcept
    {
#ifdef _WIN32
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
      const DWORD access = SYNCHRONIZE | DELETE;
      const DWORD fileshare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
      const DWORD renamefile_ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x040 /*FILE_NON_DIRECTORY_FILE*/;
      const DWORD renamedir_ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x01 /*FILE_DIRECTORY_FILE*/;
      IO_STATUS_BLOCK isb = make_iostatus();
      path_view::zero_terminated_rendered_path<> zpath(leafname);
      UNICODE_STRING _path{};
      _path.Buffer = const_cast<wchar_t *>(zpath.data());
      _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.size() * sizeof(wchar_t))) + sizeof(wchar_t);

      OBJECT_ATTRIBUTES oa{};
      memset(&oa, 0, sizeof(oa));
      oa.Length = sizeof(OBJECT_ATTRIBUTES);
      oa.ObjectName = &_path;
      oa.RootDirectory = dirh.native_handle().h;
      oa.Attributes = 0;  // 0x40 /*OBJ_CASE_INSENSITIVE*/;
      // if(!!(flags & file_flags::int_opening_link))
      //  oa.Attributes|=0x100/*OBJ_OPENLINK*/;

      HANDLE h = INVALID_HANDLE_VALUE;
      NTSTATUS ntstat = 0;
      // Try to rename the entry into topdirh
      if(dirh.unique_id() != topdirh.unique_id())
      {
        // We need to try opening the file again, but this time without FILE_DELETE_ON_CLOSE
        if(!is_dir)
        {
          ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, renamefile_ntflags);
          if(STATUS_PENDING == ntstat)
          {
            ntstat = ntwait(h, isb, deadline());
          }
          if(0xC0000056 /*STATUS_DELETE_PENDING*/ == ntstat)
          {
            // Can skip this fellow
            return success();
          }
        }
        else
        {
          ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, renamedir_ntflags);
          if(STATUS_PENDING == ntstat)
          {
            ntstat = ntwait(h, isb, deadline());
          }
          if(0xC0000056 /*STATUS_DELETE_PENDING*/ == ntstat)
          {
            // Can skip this fellow
            return success();
          }
        }
        if(h != INVALID_HANDLE_VALUE)
        {
          auto randomname = utils::random_string(32);
          alignas(8) char buffer[sizeof(FILE_RENAME_INFORMATION) + 96];
          auto *fni = reinterpret_cast<FILE_RENAME_INFORMATION *>(buffer);
          fni->Flags = 0;
          fni->RootDirectory = topdirh.native_handle().h;
          fni->FileNameLength = 64;
          for(size_t n = 0; n < 32; n++)
          {
            fni->FileName[n] = randomname[n];
          }
          ntstat = NtSetInformationFile(h, &isb, fni, sizeof(FILE_RENAME_INFORMATION) + fni->FileNameLength, FileRenameInformation);
          NtClose(h);
          if(ntstat >= 0)
          {
            // std::cout << "Renamed into base " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
            return success();
          }
          // std::cout << "Failed to remove " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
        }
        return ntkernel_error(ntstat);
      }
      return success();
#else
      (void) is_dir;
      path_view::zero_terminated_rendered_path<> zpath(leafname);
      if(dirh.unique_id() != topdirh.unique_id())
      {
        // Try renaming it into topdirh
        auto randomname = utils::random_string(32);
        if(-1 != ::renameat(topdirh.native_handle().fd, randomname.c_str(), dirh.native_handle().fd, zpath.data()))
        {
          return success();
        }
        return posix_error();
      }
      return success();
#endif
    }
    struct reduction_state
    {
      const directory_handle &topdirh;
      reduce_visitor *visitor{nullptr};
      std::atomic<size_t> items_removed{0}, directory_open_failed{0}, failed_to_remove{0}, failed_to_rename{0};

      reduction_state(const directory_handle &_topdirh, reduce_visitor *_visitor)
          : topdirh(_topdirh)
          , visitor(_visitor)
      {
      }
    };
  }  // namespace detail

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<directory_handle>
  reduce_visitor::directory_open_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh, path_view leaf, size_t depth) noexcept
  {
    (void) error;
    (void) depth;
    // Opening it for enumeration failed, but maybe it might rename?
    auto *state = (detail::reduction_state *) data;
    auto r = detail::rename(dirh, leaf, true, state->topdirh);
    if(!r)
    {
      state->directory_open_failed.fetch_add(1, std::memory_order_relaxed);
    }
    return success();  // ignore failure to enter
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<void> reduce_visitor::post_enumeration(void *data, const directory_handle &dirh,
                                                                                directory_handle::buffers_type &contents, size_t depth) noexcept
  {
    auto *state = (detail::reduction_state *) data;
    bool removed_everything = true;
    for(auto &entry : contents)
    {
      switch(entry.stat.st_type)
      {
      case filesystem::file_type::directory:
      {
        log_level_guard g(log_level::fatal);
        auto r = detail::remove(dirh, entry.leafname, true);
        if(r)
        {
          state->items_removed.fetch_add(1, std::memory_order_relaxed);
          entry.stat = stat_t(nullptr);  // prevent traversal
        }
        else
        {
          state->failed_to_remove.fetch_add(1, std::memory_order_relaxed);
          removed_everything = false;
        }
        break;
      }
      default:
      {
        auto r = detail::remove(dirh, entry.leafname, false);
        if(!r)
        {
          OUTCOME_TRY(auto &&success, state->visitor->unlink_failed(data, std::move(r).error(), dirh, entry, depth));
          if(success)
          {
            state->items_removed.fetch_add(1, std::memory_order_relaxed);
          }
          else
          {
            state->failed_to_remove.fetch_add(1, std::memory_order_relaxed);
            removed_everything = false;
          }
        }
        else
        {
          state->items_removed.fetch_add(1, std::memory_order_relaxed);
        }
        break;
      }
      }
    }
    // Tail call optimisation
    if(removed_everything)
    {
      // TODO
    }
    return success();
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<bool> reduce_visitor::unlink_failed(void *data, result<void>::error_type &&error, const directory_handle &dirh,
                                                                             directory_entry &entry, size_t depth) noexcept
  {
    (void) error;
    auto *state = (detail::reduction_state *) data;
    // Try to rename
    auto r = detail::rename(dirh, entry.leafname, false, state->topdirh);
    if(!r)
    {
      OUTCOME_TRY(auto &&success, state->visitor->rename_failed(data, std::move(r).error(), dirh, entry, depth));
      if(!success)
      {
        state->failed_to_rename.fetch_add(1, std::memory_order_relaxed);
      }
    }
    return false;
  }

  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_t> reduce(directory_handle &&topdirh, reduce_visitor *visitor, size_t threads, bool force_slow_path) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(&topdirh);
    reduce_visitor default_visitor;
    if(visitor == nullptr)
    {
      visitor = &default_visitor;
    }
    {
      auto dirhparent = topdirh.parent_path_handle();
      if(dirhparent)
      {
        for(;;)
        {
          auto randomname = utils::random_string(32);
          auto ret = topdirh.relink(dirhparent.value(), randomname);
          if(ret)
          {
            break;
          }
          if(!ret && ret.error() != errc::file_exists)
          {
            break;
          }
        }
      }
    }
    size_t round = 0;
    detail::reduction_state state(topdirh, visitor);
    OUTCOME_TRY(traverse(topdirh, visitor, threads, &state, force_slow_path));
    auto not_removed = state.directory_open_failed.load(std::memory_order_relaxed) + state.failed_to_remove.load(std::memory_order_relaxed) +
                       state.failed_to_rename.load(std::memory_order_relaxed);
    OUTCOME_TRY(visitor->reduction_round(&state, round++, state.items_removed.load(std::memory_order_relaxed), not_removed));
    while(not_removed > 0)
    {
      state.directory_open_failed.store(0, std::memory_order_relaxed);
      state.failed_to_remove.store(0, std::memory_order_relaxed);
      state.failed_to_rename.store(0, std::memory_order_relaxed);
      OUTCOME_TRY(traverse(topdirh, visitor, (round > 16) ? 1 : threads, &state, force_slow_path));
      not_removed = state.directory_open_failed.load(std::memory_order_relaxed) + state.failed_to_remove.load(std::memory_order_relaxed) +
                    state.failed_to_rename.load(std::memory_order_relaxed);
      OUTCOME_TRY(visitor->reduction_round(&state, round++, state.items_removed.load(std::memory_order_relaxed), not_removed));
    }
    OUTCOME_TRY(topdirh.unlink());
    state.items_removed.fetch_add(1, std::memory_order_relaxed);
    // If we successfully unlinked it, move the directory in here, and close it.
    auto _topdirh(std::move(topdirh));
    OUTCOME_TRY(_topdirh.close());
    return state.items_removed;
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
