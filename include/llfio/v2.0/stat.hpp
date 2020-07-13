/* Information about a file
(C) 2015-2018 Niall Douglas <http://www.nedproductions.biz/> (4 commits)
File Created: Apr 2017


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

#ifndef LLFIO_STAT_H
#define LLFIO_STAT_H

#ifndef LLFIO_CONFIG_HPP
#error You must include the master llfio.hpp, not individual header files directly
#endif
#include "config.hpp"

//! \file stat.hpp Provides stat

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class handle;

/*! \struct stat_t
\brief Metadata about a directory entry

This structure looks somewhat like a `struct stat`, and indeed it was derived from BSD's `struct stat`.
However there are a number of changes to better interoperate with modern practice, specifically:

- inode value containers are forced to 64 bits.
- Timestamps use C++11's `std::chrono::system_clock::time_point` or Boost equivalent. The resolution
of these may or may not equal what a `struct timespec` can do depending on your STL.
- The type of a file, which is available on Windows and on POSIX without needing an additional
syscall, is provided by `st_type` which is one of the values from `filesystem::file_type`.
- As type is now separate from permissions, there is no longer a `st_mode`, instead being a
`st_perms` which is solely the permissions bits. If you want to test permission bits in `st_perms`
but don't want to include platform specific headers, note that `filesystem::perms` contains
definitions of the POSIX permissions flags.
- The st_sparse and st_compressed flags indicate if your file is sparse and/or compressed, or if
the directory will compress newly created files by default. Note that on POSIX, a file is sparse
if and only if st_allocated < st_size which can include compressed files if that filing system is mounted
with compression enabled (e.g. ZFS with ZLE compression which elides runs of zeros).
- The st_reparse_point is a Windows only flag and is never set on POSIX, even on a NTFS volume.
*/
struct LLFIO_DECL stat_t  // NOLINT
{
  /* NOTE TO THOSE WHO WOULD MODIFY THIS:

  These members need to be non-initialised. Do NOT initialise them. We WANT uninitialised data here
  by default!
  */
  uint64_t st_dev;               /*!< inode of device containing file (POSIX only) */
  uint64_t st_ino;               /*!< inode of file                   (Windows, POSIX) */
  filesystem::file_type st_type; /*!< type of file                    (Windows, POSIX) */
#ifndef _WIN32
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  uint16_t st_perms;
#else
  stil1z::filesystem::perms st_perms; /*!< uint16_t bitfield perms of file (POSIX only) */
#endif
#endif
  int16_t st_nlink; /*!< number of hard links            (Windows, POSIX) */
#ifndef _WIN32
  int16_t st_uid; /*!< user ID of the file             (POSIX only) */
  int16_t st_gid; /*!< group ID of the file            (POSIX only) */
  dev_t st_rdev;  /*!< id of file if special           (POSIX only) */
#endif
  union
  {
    std::chrono::system_clock::time_point st_atim; /*!< time of last access             (Windows, POSIX) */
  };
  union
  {
    std::chrono::system_clock::time_point st_mtim; /*!< time of last data modification  (Windows, POSIX) */
  };
  union
  {
    std::chrono::system_clock::time_point st_ctim; /*!< time of last status change      (Windows, POSIX) */
  };
  handle::extent_type st_size;      /*!< file size, in bytes             (Windows, POSIX) */
  handle::extent_type st_allocated; /*!< bytes allocated for file        (Windows, POSIX) */
  handle::extent_type st_blocks;    /*!< number of blocks allocated      (Windows, POSIX) */
  uint16_t st_blksize;              /*!< block size used by this device  (Windows, POSIX) */
  uint32_t st_flags;                /*!< user defined flags for file     (FreeBSD, OS X, zero
       otherwise) */
  uint32_t st_gen;                  /*!< file generation number          (FreeBSD, OS X, zero
       otherwise)*/
  union
  {
    std::chrono::system_clock::time_point st_birthtim; /*!< time of file creation           (Windows, POSIX) */
  };
  static_assert(std::is_trivially_destructible<std::chrono::system_clock::time_point>::value,
                "std::chrono::system_clock::time_point is not trivally destructible!");

  unsigned st_sparse : 1;        /*!< if this file is sparse, or this directory capable of sparse files (Windows, POSIX) */
  unsigned st_compressed : 1;    /*!< if this file is compressed, or this directory capable of compressed files (Windows, Linux) */
  unsigned st_reparse_point : 1; /*!< if this file or directory is a reparse point (Windows) */

  //! Used to indicate what metadata should be filled in
  QUICKCPPLIB_BITFIELD_BEGIN(want){dev = 1 << 0,
                                   ino = 1 << 1,
                                   type = 1 << 2,
                                   perms = 1 << 3,
                                   nlink = 1 << 4,
                                   uid = 1 << 5,
                                   gid = 1 << 6,
                                   rdev = 1 << 7,
                                   atim = 1 << 8,
                                   mtim = 1 << 9,
                                   ctim = 1 << 10,
                                   size = 1 << 11,
                                   allocated = 1 << 12,
                                   blocks = 1 << 13,
                                   blksize = 1 << 14,
                                   flags = 1 << 15,
                                   gen = 1 << 16,
                                   birthtim = 1 << 17,
                                   sparse = 1 << 24,
                                   compressed = 1 << 25,
                                   reparse_point = 1 << 26,
                                   all = static_cast<unsigned>(-1),
                                   none = 0} QUICKCPPLIB_BITFIELD_END(want)
  //! Constructs a UNINITIALIZED instance i.e. full of random garbage
  stat_t()
  {
  }  // NOLINT   CANNOT be constexpr because we are INTENTIONALLY not initialising the storage
  //! Constructs a zeroed instance
  constexpr explicit stat_t(std::nullptr_t) noexcept
      : st_dev(0)                                // NOLINT
      , st_ino(0)                                // NOLINT
      , st_type(filesystem::file_type::unknown)  // NOLINT
#ifndef _WIN32
      , st_perms(0)  // NOLINT
#endif
      , st_nlink(0)  // NOLINT
#ifndef _WIN32
      , st_uid(0)   // NOLINT
      , st_gid(0)   // NOLINT
      , st_rdev(0)  // NOLINT
#endif
      , st_atim{}            // NOLINT
      , st_mtim{}            // NOLINT
      , st_ctim{}            // NOLINT
      , st_size(0)           // NOLINT
      , st_allocated(0)      // NOLINT
      , st_blocks(0)         // NOLINT
      , st_blksize(0)        // NOLINT
      , st_flags(0)          // NOLINT
      , st_gen(0)            // NOLINT
      , st_birthtim{}        // NOLINT
      , st_sparse(0)         // NOLINT
      , st_compressed(0)     // NOLINT
      , st_reparse_point(0)  // NOLINT
  {
  }
#ifdef __cpp_exceptions
  //! Constructs a filled instance, throwing as an exception any error which might occur
  explicit stat_t(const handle &h, want wanted = want::all)
      : stat_t(nullptr)
  {
    auto v(fill(h, wanted));
    if(v.has_error())
    {
      v.error().throw_exception();
    }
  }
#endif
  //! Equality comparison
  bool operator==(const stat_t &o) const noexcept
  {
    return st_dev == o.st_dev && st_ino == o.st_ino && st_type == o.st_type
#ifndef _WIN32
           && st_perms == o.st_perms
#endif
           && st_nlink == o.st_nlink
#ifndef _WIN32
           && st_uid == o.st_uid && st_gid == o.st_gid && st_rdev == o.st_rdev
#endif
           && st_atim == o.st_atim && st_mtim == o.st_mtim && st_ctim == o.st_ctim && st_size == o.st_size && st_allocated == o.st_allocated &&
           st_blocks == o.st_blocks && st_blksize == o.st_blksize && st_flags == o.st_flags && st_gen == o.st_gen && st_birthtim == o.st_birthtim &&
           st_sparse == o.st_sparse && st_compressed == o.st_compressed && st_reparse_point == o.st_reparse_point;
  }
  //! Inequality comparison
  bool operator!=(const stat_t &o) const noexcept
  {
    return st_dev != o.st_dev || st_ino != o.st_ino || st_type != o.st_type
#ifndef _WIN32
           || st_perms != o.st_perms
#endif
           || st_nlink != o.st_nlink
#ifndef _WIN32
           || st_uid != o.st_uid || st_gid != o.st_gid || st_rdev != o.st_rdev
#endif
           || st_atim != o.st_atim || st_mtim != o.st_mtim || st_ctim != o.st_ctim || st_size != o.st_size || st_allocated != o.st_allocated ||
           st_blocks != o.st_blocks || st_blksize != o.st_blksize || st_flags != o.st_flags || st_gen != o.st_gen || st_birthtim != o.st_birthtim ||
           st_sparse != o.st_sparse || st_compressed != o.st_compressed || st_reparse_point != o.st_reparse_point;
  }
  //! Ordering
  bool operator<(const stat_t &o) const noexcept
  {
    if(st_dev != o.st_dev)
    {
      return st_dev < o.st_dev;
    }
    if(st_ino != o.st_ino)
    {
      return st_ino < o.st_ino;
    }
    if(st_type != o.st_type)
    {
      return st_type < o.st_type;
    }
#ifndef _WIN32
    if(st_perms != o.st_perms)
    {
      return st_perms < o.st_perms;
    }
#endif
    if(st_nlink != o.st_nlink)
    {
      return st_nlink < o.st_nlink;
    }
#ifndef _WIN32
    if(st_uid != o.st_uid)
    {
      return st_uid < o.st_uid;
    }
    if(st_gid != o.st_gid)
    {
      return st_gid < o.st_gid;
    }
    if(st_rdev != o.st_rdev)
    {
      return st_rdev < o.st_rdev;
    }
#endif
    if(st_atim != o.st_atim)
    {
      return st_atim < o.st_atim;
    }
    if(st_mtim != o.st_mtim)
    {
      return st_mtim < o.st_mtim;
    }
    if(st_ctim != o.st_ctim)
    {
      return st_ctim < o.st_ctim;
    }
    if(st_size != o.st_size)
    {
      return st_size < o.st_size;
    }
    if(st_allocated != o.st_allocated)
    {
      return st_allocated < o.st_allocated;
    }
    if(st_blocks != o.st_blocks)
    {
      return st_blocks < o.st_blocks;
    }
    if(st_blksize != o.st_blksize)
    {
      return st_blksize < o.st_blksize;
    }
    if(st_flags != o.st_flags)
    {
      return st_flags < o.st_flags;
    }
    if(st_gen != o.st_gen)
    {
      return st_gen < o.st_gen;
    }
    if(st_birthtim != o.st_birthtim)
    {
      return st_birthtim < o.st_birthtim;
    }
    if(st_sparse != o.st_sparse)
    {
      return st_sparse < o.st_sparse;
    }
    if(st_compressed != o.st_compressed)
    {
      return st_compressed < o.st_compressed;
    }
    if(st_reparse_point != o.st_reparse_point)
    {
      return st_reparse_point < o.st_reparse_point;
    }
    return false;
  }

  /*! Fills the structure with metadata.

  \return The number of items filled in. You should use a nullptr constructed structure if you wish
  to detect which items were filled in, and which not (those not may be all bits zero).
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_t> fill(const handle &h, want wanted = want::all) noexcept;
  /*! Stamps the handle with the metadata in the structure, returning the metadata written.

  The following want bits are always ignored, and are cleared in the want bits returned:
  - `dev`
  - `ino`
  - `type`
  - `nlink`
  - `rdev`
  - `ctim`
  - `size` (use `truncate()` on the file instead)
  - `allocated`
  - `blocks`
  - `blksize`
  - `flags`
  - `gen`
  - `sparse`
  - `compressed`
  - `reparse_point`

  The following want bits are supported by these platforms:
  - `perms`, `uid`, `gid`  (POSIX only)
  - `atim`                 (Windows, POSIX)
  - `mtim`                 (Windows, POSIX)
  - `birthtim`             (Windows, POSIX)

  Note that on POSIX, setting birth time involves two syscalls, the first of which
  temporarily sets the modified date to the birth time, which is racy. This is
  unavoidable given the syscall's design.

  Note also that on POSIX one can never make a birth time newer than the current
  birth time, nor a modified time older than a birth time. You can do these on
  Windows, however.
  */
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<want> stamp(handle &h, want wanted = want::all) noexcept;
};
static_assert(std::is_trivially_copyable<stat_t>::value, "stat_t is not trivally copyable!");

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/stat.ipp"
#else
#include "detail/impl/posix/stat.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
