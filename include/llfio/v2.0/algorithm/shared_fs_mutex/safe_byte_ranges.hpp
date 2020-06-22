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

#ifndef LLFIO_SHARED_FS_MUTEX_SAFE_BYTE_RANGES_HPP
#define LLFIO_SHARED_FS_MUTEX_SAFE_BYTE_RANGES_HPP

#if defined(_WIN32)
#include "byte_ranges.hpp"
LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace shared_fs_mutex
  {
    class safe_byte_ranges : public byte_ranges
    {
    public:
      using byte_ranges::byte_ranges;
      explicit safe_byte_ranges(byte_ranges &&o)
          : byte_ranges(std::move(o))
      {
      }
      //! Initialises a shared filing system mutex using the file at \em lockfile
      LLFIO_MAKE_FREE_FUNCTION
      static result<safe_byte_ranges> fs_mutex_safe_byte_ranges(const path_handle &base, path_view lockfile) noexcept
      {
        OUTCOME_TRY(auto &&v, byte_ranges::fs_mutex_byte_ranges(base, lockfile));
        return safe_byte_ranges(std::move(v));
      }
    };
  }  // namespace shared_fs_mutex
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#else

#include "base.hpp"

#include <memory>

//! \file safe_byte_ranges.hpp Provides algorithm::shared_fs_mutex::safe_byte_ranges

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  namespace shared_fs_mutex
  {
    namespace detail
    {
      LLFIO_HEADERS_ONLY_FUNC_SPEC result<std::shared_ptr<shared_fs_mutex>> inode_to_fs_mutex(const path_handle &base, path_view lockfile) noexcept;
    }

    /*! \class safe_byte_ranges
    \brief Safe many entity shared/exclusive file system based lock

    \sa byte_ranges

    POSIX requires that byte range locks have insane semantics, namely that:

    - Any fd closed on an inode must release all byte range locks on that inode for all
    other fds.
    - Threads replace each other's locks.

    This makes the use of byte range locks in modern code highly problematic.

    This implementation of `shared_fs_mutex` exclusively uses byte range locks on an inode
    for exclusion with other processes, but on POSIX only also layers on top:

    - Keeps a process-wide store of reference counted fd's per lock in the process, thus
    preventing byte range locks ever being dropped unexpectedly.
    - A process-local thread aware layer, allowing byte range locks to be held by a thread
    and excluding other threads as well as processes.
    - A thread may add an additional exclusive or shared lock on top of its existing
    exclusive or shared lock, but unlocks always unlock the exclusive lock first. This
    choice of behaviour is to match Microsoft Windows' behaviour to aid writing portable
    code.

    On Microsoft Windows `safe_byte_ranges` is typedefed to `byte_ranges`, as the Windows
    byte range locking API already works as described above.

    Benefits:

    - Compatible with networked file systems, though be cautious with older NFS.
    - Linear complexity to number of concurrent users.
    - Exponential complexity to number of entities being concurrently locked.
    - Does a reasonable job of trying to sleep the thread if any of the entities are locked.
    - Sudden process exit with lock held is recovered from.
    - Sudden power loss during use is recovered from.
    - Safe for multithreaded usage.

    Caveats:
    - When entities being locked is more than one, the algorithm places the contending lock at the
    front of the list during the randomisation after lock failure so we can sleep the thread until
    it becomes free. However, under heavy churn the thread will generally spin, consuming 100% CPU.
    - Byte range locks need to work properly on your system. Misconfiguring NFS or Samba
    to cause byte range locks to not work right will produce bad outcomes.
    - Unavoidably these locks will be a good bit slower than `byte_ranges`.
    */
    class safe_byte_ranges : public shared_fs_mutex
    {
      std::shared_ptr<shared_fs_mutex> _p;

      explicit safe_byte_ranges(std::shared_ptr<shared_fs_mutex> p)
          : _p(std::move(p))
      {
      }

    public:
      //! The type of an entity id
      using entity_type = shared_fs_mutex::entity_type;
      //! The type of a sequence of entities
      using entities_type = shared_fs_mutex::entities_type;

      //! No copy construction
      safe_byte_ranges(const safe_byte_ranges &) = delete;
      //! No copy assignment
      safe_byte_ranges &operator=(const safe_byte_ranges &) = delete;
      ~safe_byte_ranges() = default;
      //! Move constructor
      safe_byte_ranges(safe_byte_ranges &&o) noexcept : _p(std::move(o._p)) {}
      //! Move assign
      safe_byte_ranges &operator=(safe_byte_ranges &&o) noexcept
      {
        if(this == &o)
        {
          return *this;
        }
        _p = std::move(o._p);
        return *this;
      }

      //! Initialises a shared filing system mutex using the file at \em lockfile
      LLFIO_MAKE_FREE_FUNCTION
      static result<safe_byte_ranges> fs_mutex_safe_byte_ranges(const path_handle &base, path_view lockfile) noexcept
      {
        OUTCOME_TRY(auto &&ret, detail::inode_to_fs_mutex(base, lockfile));
        return safe_byte_ranges(std::move(ret));
      }

    protected:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> _lock(entities_guard &out, deadline d, bool spin_not_sleep) noexcept final { return _p->_lock(out, d, spin_not_sleep); }

    public:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock(entities_type entities, unsigned long long hint) noexcept final { return _p->unlock(entities, hint); }
    };

  }  // namespace shared_fs_mutex
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "../../detail/impl/safe_byte_ranges.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif

#endif
