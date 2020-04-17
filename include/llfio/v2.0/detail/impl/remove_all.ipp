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

#include "../../algorithm/remove_all.hpp"
#ifdef _WIN32
#include "windows/import.hpp"
#else
#include "posix/import.hpp"
#endif

#include <atomic>
#include <condition_variable>
#include <deque>
//#include <iostream>
#include <forward_list>
#include <mutex>
#include <thread>
#include <vector>

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace detail
  {
    LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> remove_all(directory_handle &&topdirh, LLFIO_V2_NAMESPACE::detail::function_ptr<result<void>(remove_all_callback_reason reason, remove_all_callback_arg arg1, remove_all_callback_arg arg2)> _callback, size_t threads) noexcept
    {
#ifdef _WIN32
      windows_nt_kernel::init();
      using namespace windows_nt_kernel;
#endif
      try
      {
        LLFIO_LOG_FUNCTION_CALL(nullptr);
        log_level_guard logg(log_level::fatal);
        auto default_callback = [](remove_all_callback_reason reason, remove_all_callback_arg /*unused*/, remove_all_callback_arg /*unused*/) -> result<void> {
          // Ignore any unremoveable during directory enumeration, we only care about those
          // which occur after the tree has been removed.
          static thread_local struct state_t
          {
            bool inside_enumeration{false};
            std::chrono::steady_clock::time_point timer;
          } state;
          if(reason == remove_all_callback_reason::begin_enumeration)
          {
            state.inside_enumeration = true;
            return success();
          }
          if(reason == remove_all_callback_reason::end_enumeration)
          {
            state.inside_enumeration = false;
            state.timer = {};
            return success();
          }
          if(!state.inside_enumeration)
          {
            if(reason == remove_all_callback_reason::unremoveable)
            {
              if(state.timer == std::chrono::steady_clock::time_point())
              {
                state.timer = std::chrono::steady_clock::now();
              }
              else if(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - state.timer).count() >= 10)
              {
                return errc::timed_out;
              }
            }
          }
          return success();
        };
        auto callback = [&](remove_all_callback_reason reason, remove_all_callback_arg arg1, remove_all_callback_arg arg2) -> result<void> {
          if(_callback)
          {
            return _callback(reason, arg1, arg2);
          }
          return default_callback(reason, arg1, arg2);
        };
        // 1. Rename the directory to something random.
        bool successfully_renamed_base_directory = true;
        {
          OUTCOME_TRY(dirhparent, topdirh.parent_path_handle());
          for(;;)
          {
            auto randomname = utils::random_string(32);
            auto ret = topdirh.relink(dirhparent, randomname);
            if(ret)
            {
              break;
            }
            if(!ret && ret.error() != errc::file_exists)
            {
              successfully_renamed_base_directory = false;
              break;
            }
          }
        }
        // 2. Enumerate the directory tree using a pool of threads to concurrently
        // enumerate the tree as quickly as possible
        std::mutex lock;
        std::atomic<int> done(0);
        std::atomic<uint64_t> totalnotremoved(0), totalremoved(0);
        std::condition_variable cond;
        optional<result<void>::error_type> callback_error;
        std::vector<std::thread> threadpool;
        // Recursion level, then list. If any thread runs out of work,
        // it always tries the lowest recursion level first for new work
        // in order to encourage maximum possible concurrency.
        std::vector<std::forward_list<directory_handle>> workqueue;
        workqueue.reserve(16);
        auto enumerate_and_remove = [&](directory_handle &dirh, size_t mylevel) -> result<bool> {
          char _kernelbuffer[65536];
          directory_handle::buffer_type _entries[4096];  // assumes ~16 everage bytes of kernel buffer per entry
          bool all_deleted = true;
          for(;;)
          {
            auto _filled = dirh.read({{_entries}, {}, directory_handle::filter::none, {_kernelbuffer}});
            if(!_filled)
            {
              // std::cout << "Directory enumeration failed with " << _filled.error().message() << std::endl;
              break;
            }
            auto &entries = _filled.value();
            size_t thisnotremoved = 0, thisremoved = 0;
            for(auto &entry : entries)
            {
              int entry_type = 0;  // 0 = unknown, 1 = file, 2 = directory
              if(entries.metadata() & stat_t::want::type)
              {
                switch(entry.stat.st_type)
                {
                case filesystem::file_type::directory:
                  entry_type = 2;
                  break;
                case filesystem::file_type::regular:
                case filesystem::file_type::symlink:
                case filesystem::file_type::block:
                case filesystem::file_type::character:
                case filesystem::file_type::fifo:
                case filesystem::file_type::socket:
                  entry_type = 1;
                  break;
                default:
                  break;
                }
              }
              // std::cout << "Entry " << entry.leafname << " has type " << entry_type << std::endl;
#ifdef _WIN32
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
              const DWORD deletefile_ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x00001000 /*FILE_DELETE_ON_CLOSE*/ | 0x040 /*FILE_NON_DIRECTORY_FILE*/;
              const DWORD deletedir_ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x00001000 /*FILE_DELETE_ON_CLOSE*/ | 0x01 /*FILE_DIRECTORY_FILE*/;
              const DWORD renamefile_ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x040 /*FILE_NON_DIRECTORY_FILE*/;
              const DWORD renamedir_ntflags = 0x20 /*FILE_SYNCHRONOUS_IO_NONALERT*/ | 0x00200000 /*FILE_OPEN_REPARSE_POINT*/ | 0x01 /*FILE_DIRECTORY_FILE*/;
              IO_STATUS_BLOCK isb = make_iostatus();
              path_view::c_str<> zpath(entry.leafname, true);
              UNICODE_STRING _path{};
              _path.Buffer = const_cast<wchar_t *>(zpath.buffer);
              _path.MaximumLength = (_path.Length = static_cast<USHORT>(zpath.length * sizeof(wchar_t))) + sizeof(wchar_t);

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
              if(entry_type != 2)
              {
                ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, deletefile_ntflags);
                if(STATUS_PENDING == ntstat)
                {
                  ntstat = ntwait(h, isb, deadline());
                }
              }
              if(entry_type != 1 || ntstat < 0)
              {
                isb = make_iostatus();
                ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, deletedir_ntflags);
                if(STATUS_PENDING == ntstat)
                {
                  ntstat = ntwait(h, isb, deadline());
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
                thisremoved++;
                // std::cout << "Removed " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
                continue;
              }

              // Try to open as a directory for later enumeration
              if(entry_type != 1)
              {
                assert(h == INVALID_HANDLE_VALUE);
                auto r = directory_handle::directory(dirh, entry.leafname, directory_handle::mode::write);
                if(r)
                {
                  // std::cout << "Adding to work queue " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
                  std::unique_lock<std::mutex> g(lock);
                  if(mylevel + 1 >= workqueue.size())
                  {
                    workqueue.resize(mylevel + 2);
                  }
                  // Enqueue for later processing
                  workqueue[mylevel + 1].emplace_front(std::move(r).value());
                  cond.notify_one();
                  all_deleted = false;
                  continue;
                }
              }

              // Try to rename the entry into topdirh
              if(dirh.unique_id() != topdirh.unique_id())
              {
                // We need to try opening the file again, but this time without FILE_DELETE_ON_CLOSE
                if(entry_type != 2)
                {
                  ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, renamefile_ntflags);
                  if(STATUS_PENDING == ntstat)
                  {
                    ntstat = ntwait(h, isb, deadline());
                  }
                }
                if(entry_type != 1 || ntstat < 0)
                {
                  ntstat = NtOpenFile(&h, access, &oa, &isb, fileshare, renamedir_ntflags);
                  if(STATUS_PENDING == ntstat)
                  {
                    ntstat = ntwait(h, isb, deadline());
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
                    continue;
                  }
                  thisnotremoved++;
                  all_deleted = false;
                  // std::cout << "Failed to remove " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
                  auto r = callback(remove_all_callback_reason::unrenameable, {dirh}, {entry.leafname});
                  if(!r)
                  {
                    std::unique_lock<std::mutex> g(lock);
                    if(!callback_error)
                    {
                      callback_error = std::move(r).error();
                    }
                    return false;
                  }
                  continue;
                }
              }

              // Otherwise this fellow is unremoveable
              auto r = callback(remove_all_callback_reason::unremoveable, {dirh}, {entry.leafname});
              if(!r)
              {
                std::unique_lock<std::mutex> g(lock);
                if(!callback_error)
                {
                  callback_error = std::move(r).error();
                }
                return false;
              }
#else
              // We have unlinkat()
              path_view::c_str<> zpath(entry.leafname);
              errno = 0;
              if(entry_type == 2 || -1 == ::unlinkat(dirh.native_handle().fd, zpath.buffer, 0))
              {
                if(ENOENT == errno)
                {
                  // Somebody else removed it
                  continue;
                }
                if(entry_type != 1 || EISDIR == errno)
                {
                  // Try to remove it as a directory, if it's empty we've saved a recurse
                  if(-1 != ::unlinkat(dirh.native_handle().fd, zpath.buffer, AT_REMOVEDIR))
                  {
                    // std::cout << "Removed quickly " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
                    thisremoved++;
                    continue;
                  }
                  // Try to open as a directory for later enumeration
                  auto r = directory_handle::directory(dirh, entry.leafname, directory_handle::mode::write);
                  if(r)
                  {
                    // std::cout << "Adding to work queue " << (dirh.current_path().value() / entry.leafname.path()) << std::endl;
                    std::unique_lock<std::mutex> g(lock);
                    if(mylevel + 1 >= workqueue.size())
                    {
                      workqueue.resize(mylevel + 2);
                    }
                    // Enqueue for later processing
                    workqueue[mylevel + 1].emplace_front(std::move(r).value());
                    cond.notify_one();
                    all_deleted = false;
                  }
                  continue;
                }
                if(dirh.unique_id() != topdirh.unique_id())
                {
                  // Try renaming it into topdirh
                  auto randomname = utils::random_string(32);
                  if(-1 != ::renameat(topdirh.native_handle().fd, randomname.c_str(), dirh.native_handle().fd, zpath.buffer))
                  {
                    continue;
                  }
                  thisnotremoved++;
                  all_deleted = false;
                  auto r = callback(remove_all_callback_reason::unrenameable, {dirh}, {entry.leafname});
                  if(!r)
                  {
                    std::unique_lock<std::mutex> g(lock);
                    if(!callback_error)
                    {
                      callback_error = std::move(r).error();
                    }
                    return false;
                  }
                  continue;
                }
                // Otherwise this fellow is unremoveable
                auto r = callback(remove_all_callback_reason::unremoveable, {dirh}, {entry.leafname});
                if(!r)
                {
                  std::unique_lock<std::mutex> g(lock);
                  if(!callback_error)
                  {
                    callback_error = std::move(r).error();
                  }
                  return false;
                }
                continue;
              }
              thisremoved++;
#endif
            }
            if(entries.done())
            {
              totalnotremoved.fetch_add(thisnotremoved, std::memory_order_relaxed);
              totalremoved.fetch_add(thisremoved, std::memory_order_relaxed);
              auto r = callback(remove_all_callback_reason::progress_enumeration, {(uint64_t) totalnotremoved}, {(uint64_t) totalremoved});
              if(!r)
              {
                std::unique_lock<std::mutex> g(lock);
                if(!callback_error)
                {
                  callback_error = std::move(r).error();
                }
                return false;
              }
              break;
            }
          }
          // Try to remove myself, if I am already deleted sink that
          if(all_deleted)
          {
            // auto path = dirh.current_path().value();
            auto r = dirh.unlink();
            if(r || r.error() == errc::no_such_file_or_directory)
            {
              totalremoved.fetch_add(1, std::memory_order_relaxed);
              // std::cout << "Removed " << path << std::endl;
              return true;
            }
            return std::move(r).error();
          }
          return false;
        };
        // Try a fast path exit without needing a thread pool
        {
          auto r = enumerate_and_remove(topdirh, 0);
          if(r && r.value())
          {
            // There were no subdirectories, so we are done
            return totalremoved;
          }
        }

        // Set up the threadpool of workers
        auto worker = [&] {
          log_level_guard logg(log_level::fatal);
          for(;;)
          {
            directory_handle mywork;
            size_t mylevel = 0;
            do
            {
              std::unique_lock<std::mutex> g(lock);
              if(callback_error)
              {
                return;
              }
              for(size_t level = 0; level < workqueue.size(); level++)
              {
                if(!workqueue[level].empty())
                {
                  mywork = std::move(workqueue[level].front());
                  mylevel = level;
                  workqueue[level].pop_front();
                  break;
                }
              }
              if(!mywork.is_valid())
              {
                done.fetch_add(1, std::memory_order_relaxed);
                cond.wait(g);
                if(done.fetch_sub(1, std::memory_order_relaxed) <= 0)
                {
                  done.fetch_sub(1000000, std::memory_order_relaxed);
                  return;
                }
              }
            } while(!mywork.is_valid());
            auto r = enumerate_and_remove(mywork, mylevel);
            (void) r;
          }
        };
        if(0 == threads)
        {
          // Filesystems are generally only concurrent to the real CPU count
          threads = std::thread::hardware_concurrency() / 2;
          if(threads == 0)
          {
            threads = 4;
          }
        }
        threadpool.reserve(threads);
        OUTCOME_TRY(callback(remove_all_callback_reason::begin_enumeration, {successfully_renamed_base_directory}, {nullptr}));
        // Kick off parallel unlinking
        for(size_t n = 0; n < threads; n++)
        {
          threadpool.emplace_back(worker);
        }

        // Wait until the thread pool falls inactive, then loop removing the tree
        // until nothing more can be removed
        uint64_t lasttotalremoved(totalremoved);
        for(;;)
        {
          bool alldone = false;
          std::unique_lock<std::mutex> g(lock);
          if(callback_error)
          {
            break;
          }
          if(done.load(std::memory_order_relaxed) == (int) threadpool.size())
          {
            // All threads are currently in cond.wait()
            alldone = true;
            for(auto &i : workqueue)
            {
              if(!i.empty())
              {
                alldone = false;
                break;
              }
            }
          }
          g.unlock();
          if(alldone)
          {
            if(lasttotalremoved == totalremoved)
            {
              // We are done with the threadpool
              break;
            }
            // Run it again
            lasttotalremoved = totalremoved;
            auto r = enumerate_and_remove(topdirh, 0);
            if(r && r.value())
            {
              // There were no subdirectories, so we are done
              totalnotremoved = 0;
              break;
            }
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        done.fetch_sub((int) threadpool.size(), std::memory_order_relaxed);  // threads ought to exit
        while(done != (int) threadpool.size() * -1000000 - (int) threadpool.size())
        {
          cond.notify_all();
          std::this_thread::yield();
        }
        for(auto &i : threadpool)
        {
          i.join();
        }
        if(callback_error)
        {
          return std::move(*callback_error);
        }
        OUTCOME_TRY(callback(remove_all_callback_reason::end_enumeration, {(uint64_t) totalnotremoved}, {(uint64_t) totalremoved}));
        while(totalnotremoved != 0)
        {
          totalnotremoved = 0;
          // We now fall back to a single threaded traversal of the directory tree,
          // repeatedly trying to unlink the items
          auto r = enumerate_and_remove(topdirh, 0);
          if(r && r.value())
          {
            // There were no subdirectories, so we are done
            assert(totalnotremoved == 0);
            break;
          }
          if(callback_error)
          {
            return std::move(*callback_error);
          }
          for(;;)
          {
            bool alldone = true;
            for(size_t level = 0; level < workqueue.size(); level++)
            {
              if(!workqueue[level].empty())
              {
                (void) enumerate_and_remove(workqueue[level].front(), level);
                if(callback_error)
                {
                  return std::move(*callback_error);
                }
                workqueue[level].pop_front();
                alldone = false;
              }
            }
            if(alldone)
            {
              break;
            }
          }
        }
        OUTCOME_TRY(callback(remove_all_callback_reason::finished, {(uint64_t) totalnotremoved}, {(uint64_t) totalremoved}));
        auto closetopdirh = std::move(topdirh);
        OUTCOME_TRY(closetopdirh.close());
        return totalremoved;
      }
      catch(...)
      {
        return error_from_exception();
      }
    }
  }  // namespace detail
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
