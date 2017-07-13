/* Information about a file
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (4 commits)
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

#ifndef BOOST_AFIO_STAT_H
#define BOOST_AFIO_STAT_H

#ifndef BOOST_AFIO_CONFIGURED
#error You must include the master afio.hpp, not individual header files directly
#endif
#include "config.hpp"

//! \file stat.hpp Provides stat

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

BOOST_AFIO_V2_NAMESPACE_EXPORT_BEGIN

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
struct stat_t
{
  uint64_t        st_dev;                            /*!< inode of device containing file (POSIX only) */
  uint64_t        st_ino;                            /*!< inode of file                   (Windows, POSIX) */
  stl1z::filesystem::file_type st_type;              /*!< type of file                    (Windows, POSIX) */
#ifndef _WIN32
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  uint16_t        st_perms;
#else
  stil1z::filesystem::perms st_perms;               /*!< uint16_t bitfield perms of file (POSIX only) */
#endif
#endif
  int16_t         st_nlink;                         /*!< number of hard links            (Windows, POSIX) */
#ifndef _WIN32
  int16_t         st_uid;                           /*!< user ID of the file             (POSIX only) */
  int16_t         st_gid;                           /*!< group ID of the file            (POSIX only) */
  dev_t           st_rdev;                          /*!< id of file if special           (POSIX only) */
#endif
  stl11::chrono::system_clock::time_point st_atim;  /*!< time of last access             (Windows, POSIX) */
  stl11::chrono::system_clock::time_point st_mtim;  /*!< time of last data modification  (Windows, POSIX) */
  stl11::chrono::system_clock::time_point st_ctim;  /*!< time of last status change      (Windows, POSIX) */
  handle::extent_type st_size;                      /*!< file size, in bytes             (Windows, POSIX) */
  handle::extent_type st_allocated;                 /*!< bytes allocated for file        (Windows, POSIX) */
  handle::extent_type st_blocks;                    /*!< number of blocks allocated      (Windows, POSIX) */
  uint16_t        st_blksize;                       /*!< block size used by this device  (Windows, POSIX) */
  uint32_t        st_flags;                         /*!< user defined flags for file     (FreeBSD, OS X, zero otherwise) */
  uint32_t        st_gen;                           /*!< file generation number          (FreeBSD, OS X, zero otherwise)*/
  stl11::chrono::system_clock::time_point st_birthtim; /*!< time of file creation           (Windows, FreeBSD, OS X, zero otherwise) */

  unsigned        st_sparse : 1;                   /*!< if this file is sparse, or this directory capable of sparse files (Windows, POSIX) */
  unsigned        st_compressed : 1;               /*!< if this file is compressed, or this directory capable of compressed files (Windows) */
  unsigned        st_reparse_point : 1;            /*!< if this file or directory is a reparse point (Windows) */
    
  //! Used to indicate what metadata should be filled in
  BOOSTLITE_BITFIELD_BEGIN(want) {
    dev=1<<0,
    ino=1<<1,
    type=1<<2,
    perms=1<<3,
    nlink=1<<4,
    uid=1<<5,
    gid=1<<6,
    rdev=1<<7,
    atim=1<<8,
    mtim=1<<9,
    ctim=1<<10,
    size=1<<11,
    allocated=1<<12,
    blocks=1<<13,
    blksize=1<<14,
    flags=1<<15,
    gen=1<<16,
    birthtim=1<<17,
    sparse=1<<24,
    compressed=1<<25,
    reparse_point=1<<26,
    all = (unsigned) -1
  }
  BOOSTLITE_BITFIELD_END(want)
  //! Constructs a UNINITIALIZED instance i.e. full of random garbage
  stat_t() noexcept { }
  //! Constructs a zeroed instance
  constexpr stat_t(std::nullptr_t) noexcept :
      st_dev(0),
      st_ino(0),
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
      st_type(stl1z::filesystem::file_type::type_unknown),
#else
      st_type(stl1z::filesystem::file_type::unknown),
#endif
#ifndef _WIN32
      st_perms(0),
#endif
      st_nlink(0),
#ifndef _WIN32
      st_uid(0), st_gid(0), st_rdev(0),
#endif
      st_size(0), st_allocated(0), st_blocks(0), st_blksize(0), st_flags(0), st_gen(0), st_sparse(0), st_compressed(0), st_reparse_point(0) { }
#ifdef __cpp_exceptions
  //! Constructs a filled instance, throwing as an exception any error which might occur
  stat_t(const handle &h, want wanted = want::all)
      : stat_t()
  {
    auto v(fill(h, wanted));
    if(v.has_error())
      throw std::system_error(v.get_error());
  }
#endif
  //! Fills in the structure with metadata, returning number of items filled in
  BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_t> fill(const handle &h, want wanted = want::all) noexcept;
};

BOOST_AFIO_V2_NAMESPACE_END

#if BOOST_AFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define BOOST_AFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/stat.ipp"
#else
#include "detail/impl/posix/stat.ipp"
#endif
#undef BOOST_AFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
