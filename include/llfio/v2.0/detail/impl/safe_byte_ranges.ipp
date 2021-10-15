/* Safe small actor read-write lock
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
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

#if defined(_WIN32)
#error This file should never be compiled on Windows.
#endif

#include "../../algorithm/shared_fs_mutex/safe_byte_ranges.hpp"

#include "quickcpplib/uint128.hpp"
#include "quickcpplib/utils/thread.hpp"

#include <fcntl.h>
#include <sys/stat.h>

#include <condition_variable>
#include <mutex>
#include <unordered_map>

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace shared_fs_mutex
  {
    namespace detail
    {
#if 0
      struct _byte_ranges : public byte_ranges
      {
        using byte_ranges::byte_ranges;
        using byte_ranges::_lock;
        _byte_ranges(byte_ranges &&o)
            : byte_ranges(std::move(o))
        {
        }
      };
#endif
      class threaded_byte_ranges : public shared_fs_mutex
      {
      public:
        using entity_type = shared_fs_mutex::entity_type;
        using entities_type = shared_fs_mutex::entities_type;

      private:
        std::mutex _m;
        file_handle _h;
        std::condition_variable _changed;
        struct _entity_info
        {
          std::vector<unsigned> reader_tids;  // thread ids of all shared lock holders
          unsigned writer_tid;                // thread id of exclusive lock holder
          file_handle::extent_guard filelock;   // exclusive if writer_tid, else shared
          _entity_info(bool exclusive, unsigned tid, file_handle::extent_guard _filelock)
              : writer_tid(exclusive ? tid : 0)
              , filelock(std::move(_filelock))
          {
            reader_tids.reserve(4);
            if(!exclusive)
            {
              reader_tids.push_back(tid);
            }
          }
        };
        std::unordered_map<entity_type::value_type, _entity_info> _thread_locks;  // entity to thread lock
        // _m mutex must be held on entry!
        void _unlock(unsigned mythreadid, entity_type entity)
        {
          auto it = _thread_locks.find(entity.value);  // NOLINT
          assert(it != _thread_locks.end());
          assert(it->second.writer_tid == mythreadid || it->second.writer_tid == 0);
          if(it->second.writer_tid == mythreadid)
          {
            if(!it->second.reader_tids.empty())
            {
              // Downgrade the lock from exclusive to shared
              auto l = _h.lock_file_range(entity.value, 1, lock_kind::shared).value();
#ifndef _WIN32
              // On POSIX byte range locks replace
              it->second.filelock.release();
#endif
              it->second.filelock = std::move(l);
              it->second.writer_tid = 0;
              return;
            }
          }
          else
          {
            // Remove me from reader tids
            auto reader_tid_it = std::find(it->second.reader_tids.begin(), it->second.reader_tids.end(), mythreadid);
            assert(reader_tid_it != it->second.reader_tids.end());
            if(reader_tid_it != it->second.reader_tids.end())
            {
              // We don't care about the order, so fastest is to swap this tid with final tid and resize down
              std::swap(*reader_tid_it, it->second.reader_tids.back());
              it->second.reader_tids.pop_back();
            }
          }
          if(it->second.reader_tids.empty())
          {
            // Release the lock and delete this entity from the map
            _h.unlock_file_range(entity.value, 1);
            _thread_locks.erase(it);
          }
        }

      public:
        threaded_byte_ranges(const path_handle &base, path_view lockfile)
        {
          LLFIO_LOG_FUNCTION_CALL(0);
          _h = file_handle::file(base, lockfile, file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::temporary).value();
        }
        LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> _lock(entities_guard &out, deadline d, bool spin_not_sleep) noexcept final
        {
          LLFIO_LOG_FUNCTION_CALL(this);
          unsigned mythreadid = QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id();
          std::chrono::steady_clock::time_point began_steady;
          std::chrono::system_clock::time_point end_utc;
          if(d)
          {
            if((d).steady)
            {
              began_steady = std::chrono::steady_clock::now();
            }
            else
            {
              end_utc = (d).to_time_point();
            }
          }
          // Fire this if an error occurs
          auto disableunlock = make_scope_exit([&]() noexcept { out.release(); });
          size_t n;
          for(;;)
          {
            auto was_contended = static_cast<size_t>(-1);
            bool pls_sleep = true;
            std::unique_lock<decltype(_m)> guard(_m);
            {
              auto undo = make_scope_exit([&]() noexcept {
                // 0 to (n-1) need to be closed
                if(n > 0)
                {
                  --n;
                  // Now 0 to n needs to be closed
                  for(; n > 0; n--)
                  {
                    _unlock(mythreadid, out.entities[n]);
                  }
                  _unlock(mythreadid, out.entities[0]);
                }
              });
              for(n = 0; n < out.entities.size(); n++)
              {
                auto it = _thread_locks.find(out.entities[n].value);
                if(it == _thread_locks.end())
                {
                  // This entity has not been locked before
                  deadline nd;
                  // Only for very first entity will we sleep until its lock becomes available
                  if(n != 0u)
                  {
                    nd = deadline(std::chrono::seconds(0));
                  }
                  else
                  {
                    nd = deadline();
                    if(d)
                    {
                      if((d).steady)
                      {
                        std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());
                        if(ns.count() < 0)
                        {
                          (nd).nsecs = 0;
                        }
                        else
                        {
                          (nd).nsecs = ns.count();
                        }
                      }
                      else
                      {
                        (nd) = (d);
                      }
                    }
                  }
                  // Allow other threads to use this threaded_byte_ranges
                  guard.unlock();
                  auto outcome = _h.lock_file_range(out.entities[n].value, 1, (out.entities[n].exclusive != 0u) ? lock_kind::exclusive : lock_kind::shared, nd);
                  guard.lock();
                  if(!outcome)
                  {
                    was_contended = n;
                    pls_sleep = false;
                    goto failed;
                  }
                  // Did another thread already fill this in?
                  it = _thread_locks.find(out.entities[n].value);
                  if(it == _thread_locks.end())
                  {
                    it = _thread_locks.insert(std::make_pair(static_cast<entity_type::value_type>(out.entities[n].value), _entity_info(out.entities[n].exclusive != 0u, mythreadid, std::move(outcome).value()))).first;
                    continue;
                  }
                  // Otherwise throw away the presumably shared superfluous byte range lock
                  assert(!out.entities[n].exclusive);
                }

                // If we are here, then this entity has been locked by someone before
                auto reader_tid_it = std::find(it->second.reader_tids.begin(), it->second.reader_tids.end(), mythreadid);
                bool already_have_shared_lock = (reader_tid_it != it->second.reader_tids.end());
                // Is somebody already locking this entity exclusively?
                if(it->second.writer_tid != 0)
                {
                  if(it->second.writer_tid == mythreadid)
                  {
                    // If I am relocking myself, return deadlock
                    if((out.entities[n].exclusive != 0u) || already_have_shared_lock)
                    {
                      return errc::resource_deadlock_would_occur;
                    }
                    // Otherwise just add myself to the reader list
                    it->second.reader_tids.push_back(mythreadid);
                    continue;
                  }
                  // Some other thread holds the exclusive lock, so we cannot take it
                  was_contended = n;
                  pls_sleep = true;
                  goto failed;
                }
                // If reached here, nobody is holding the exclusive lock
                assert(it->second.writer_tid == 0);
                if(out.entities[n].exclusive == 0u)
                {
                  // If I am relocking myself, return deadlock
                  if(already_have_shared_lock)
                  {
                    return errc::resource_deadlock_would_occur;
                  }
                  // Otherwise just add myself to the reader list
                  it->second.reader_tids.push_back(mythreadid);
                  continue;
                }
                // We are thus now upgrading shared to exclusive
                assert(out.entities[n].exclusive);
                deadline nd;
                // Only for very first entity will we sleep until its lock becomes available
                if(n != 0u)
                {
                  nd = deadline(std::chrono::seconds(0));
                }
                else
                {
                  nd = deadline();
                  if(d)
                  {
                    if((d).steady)
                    {
                      std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());
                      if(ns.count() < 0)
                      {
                        (nd).nsecs = 0;
                      }
                      else
                      {
                        (nd).nsecs = ns.count();
                      }
                    }
                    else
                    {
                      (nd) = (d);
                    }
                  }
                }
                // Allow other threads to use this threaded_byte_ranges
                guard.unlock();
                auto outcome = _h.lock_file_range(out.entities[n].value, 1, lock_kind::exclusive, nd);
                guard.lock();
                if(!outcome)
                {
                  was_contended = n;
                  goto failed;
                }
// unordered_map iterators do not invalidate, so no need to refresh
#ifndef _WIN32
                // On POSIX byte range locks replace
                it->second.filelock.release();
#endif
                it->second.filelock = std::move(outcome).value();
                it->second.writer_tid = mythreadid;
              }
              // Dismiss unwind of thread locking and return success
              undo.release();
              disableunlock.release();
              return success();
            }
          failed:
            if(d)
            {
              if((d).steady)
              {
                if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds((d).nsecs)))
                {
                  return errc::timed_out;
                }
              }
              else
              {
                if(std::chrono::system_clock::now() >= end_utc)
                {
                  return errc::timed_out;
                }
              }
            }
            // Move was_contended to front and randomise rest of out.entities
            std::swap(out.entities[was_contended], out.entities[0]);
            auto front = out.entities.begin();
            ++front;
            QUICKCPPLIB_NAMESPACE::algorithm::small_prng::random_shuffle(front, out.entities.end());
            if(pls_sleep && !spin_not_sleep)
            {
              // Sleep until the thread locks next change
              if((d).steady)
              {
                std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>((began_steady + std::chrono::nanoseconds((d).nsecs)) - std::chrono::steady_clock::now());
                _changed.wait_for(guard, ns);
              }
              else
              {
                _changed.wait_until(guard, d.to_time_point());
              }
            }
          }
          // return success();
        }
        LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock(entities_type entities, unsigned long long /*unused*/) noexcept final
        {
          LLFIO_LOG_FUNCTION_CALL(this);
          unsigned mythreadid = QUICKCPPLIB_NAMESPACE::utils::thread::this_thread_id();
          std::unique_lock<decltype(_m)> guard(_m);
          for(auto &entity : entities)
          {
            _unlock(mythreadid, entity);
          }
        }
      };
      struct threaded_byte_ranges_list
      {
        using key_type = QUICKCPPLIB_NAMESPACE::integers128::uint128;
        std::mutex lock;
        std::unordered_map<key_type, std::weak_ptr<threaded_byte_ranges>, QUICKCPPLIB_NAMESPACE::integers128::uint128_hasher> db;
      };
      inline threaded_byte_ranges_list &tbrlist()
      {
        static threaded_byte_ranges_list v;
        return v;
      }
      LLFIO_HEADERS_ONLY_FUNC_SPEC result<std::shared_ptr<shared_fs_mutex>> inode_to_fs_mutex(const path_handle &base, path_view lockfile) noexcept
      {
        try
        {
          path_view::zero_terminated_rendered_path<> zpath(lockfile);
          struct stat s
          {
          };
          if(-1 == ::fstatat(base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.data(), &s, AT_SYMLINK_NOFOLLOW))
          {
            return posix_error();
          }
          threaded_byte_ranges_list::key_type key;
          key.as_longlongs[0] = s.st_ino;
          key.as_longlongs[1] = s.st_dev;
          std::shared_ptr<threaded_byte_ranges> ret;
          auto &list = tbrlist();
          std::lock_guard<decltype(list.lock)> g(list.lock);
          auto it = list.db.find(key);
          if(it != list.db.end())
          {
            ret = it->second.lock();
            if(ret)
            {
              return ret;
            }
          }
          ret = std::make_shared<threaded_byte_ranges>(base, lockfile);
          if(it != list.db.end())
          {
            it->second = std::weak_ptr<threaded_byte_ranges>(ret);
          }
          else
          {
            list.db.insert({key, std::weak_ptr<threaded_byte_ranges>(ret)});
          }
          return ret;
        }
        catch(...)
        {
          return error_from_exception();
        }
      }
    }  // namespace detail
  }    // namespace shared_fs_mutex
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
