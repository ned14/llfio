/* Efficient large actor read-write lock
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (23 commits)
File Created: Aug 2016


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

#ifndef LLFIO_SHARED_FS_MUTEX_MEMORY_MAP_HPP
#define LLFIO_SHARED_FS_MUTEX_MEMORY_MAP_HPP

#include "../../map_handle.hpp"
#include "base.hpp"

#include "quickcpplib/algorithm/hash.hpp"
#include "quickcpplib/algorithm/small_prng.hpp"

//! \file memory_map.hpp Provides algorithm::shared_fs_mutex::memory_map

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace shared_fs_mutex
  {
    /*! \class memory_map
    \brief Many entity memory mapped shared/exclusive file system based lock
    \tparam Hasher A STL compatible hash algorithm to use (defaults to `fnv1a_hash`)
    \tparam HashIndexSize The size in bytes of the hash index to use (defaults to 4Kb)
    \tparam SpinlockType The type of spinlock to use (defaults to a `SharedMutex` concept spinlock)

    This is the highest performing filing system mutex in LLFIO, but it comes with a long list of potential
    gotchas. It works by creating a random temporary file somewhere on the system and placing its path
    in a file at the lock file location. The random temporary file is mapped into memory by all processes
    using the lock where an open addressed hash table is kept. Each entity is hashed into somewhere in the
    hash table and its individual spin lock is used to implement the exclusion. As with `byte_ranges`, each
    entity is locked individually in sequence but if a particular lock fails, all are unlocked and the
    list is randomised before trying again. Because this locking
    implementation is entirely implemented in userspace using shared memory without any kernel syscalls,
    performance is probably as fast as any many-arbitrary-entity shared locking system could be.

    As it uses shared memory, this implementation of `shared_fs_mutex` cannot work over a networked
    drive. If you attempt to open this lock on a network drive and the first user of the lock is not
    on this local machine, `errc::no_lock_available` will be returned from the constructor.

    - Linear complexity to number of concurrent users up until hash table starts to get full or hashed
    entries collide.
    - Sudden power loss during use is recovered from.
    - Safe for multithreaded usage of the same instance.
    - In the lightly contended case, an order of magnitude faster than any other `shared_fs_mutex` algorithm.

    Caveats:
    - No ability to sleep until a lock becomes free, so CPUs are spun at 100%.
    - Sudden process exit with locks held will deadlock all other users.
    - Exponential complexity to number of entities being concurrently locked.
    - Exponential complexity to concurrency if entities hash to the same cache line. Most SMP and especially
    NUMA systems have a finite bandwidth for atomic compare and swap operations, and every attempt to
    lock or unlock an entity under this implementation is several of those operations. Under heavy contention,
    whole system performance very noticeably nose dives from excessive atomic operations, things like audio and the
    mouse pointer will stutter.
    - Sometimes different entities hash to the same offset and collide with one another, causing very poor performance.
    - Memory mapped files need to be cache unified with normal i/o in your OS kernel. Known OSs which
    don't use a unified cache for memory mapped and normal i/o are QNX, OpenBSD. Furthermore, doing
    normal i/o and memory mapped i/o to the same file needs to not corrupt the file. In the past,
    there have been editions of the Linux kernel and the OS X kernel which did this.
    - If your OS doesn't have sane byte range locks (OS X, BSD, older Linuxes) and multiple
    objects in your process use the same lock file, misoperation will occur.
    - Requires `handle::current_path()` to be working.

    \todo memory_map::_hash_entities needs to hash x16, x8 and x4 at a time to encourage auto vectorisation
    */
    template <template <class> class Hasher = QUICKCPPLIB_NAMESPACE::algorithm::hash::fnv1a_hash, size_t HashIndexSize = 4096, class SpinlockType = QUICKCPPLIB_NAMESPACE::configurable_spinlock::shared_spinlock<>> class memory_map : public shared_fs_mutex
    {
    public:
      //! The type of an entity id
      using entity_type = shared_fs_mutex::entity_type;
      //! The type of a sequence of entities
      using entities_type = shared_fs_mutex::entities_type;
      //! The type of the hasher being used
      using hasher_type = Hasher<entity_type::value_type>;
      //! The type of the spinlock being used
      using spinlock_type = SpinlockType;

    private:
      static constexpr size_t _container_entries = HashIndexSize / sizeof(spinlock_type);
      using _hash_index_type = std::array<spinlock_type, _container_entries>;
      static constexpr file_handle::extent_type _initialisingoffset = static_cast<file_handle::extent_type>(1024) * 1024;
      static constexpr file_handle::extent_type _lockinuseoffset = static_cast<file_handle::extent_type>(1024) * 1024 + 1;

      file_handle _h, _temph;
      file_handle::extent_guard _hlockinuse;  // shared lock of last byte of _h marking if lock is in use
      map_handle _hmap, _temphmap;

      _hash_index_type &_index() const
      {
        auto *ret = reinterpret_cast<_hash_index_type *>(_temphmap.address());
        return *ret;
      }

      memory_map(file_handle &&h, file_handle &&temph, file_handle::extent_guard &&hlockinuse, map_handle &&hmap, map_handle &&temphmap)
          : _h(std::move(h))
          , _temph(std::move(temph))
          , _hlockinuse(std::move(hlockinuse))
          , _hmap(std::move(hmap))
          , _temphmap(std::move(temphmap))
      {
        _hlockinuse.set_handle(&_h);
      }

    public:
      //! No copy construction
      memory_map(const memory_map &) = delete;
      //! No copy assignment
      memory_map &operator=(const memory_map &) = delete;
      //! Move constructor
      memory_map(memory_map &&o) noexcept : _h(std::move(o._h)), _temph(std::move(o._temph)), _hlockinuse(std::move(o._hlockinuse)), _hmap(std::move(o._hmap)), _temphmap(std::move(o._temphmap)) { _hlockinuse.set_handle(&_h); }
      //! Move assign
      memory_map &operator=(memory_map &&o) noexcept
      {
        this->~memory_map();
        new(this) memory_map(std::move(o));
        return *this;
      }
      ~memory_map() override
      {
        if(_h.is_valid())
        {
          // Release the maps
          _hmap = {};
          _temphmap = {};
          // Release my shared locks and try locking inuse exclusively
          _hlockinuse.unlock();
          auto lockresult = _h.lock_file_range(_initialisingoffset, 2, lock_kind::exclusive, std::chrono::seconds(0));
#ifndef NDEBUG
          if(!lockresult && lockresult.error() != errc::timed_out)
          {
            LLFIO_LOG_FATAL(0, "memory_map::~memory_map() try_lock failed");
            abort();
          }
#endif
          if(lockresult)
          {
            // This means I am the last user, so zop the file contents as temp file is about to go away
            auto o2 = _h.truncate(0);
            if(!o2)
            {
              LLFIO_LOG_FATAL(0, "memory_map::~memory_map() truncate failed");
#ifndef NDEBUG
              std::cerr << "~memory_map() truncate failed due to " << o2.error().message().c_str() << std::endl;
#endif
              abort();
            }
            // Unlink the temp file. We don't trap any failure to unlink on FreeBSD it can forget current path.
            auto o3 = _temph.unlink();
            if(!o3)
            {
#ifdef __FreeBSD__
#ifndef NDEBUG
              std::cerr << "~memory_map() unlink failed due to " << o3.error().message().c_str() << std::endl;
#endif
#else
              LLFIO_LOG_FATAL(0, "memory_map::~memory_map() unlink failed");
#ifndef NDEBUG
              std::cerr << "~memory_map() unlink failed due to " << o3.error().message().c_str() << std::endl;
#endif
              abort();
#endif
            }
          }
        }
      }

      /*! Initialises a shared filing system mutex using the file at \em lockfile.
      \errors Awaiting the clang result<> AST parser which auto generates all the error codes which could occur,
      but a particularly important one is `errc::no_lock_available` which will be returned if the lock
      is in use by another computer on a network.
      */
      LLFIO_MAKE_FREE_FUNCTION
      static result<memory_map> fs_mutex_map(const path_handle &base, path_view lockfile) noexcept
      {
        LLFIO_LOG_FUNCTION_CALL(0);
        try
        {
          OUTCOME_TRY(auto &&ret, file_handle::file(base, lockfile, file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::reads));
          file_handle temph;
          // Am I the first person to this file? Lock everything exclusively
          auto lockinuse = ret.lock_file_range(_initialisingoffset, 2, lock_kind::exclusive, std::chrono::seconds(0));
          if(lockinuse.has_error())
          {
            if(lockinuse.error() != errc::timed_out)
            {
              return std::move(lockinuse).error();
            }
            // Somebody else is also using this file, so try to read the hash index file I ought to use
            lockinuse = ret.lock_file_range(_lockinuseoffset, 1, lock_kind::shared);  // inuse shared access, blocking
            if(!lockinuse)
            {
              return std::move(lockinuse).error();
            }
            byte buffer[65536];
            memset(buffer, 0, sizeof(buffer));
            OUTCOME_TRYV(ret.read(0, {{buffer, 65535}}));
            path_view temphpath(reinterpret_cast<filesystem::path::value_type *>(buffer));
            result<file_handle> _temph(in_place_type<file_handle>);
            _temph = file_handle::file({}, temphpath, file_handle::mode::write, file_handle::creation::open_existing, file_handle::caching::temporary);
            // If temp file doesn't exist, I am on a different machine
            if(!_temph)
            {
              // Release the exclusive lock and tell caller that this lock is not available
              return errc::no_lock_available;
            }
            temph = std::move(_temph.value());
            // Map the hash index file into memory for read/write access
            OUTCOME_TRY(auto &&temphsection, section_handle::section(temph, HashIndexSize));
            OUTCOME_TRY(auto &&temphmap, map_handle::map(temphsection, HashIndexSize));
            // Map the path file into memory with its maximum possible size, read only
            OUTCOME_TRY(auto &&hsection, section_handle::section(ret, 65536, section_handle::flag::read));
            OUTCOME_TRY(auto &&hmap, map_handle::map(hsection, 0, 0, section_handle::flag::read));
            return memory_map(std::move(ret), std::move(temph), std::move(lockinuse.value()), std::move(hmap), std::move(temphmap));
          }

          // I am the first person to be using this (stale?) file, so create a new hash index file in /tmp
          auto &tempdirh = path_discovery::memory_backed_temporary_files_directory().is_valid() ? path_discovery::memory_backed_temporary_files_directory() : path_discovery::storage_backed_temporary_files_directory();
          OUTCOME_TRY(auto &&_temph, file_handle::uniquely_named_file(tempdirh));
          temph = std::move(_temph);
          // Truncate it out to the hash index size, and map it into memory for read/write access
          OUTCOME_TRYV(temph.truncate(HashIndexSize));
          OUTCOME_TRY(auto &&temphsection, section_handle::section(temph, HashIndexSize));
          OUTCOME_TRY(auto &&temphmap, map_handle::map(temphsection, HashIndexSize));
          // Write the path of my new hash index file, padding zeros to the nearest page size
          // multiple to work around a race condition in the Linux kernel
          OUTCOME_TRY(auto &&temppath, temph.current_path());
          char buffer[4096];
          memset(buffer, 0, sizeof(buffer));
          size_t bytes = temppath.native().size() * sizeof(*temppath.c_str());
          file_handle::const_buffer_type buffers[] = {{reinterpret_cast<const byte *>(temppath.c_str()), bytes}, {reinterpret_cast<const byte *>(buffer), 4096 - (bytes % 4096)}};
          OUTCOME_TRYV(ret.truncate(65536));
          OUTCOME_TRYV(ret.write({buffers, 0}));
          // Map for read the maximum possible path file size, again to avoid race problems
          OUTCOME_TRY(auto &&hsection, section_handle::section(ret, 65536, section_handle::flag::read));
          OUTCOME_TRY(auto &&hmap, map_handle::map(hsection, 0, 0, section_handle::flag::read));
          /* Take shared locks on inuse. Even if this implementation doesn't implement
          atomic downgrade of exclusive range to shared range, we're fully prepared for other users
          now. The _initialisingoffset remains exclusive to prevent double entry into this init routine.
          */
          OUTCOME_TRY(auto &&lockinuse2, ret.lock_file_range(_lockinuseoffset, 1, lock_kind::shared));
          lockinuse = std::move(lockinuse2);  // releases exclusive lock on all three offsets
          return memory_map(std::move(ret), std::move(temph), std::move(lockinuse.value()), std::move(hmap), std::move(temphmap));
        }
        catch(...)
        {
          return error_from_exception();
        }
      }

      //! Return the handle to file being used for this lock
      const file_handle &handle() const noexcept { return _h; }

    protected:
      struct _entity_idx
      {
        unsigned value : 31;
        unsigned exclusive : 1;
      };
      // Create a cache of entities to their indices, eliding collisions where necessary
      static span<_entity_idx> _hash_entities(_entity_idx *entity_to_idx, entities_type &entities)
      {
        _entity_idx *ep = entity_to_idx;
        for(size_t n = 0; n < entities.size(); n++)
        {
          ep->value = hasher_type()(entities[n].value) % _container_entries;
          ep->exclusive = entities[n].exclusive;
          bool skip = false;
          for(size_t m = 0; m < n; m++)
          {
            if(entity_to_idx[m].value == ep->value)
            {
              if(ep->exclusive && !entity_to_idx[m].exclusive)
              {
                entity_to_idx[m].exclusive = true;
              }
              skip = true;
            }
          }
          if(!skip)
          {
            ++ep;
          }
        }
        return span<_entity_idx>(entity_to_idx, ep - entity_to_idx);
      }
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> _lock(entities_guard &out, deadline d, bool spin_not_sleep) noexcept final
      {
        LLFIO_LOG_FUNCTION_CALL(this);
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
        // alloca() always returns 16 byte aligned addresses
        span<_entity_idx> entity_to_idx(_hash_entities(reinterpret_cast<_entity_idx *>(alloca(sizeof(_entity_idx) * out.entities.size())), out.entities));
        _hash_index_type &index = _index();
        // Fire this if an error occurs
        auto disableunlock = make_scope_exit([&]() noexcept { out.release(); });
        size_t n;
        for(;;)
        {
          auto was_contended = static_cast<size_t>(-1);
          {
            auto undo = make_scope_exit([&]() noexcept {
              // 0 to (n-1) need to be closed
              if(n > 0)
              {
                --n;
                // Now 0 to n needs to be closed
                for(; n > 0; n--)
                {
                  entity_to_idx[n].exclusive ? index[entity_to_idx[n].value].unlock() : index[entity_to_idx[n].value].unlock_shared();
                }
                entity_to_idx[0].exclusive ? index[entity_to_idx[0].value].unlock() : index[entity_to_idx[0].value].unlock_shared();
              }
            });
            for(n = 0; n < entity_to_idx.size(); n++)
            {
              if(!(entity_to_idx[n].exclusive ? index[entity_to_idx[n].value].try_lock() : index[entity_to_idx[n].value].try_lock_shared()))
              {
                was_contended = n;
                goto failed;
              }
            }
            // Everything is locked, exit
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
          std::swap(entity_to_idx[was_contended], entity_to_idx[0]);
          auto front = entity_to_idx.begin();
          ++front;
          QUICKCPPLIB_NAMESPACE::algorithm::small_prng::random_shuffle(front, entity_to_idx.end());
          if(!spin_not_sleep)
          {
            std::this_thread::yield();
          }
        }
        // return success();
      }

    public:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock(entities_type entities, unsigned long long /*unused*/) noexcept final
      {
        LLFIO_LOG_FUNCTION_CALL(this);
        span<_entity_idx> entity_to_idx(_hash_entities(reinterpret_cast<_entity_idx *>(alloca(sizeof(_entity_idx) * entities.size())), entities));
        _hash_index_type &index = _index();
        for(const auto &i : entity_to_idx)
        {
          i.exclusive ? index[i.value].unlock() : index[i.value].unlock_shared();
        }
      }
    };

  }  // namespace shared_fs_mutex
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END


#endif
