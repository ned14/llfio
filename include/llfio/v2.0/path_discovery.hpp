/* Discovery of various useful filesystem paths
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Sept 2017


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

#ifndef LLFIO_PATH_DISCOVERY_H
#define LLFIO_PATH_DISCOVERY_H

#include "fs_handle.hpp"
#include "stat.hpp"

//! \file path_discovery.hpp Provides `path_discovery`

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

//! \brief Contains functions used to discover suitable paths for things
namespace path_discovery
{
  //! \brief A discovered path.
  struct discovered_path
  {
    path_view path;  //!< The path discovered.
    //! Source of the discovered path.
    enum class source_type
    {
      unknown,      //!< This path comes from an unknown source.
      local,        //!< This path was added locally.
      environment,  //!< This path came from an environment variable (an override?).
      system,       //!< This path came from querying the system.
      hardcoded     //!< This path came from an internal hardcoded list of paths likely for this system.
    } source{source_type::unknown};

    /*! If this path was successfully probed for criteria verification, this was its stat after any symlink
    derefencing at that time. Secure applications ought to verify that any handles opened to the path have
    the same `st_ino` and `st_dev` as this structure before use.
    */
    optional<stat_t> stat;
  };
  inline std::ostream &operator<<(std::ostream &s, const discovered_path::source_type &v)
  {
    static constexpr const char *values[] = {"unknown", "local", "environment", "system", "hardcoded"};
    if(static_cast<size_t>(v) > sizeof(values) / sizeof(values[0]) || (values[static_cast<size_t>(v)] == nullptr))
    {
      return s << "llfio::path_discovery::discovered_path::source_type::<unknown>";
    }
    return s << "llfio::path_discovery::discovered_path::source_type::" << values[static_cast<size_t>(v)];
  }

  /*! \brief Returns a list of potential directories which might be usuable for temporary files.

  This is a fairly lightweight call which builds a master list of all potential temporary file directories
  given the environment block of this process (unless SUID or SGID or Privilege Elevation are in effect) and the user
  running this process. It does not verify if any of them exist, or are writable, or anything else about them.
  An internal mutex is held for the duration of this call.

  \mallocs Allocates the master list of discovered temporary directories exactly once per process,
  unless `refresh` is true in which case the list will be refreshed. The system calls to retrieve paths
  may allocate additional memory for paths returned.
  \errors This call never fails, except to return an empty span.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC span<discovered_path> all_temporary_directories(bool refresh = false) noexcept;

  /*! \brief Returns a subset of `all_temporary_directories()` each of which has been tested to be writable
  by the current process. No testing is done of available writable space.

  After this call returns, the successfully probed entries returned by `all_temporary_directories()` will have their
  stat structure set. As the probing involves creating a non-zero sized file in each possible temporary
  directory to verify its validity, this is not a fast call. It is however cached statically, so the
  cost occurs exactly once per process, unless someone calls `all_temporary_directories(true)` to wipe and refresh
  the master list. An internal mutex is held for the duration of this call.
  \mallocs None.
  \errors This call never fails, though if it fails to find any writable temporary directory, it will
  terminate the process.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC span<discovered_path> verified_temporary_directories() noexcept;

  /*! \brief Returns a reference to an open handle to a verified temporary directory where files created are
  stored in a filesystem directory, usually under the current user's quota.

  This is implemented by iterating all of the paths returned by `verified_temporary_directories()`
  and checking what file system is in use. The following regex is used:

  `btrfs|cifs|exfat|ext(2|3|4)|f2fs|hfs|apfs|jfs|lxfs|nfs|nilf2|ufs|vfat|xfs|zfs|msdosfs|newnfs|ntfs|smbfs|unionfs|fat|fat32`

  The handle is created during `verified_temporary_directories()` and is statically cached thereafter.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC const path_handle &storage_backed_temporary_files_directory() noexcept;

  /*! \brief Returns a reference to an open handle to a verified temporary directory where files created are
  stored in memory/paging file, and thus access may be a lot quicker, but stronger limits on
  capacity may apply.

  This is implemented by iterating all of the paths returned by `verified_temporary_directories()`
  and checking what file system is in use. The following regex is used:

  `tmpfs|ramfs`

  The handle is created during `verified_temporary_directories()` and is statically cached thereafter.

  \note If you wish to create an anonymous memory-backed inode for mmap and paging tricks like mapping
  the same extent into multiple addresses e.g. to implement a constant time zero copy `realloc()`,
  strongly consider using a non-file-backed `section_handle` as this is more portable.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC const path_handle &memory_backed_temporary_files_directory() noexcept;

  /*! \brief Returns a reference to an open handle to a verified temporary directory where named
  pipes may be created and found.

  On Microsoft Windows, this is `\Device\NamedPipe` within the NT kernel namespace.

  On POSIX, this is `storage_backed_temporary_files_directory()`.
  */
  LLFIO_HEADERS_ONLY_FUNC_SPEC const path_handle &temporary_named_pipes_directory() noexcept;

}  // namespace path_discovery

LLFIO_V2_NAMESPACE_END

// .ipp is included by file_handle.hpp if in header only mode

#endif
