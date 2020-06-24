/* Efficient many actor read-write lock
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: March 2016


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

#ifndef LLFIO_SHARED_FS_MUTEX_ATOMIC_APPEND_HPP
#define LLFIO_SHARED_FS_MUTEX_ATOMIC_APPEND_HPP

#include "../../file_handle.hpp"
#include "base.hpp"

#include <cassert>
#include <thread>  // for yield()

//! \file atomic_append.hpp Provides algorithm::shared_fs_mutex::atomic_append

#if __GNUC__ >= 8
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace shared_fs_mutex
  {
#if !DOXYGEN_SHOULD_SKIP_THIS
    namespace atomic_append_detail
    {
#pragma pack(push)
#pragma pack(1)
      struct alignas(16) header
      {
        uint128 hash;                     // Hash of remaining 112 bytes
        uint64 generation{};              // Iterated per write
        uint64 time_offset{};             // time_t in seconds at time of creation. Used to
                                          // offset us_count below.
        uint64 first_known_good{};        // offset to first known good lock_request
        uint64 first_after_hole_punch{};  // offset to first byte after last hole
                                          // punch
        // First 48 bytes are the header, remainder is zeros for future
        // expansion
        uint64 _padding[10]{};
        // Last byte is used to detect first user of the file
      };
      static_assert(sizeof(header) == 128, "header structure is not 128 bytes long!");
      static_assert(std::is_trivially_copyable<header>::value, "header structure is not trivially copyable");

      struct alignas(16) lock_request
      {
        uint128 hash;                               // Hash of remaining 112 bytes
        uint64 unique_id{};                         // A unique id identifying this locking instance
        uint64 us_count : 56;                       // Microseconds since the lock file created
        uint64 items : 8;                           // The number of entities below which are valid
        shared_fs_mutex::entity_type entities[12];  // Entities to exclusive or share lock
        constexpr lock_request()
            : us_count{}
            , items{}
        {
        }
      };
      static_assert(sizeof(lock_request) == 128, "lock_request structure is not 128 bytes long!");
      static_assert(std::is_trivially_copyable<lock_request>::value, "header structure is not trivially copyable");
#pragma pack(pop)
    }  // namespace atomic_append_detail
#endif
    /*! \class atomic_append
    \brief Scalable many entity shared/exclusive file system based lock

    Lock files and byte ranges scale poorly to the number of items being concurrently locked with typically an exponential
    drop off in performance as the number of items being concurrently locked rises. This file system
    algorithm solves this problem using IPC via a shared append-only lock file.

    - Compatible with networked file systems (NFS too if the special nfs_compatibility flag is true.
    Note turning this on is not free of cost if you don't need NFS compatibility).
    - Nearly constant time to number of entities being locked.
    - Nearly constant time to number of processes concurrently using the lock (i.e. number of waiters).
    - Can sleep until a lock becomes free in a power-efficient manner.
    - Sudden power loss during use is recovered from.

    Caveats:
    - Much slower than byte_ranges for few waiters or small number of entities.
    - Sudden process exit with locks held will deadlock all other users.
    - Maximum of twelve entities may be locked concurrently.
    - Wasteful of disk space if used on a non-extents based filing system (e.g. FAT32, ext3).
    It is best used in `/tmp` if possible (`file_handle::temp_file()`). If you really must use a non-extents based filing
    system, destroy and recreate the object instance periodically to force resetting the lock
    file's length to zero.
    - Similarly older operating systems (e.g. Linux < 3.0) do not implement extent hole punching
    and therefore will also see excessive disk space consumption. Note at the time of writing
    OS X doesn't implement hole punching at all.
    - If your OS doesn't have sane byte range locks (OS X, BSD, older Linuxes) and multiple
    objects in your process use the same lock file, misoperation will occur. Use lock_files instead.

    \todo Implement hole punching once I port that code from LLFIO v1.
    \todo Decide on some resolution mechanism for sudden process exit.
    \todo There is a 1 out of 2^64-2 chance of unique id collision. It would be nice if we
    actually formally checked that our chosen unique id is actually unique.
    */
    class atomic_append : public shared_fs_mutex
    {
      file_handle _h;
      file_handle::extent_guard _guard;      // tags file so other users know they are not alone
      bool _nfs_compatibility;               // Do additional locking to work around NFS's lack of atomic append
      bool _skip_hashing;                    // Assume reads never can see torn writes
      uint64 _unique_id;                     // My (very random) unique id
      atomic_append_detail::header _header;  // Header as of the last time I read it

      atomic_append(file_handle &&h, file_handle::extent_guard &&guard, bool nfs_compatibility, bool skip_hashing)
          : _h(std::move(h))
          , _guard(std::move(guard))
          , _nfs_compatibility(nfs_compatibility)
          , _skip_hashing(skip_hashing)
          , _unique_id(0)
      {
        // guard now points at a non-existing handle
        _guard.set_handle(&_h);
        utils::random_fill(reinterpret_cast<char *>(&_unique_id), sizeof(_unique_id));  // NOLINT crypto strong random
        memset(&_header, 0, sizeof(_header));
        (void) _read_header();
      }

      result<void> _read_header()
      {
        bool first = true;
        do
        {
          file_handle::buffer_type req{reinterpret_cast<byte *>(&_header), 48};
          OUTCOME_TRY(auto &&_, _h.read({{&req, 1}, 0}));
          if(_[0].data() != reinterpret_cast<byte *>(&_header))
          {
            memcpy(&_header, _[0].data(), _[0].size());
          }
          if(_skip_hashing)
          {
            return success();
          }
          if(first)
          {
            first = false;
          }
          else
          {
            std::this_thread::yield();
          }
          // No timeout as this should very rarely block for any significant length of time
        } while(_header.hash != QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((reinterpret_cast<char *>(&_header)) + 16, sizeof(_header) - 16));
        return success();
      }

    public:
      //! The type of an entity id
      using entity_type = shared_fs_mutex::entity_type;
      //! The type of a sequence of entities
      using entities_type = shared_fs_mutex::entities_type;

      //! No copy construction
      atomic_append(const atomic_append &) = delete;
      //! No copy assignment
      atomic_append &operator=(const atomic_append &) = delete;
      ~atomic_append() = default;
      //! Move constructor
      atomic_append(atomic_append &&o) noexcept : _h(std::move(o._h)), _guard(std::move(o._guard)), _nfs_compatibility(o._nfs_compatibility), _skip_hashing(o._skip_hashing), _unique_id(o._unique_id), _header(o._header) { _guard.set_handle(&_h); }
      //! Move assign
      atomic_append &operator=(atomic_append &&o) noexcept
      {
        if(this == &o)
        {
          return *this;
        }
        this->~atomic_append();
        new(this) atomic_append(std::move(o));
        return *this;
      }

      /*! Initialises a shared filing system mutex using the file at \em lockfile

      \return An implementation of shared_fs_mutex using the atomic_append algorithm.
      \param base Optional base for the path to the file.
      \param lockfile The path to the file to use for IPC.
      \param nfs_compatibility Make this true if the lockfile could be accessed by NFS.
      \param skip_hashing Some filing systems (typically the copy on write ones e.g.
      ZFS, btrfs) guarantee atomicity of updates and therefore torn writes are never
      observed by readers. For these, hashing can be safely disabled.
      */
      LLFIO_MAKE_FREE_FUNCTION
      static result<atomic_append> fs_mutex_append(const path_handle &base, path_view lockfile, bool nfs_compatibility = false, bool skip_hashing = false) noexcept
      {
        LLFIO_LOG_FUNCTION_CALL(0);
        OUTCOME_TRY(auto &&ret, file_handle::file(base, lockfile, file_handle::mode::write, file_handle::creation::if_needed, file_handle::caching::temporary));
        atomic_append_detail::header header;
        // Lock the entire header for exclusive access
        auto lockresult = ret.lock_file_range(0, sizeof(header), lock_kind::exclusive, std::chrono::seconds(0));
        //! \todo fs_mutex_append needs to check if file still exists after lock is granted, awaiting path fetching.
        if(lockresult.has_error())
        {
          if(lockresult.error() != errc::timed_out)
          {
            return std::move(lockresult).error();
          }
          // Somebody else is also using this file
        }
        else
        {
          // I am the first person to be using this (stale?) file, so write a new header and truncate
          OUTCOME_TRYV(ret.truncate(sizeof(header)));
          memset(&header, 0, sizeof(header));
          header.time_offset = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
          header.first_known_good = sizeof(header);
          header.first_after_hole_punch = sizeof(header);
          if(!skip_hashing)
          {
            header.hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((reinterpret_cast<char *>(&header)) + 16, sizeof(header) - 16);
          }
          OUTCOME_TRYV(ret.write(0, {{reinterpret_cast<byte *>(&header), sizeof(header)}}));
        }
        // Open a shared lock on last byte in header to prevent other users zomping the file
        OUTCOME_TRY(auto &&guard, ret.lock_file_range(sizeof(header) - 1, 1, lock_kind::shared));
        // Unlock any exclusive lock I gained earlier now
        if(lockresult)
        {
          lockresult.value().unlock();
        }
        // The constructor will read and cache the header
        return atomic_append(std::move(ret), std::move(guard), nfs_compatibility, skip_hashing);
      }

      //! Return the handle to file being used for this lock
      const file_handle &handle() const noexcept { return _h; }

    protected:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> _lock(entities_guard &out, deadline d, bool spin_not_sleep) noexcept final
      {
        LLFIO_LOG_FUNCTION_CALL(this);
        atomic_append_detail::lock_request lock_request;
        if(out.entities.size() > sizeof(lock_request.entities) / sizeof(lock_request.entities[0]))
        {
          return errc::argument_list_too_long;
        }

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

        // Write my lock request immediately
        memset(&lock_request, 0, sizeof(lock_request));
        lock_request.unique_id = _unique_id;
        auto count = std::chrono::system_clock::now() - std::chrono::system_clock::from_time_t(_header.time_offset);
        lock_request.us_count = std::chrono::duration_cast<std::chrono::microseconds>(count).count();
        lock_request.items = out.entities.size();
        memcpy(lock_request.entities, out.entities.data(), sizeof(lock_request.entities[0]) * out.entities.size());
        if(!_skip_hashing)
        {
          lock_request.hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((reinterpret_cast<char *>(&lock_request)) + 16, sizeof(lock_request) - 16);
        }
        // My lock request will be the file's current length or higher
        OUTCOME_TRY(auto &&my_lock_request_offset, _h.maximum_extent());
        {
          OUTCOME_TRYV(_h.set_append_only(true));
          auto undo = make_scope_exit([this]() noexcept { (void) _h.set_append_only(false); });
          file_handle::extent_guard append_guard;
          if(_nfs_compatibility)
          {
            auto lastbyte = static_cast<file_handle::extent_type>(-1);
            // Lock up to the beginning of the shadow lock space
            lastbyte &= ~(1ULL << 63U);
            OUTCOME_TRY(auto &&append_guard_, _h.lock_file_range(my_lock_request_offset, lastbyte, lock_kind::exclusive));
            append_guard = std::move(append_guard_);
          }
          OUTCOME_TRYV(_h.write(0, {{reinterpret_cast<byte *>(&lock_request), sizeof(lock_request)}}));
        }

        // Find the record I just wrote
        alignas(64) byte _buffer[4096 + 2048];  // 6Kb cache line aligned buffer
        // Read onwards from length as reported before I wrote my lock request
        // until I find my lock request. This loop should never actually iterate
        // except under extreme load conditions.
        //! \todo Read from header.last_known_good immediately if possible in order
        //! to avoid a duplicate read later
        for(;;)
        {
          file_handle::buffer_type req{_buffer, sizeof(_buffer)};
          file_handle::io_result<file_handle::buffers_type> readoutcome = _h.read({{&req, 1}, my_lock_request_offset});
          // Should never happen :)
          if(readoutcome.has_error())
          {
            LLFIO_LOG_FATAL(this, "atomic_append::lock() saw an error when searching for just written data");
            std::terminate();
          }
          const atomic_append_detail::lock_request *record, *lastrecord;
          for(record = reinterpret_cast<const atomic_append_detail::lock_request *>(readoutcome.value()[0].data()), lastrecord = reinterpret_cast<const atomic_append_detail::lock_request *>(readoutcome.value()[0].data() + readoutcome.value()[0].size()); record < lastrecord && record->hash != lock_request.hash;
              ++record)
          {
            my_lock_request_offset += sizeof(atomic_append_detail::lock_request);
          }
          if(record->hash == lock_request.hash)
          {
            break;
          }
        }

        // extent_guard is now valid and will be unlocked on error
        out.hint = my_lock_request_offset;
        disableunlock.release();

        // Lock my request for writing so others can sleep on me
        file_handle::extent_guard my_request_guard;
        if(!spin_not_sleep)
        {
          auto lock_offset = my_lock_request_offset;
          // Set the top bit to use the shadow lock space on Windows
          lock_offset |= (1ULL << 63U);
          OUTCOME_TRY(auto &&my_request_guard_, _h.lock_file_range(lock_offset, sizeof(lock_request), lock_kind::exclusive));
          my_request_guard = std::move(my_request_guard_);
        }

        // Read every record preceding mine until header.first_known_good inclusive
        auto record_offset = my_lock_request_offset - sizeof(atomic_append_detail::lock_request);
        do
        {
        reload:
          // Refresh the header and load a snapshot of everything between record_offset
          // and first_known_good or -6Kb, whichever the sooner
          OUTCOME_TRYV(_read_header());
          // If there are no preceding records, we're done
          if(record_offset < _header.first_known_good)
          {
            break;
          }
          auto start_offset = record_offset;
          if(start_offset > sizeof(_buffer) - sizeof(atomic_append_detail::lock_request))
          {
            start_offset -= sizeof(_buffer) - sizeof(atomic_append_detail::lock_request);
          }
          else
          {
            start_offset = sizeof(atomic_append_detail::lock_request);
          }
          if(start_offset < _header.first_known_good)
          {
            start_offset = _header.first_known_good;
          }
          assert(record_offset >= start_offset);
          assert(record_offset - start_offset <= sizeof(_buffer));
          file_handle::buffer_type req{_buffer, (size_t)(record_offset - start_offset) + sizeof(atomic_append_detail::lock_request)};
          OUTCOME_TRY(auto &&batchread, _h.read({{&req, 1}, start_offset}));
          assert(batchread[0].size() == record_offset - start_offset + sizeof(atomic_append_detail::lock_request));
          const atomic_append_detail::lock_request *record = reinterpret_cast<atomic_append_detail::lock_request *>(batchread[0].data() + batchread[0].size() - sizeof(atomic_append_detail::lock_request));
          const atomic_append_detail::lock_request *firstrecord = reinterpret_cast<atomic_append_detail::lock_request *>(batchread[0].data());

          // Skip all completed lock requests or not mentioning any of my entities
          for(; record >= firstrecord; record_offset -= sizeof(atomic_append_detail::lock_request), --record)
          {
            // If a completed lock request, skip
            if(!record->hash && (record->unique_id == 0u))
            {
              continue;
            }
            // If record hash doesn't match contents it's a torn read, reload
            if(!_skip_hashing)
            {
              if(record->hash != QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((reinterpret_cast<const char *>(record)) + 16, sizeof(atomic_append_detail::lock_request) - 16))
              {
                goto reload;
              }
            }

            // Does this record lock anything I am locking?
            for(const auto &entity : out.entities)
            {
              for(size_t n = 0; n < record->items; n++)
              {
                if(record->entities[n].value == entity.value)
                {
                  // Is the lock I want exclusive or the lock he wants exclusive?
                  // If so, need to block
                  if((record->entities[n].exclusive != 0u) || (entity.exclusive != 0u))
                  {
                    goto beginwait;
                  }
                }
              }
            }
          }
          // None of this batch of records has anything to do with my request, so keep going
          continue;

        beginwait:
          // Sleep until this record is freed using a shared lock
          // on the record in our way. Note there is a race here
          // between when the lock requester writes the lock
          // request and when he takes an exclusive lock on it,
          // so if our shared lock succeeds we need to immediately
          // unlock and retry based on the data.
          std::this_thread::yield();
          if(!spin_not_sleep)
          {
            deadline nd;
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
            auto lock_offset = record_offset;
            // Set the top bit to use the shadow lock space on Windows
            lock_offset |= (1ULL << 63U);
            OUTCOME_TRYV(_h.lock_file_range(lock_offset, sizeof(*record), lock_kind::shared, nd));
          }
          // Make sure we haven't timed out during this wait
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
        } while(record_offset >= _header.first_known_good);
        return success();
      }

    public:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock(entities_type entities, unsigned long long hint) noexcept final
      {
        (void) entities;
        LLFIO_LOG_FUNCTION_CALL(this);
        if(hint == 0u)
        {
          LLFIO_LOG_WARN(this, "atomic_append::unlock() currently requires a hint to work, assuming this is a failed lock.");
          return;
        }
        auto my_lock_request_offset = static_cast<file_handle::extent_type>(hint);
        {
          atomic_append_detail::lock_request record;
#ifdef _DEBUG
          (void) _h.read(my_lock_request_offset, {{(byte *) &record, sizeof(record)}});
          if(!record.unique_id)
          {
            LLFIO_LOG_FATAL(this, "atomic_append::unlock() I have been previously unlocked!");
            std::terminate();
          }
          (void) _read_header();
          if(_header.first_known_good > my_lock_request_offset)
          {
            LLFIO_LOG_FATAL(this, "atomic_append::unlock() header exceeds the lock I am unlocking!");
            std::terminate();
          }
#endif
          memset(&record, 0, sizeof(record));
          (void) _h.write(my_lock_request_offset, {{reinterpret_cast<byte *>(&record), sizeof(record)}});
        }

        // Every 32 records or so, bump _header.first_known_good
        if((my_lock_request_offset & 4095U) == 0U)
        {
          //_read_header();

          // Forward scan records until first non-zero record is found
          // and update header with new info
          alignas(64) byte _buffer[4096 + 2048];
          bool done = false;
          while(!done)
          {
            file_handle::buffer_type req{_buffer, sizeof(_buffer)};
            auto bytesread_ = _h.read({{&req, 1}, _header.first_known_good});
            if(bytesread_.has_error())
            {
              // If distance between original first known good and end of file is exactly
              // 6Kb we can read an EOF
              break;
            }
            const auto &bytesread = bytesread_.value();
            // If read was partial, we are done after this round
            if(bytesread[0].size() < sizeof(_buffer))
            {
              done = true;
            }
            const auto *record = reinterpret_cast<const atomic_append_detail::lock_request *>(bytesread[0].data());
            const auto *lastrecord = reinterpret_cast<const atomic_append_detail::lock_request *>(bytesread[0].data() + bytesread[0].size());
            for(; record < lastrecord; ++record)
            {
              if(!record->hash && (record->unique_id == 0u))
              {
                _header.first_known_good += sizeof(atomic_append_detail::lock_request);
              }
              else
              {
                break;
              }
            }
          }
          // Hole punch if >= 1Mb of zeros exists
          if(_header.first_known_good - _header.first_after_hole_punch >= 1024U * 1024U)
          {
            handle::extent_type holepunchend = _header.first_known_good & ~(1024U * 1024U - 1);
#ifdef _DEBUG
            fprintf(stderr, "hole_punch(%llx, %llx)\n", _header.first_after_hole_punch, holepunchend - _header.first_after_hole_punch);
#endif
            _header.first_after_hole_punch = holepunchend;
          }
          ++_header.generation;
          if(!_skip_hashing)
          {
            _header.hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((reinterpret_cast<char *>(&_header)) + 16, sizeof(_header) - 16);
          }
          // Rewrite the first part of the header only
          (void) _h.write(0, {{reinterpret_cast<byte *>(&_header), 48}});
        }
      }
    };

  }  // namespace shared_fs_mutex
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if __GNUC__ >= 8
#pragma GCC diagnostic pop
#endif


#endif
