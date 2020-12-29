/* Information about the volume storing a file
(C) 2015-2020 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
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

#ifndef LLFIO_STATFS_H
#define LLFIO_STATFS_H

#ifndef LLFIO_CONFIG_HPP
#error You must include the master llfio.hpp, not individual header files directly
#endif
#include "config.hpp"

//! \file statfs.hpp Provides statfs

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

class handle;

namespace detail
{
  inline constexpr float constexpr_float_allbits_set_nan()
  {
#if defined(_MSC_VER) && !defined(__clang__)
    // Not all bits 1, but I can't see how to do better whilst inside constexpr
    return -NAN;
#else
    return -__builtin_nanf("0xffffff");  // all bits 1
#endif
  }
}  // namespace detail

/*! \struct statfs_t
\brief Metadata about a filing system. Unsupported entries are all bits set.

Note also that for some fields, a soft failure to read the requested value manifests
as all bits set. For example, `f_iosinprogress` might not be computable if the
filing system for your handle reports a `dev_t` from `fstat()` which does not
match anything in the system's disk hardware i/o stats. As this can be completely
benign (e.g. your handle is a socket), this is treated as a soft failure.

Note for `f_iosinprogress` and `f_iosbusytime` that support is not implemented yet
outside Microsoft Windows and Linux. Note also that for Linux, filing systems
spanning multiple hardware devices have undefined outcomes, whereas on Windows
you are given the average of the values for all underlying hardware devices.
Code donations improving the support for these items on Mac OS, FreeBSD and Linux
would be welcomed.
*/
struct LLFIO_DECL statfs_t
{
  static constexpr uint32_t _allbits1_32 = ~0U;
  static constexpr uint64_t _allbits1_64 = ~0ULL;
  static constexpr float _allbits1_float = detail::constexpr_float_allbits_set_nan();
  struct f_flags_t
  {
    uint32_t rdonly : 1;             //!< Filing system is read only                                      (Windows, POSIX)
    uint32_t noexec : 1;             //!< Filing system cannot execute programs                           (POSIX)
    uint32_t nosuid : 1;             //!< Filing system cannot superuser                                  (POSIX)
    uint32_t acls : 1;               //!< Filing system provides ACLs                                     (Windows, POSIX)
    uint32_t xattr : 1;              //!< Filing system provides extended attributes                      (Windows, POSIX)
    uint32_t compression : 1;        //!< Filing system provides whole volume compression                 (Windows, POSIX)
    uint32_t extents : 1;            //!< Filing system provides extent based file storage (sparse files) (Windows, POSIX)
    uint32_t filecompression : 1;    //!< Filing system provides per-file selectable compression          (Windows)
  } f_flags{0, 0, 0, 0, 0, 0, 0, 0}; /*!< copy of mount exported flags       (Windows, POSIX) */
  uint64_t f_bsize{_allbits1_64};    /*!< fundamental filesystem block size  (Windows, POSIX) */
  uint64_t f_iosize{_allbits1_64};   /*!< optimal transfer block size        (Windows, POSIX) */
  uint64_t f_blocks{_allbits1_64};   /*!< total data blocks in filesystem    (Windows, POSIX) */
  uint64_t f_bfree{_allbits1_64};    /*!< free blocks in filesystem          (Windows, POSIX) */
  uint64_t f_bavail{_allbits1_64};   /*!< free blocks avail to non-superuser (Windows, POSIX) */
  uint64_t f_files{_allbits1_64};    /*!< total file nodes in filesystem     (POSIX) */
  uint64_t f_ffree{_allbits1_64};    /*!< free nodes avail to non-superuser  (POSIX) */
  uint32_t f_namemax{_allbits1_32};  /*!< maximum filename length            (Windows, POSIX) */
#ifndef _WIN32
  int16_t f_owner{-1}; /*!< user that mounted the filesystem   (BSD, OS X) */
#endif
  uint64_t f_fsid[2]{_allbits1_64, _allbits1_64}; /*!< filesystem id                      (Windows, POSIX) */
  std::string f_fstypename;                       /*!< filesystem type name               (Windows, POSIX) */
  std::string f_mntfromname;                      /*!< mounted filesystem                 (Windows, POSIX) */
  filesystem::path f_mntonname;                   /*!< directory on which mounted         (Windows, POSIX) */

  uint32_t f_iosinprogress{_allbits1_32}; /*!< i/o's currently in progress (i.e. queue depth)  (Windows, Linux) */
  float f_iosbusytime{_allbits1_float};   /*!< percentage of time spent doing i/o (1.0 = 100%) (Windows, Linux) */

  //! Used to indicate what metadata should be filled in
  QUICKCPPLIB_BITFIELD_BEGIN(want){flags = 1 << 0,
                                   bsize = 1 << 1,
                                   iosize = 1 << 2,
                                   blocks = 1 << 3,
                                   bfree = 1 << 4,
                                   bavail = 1 << 5,
                                   files = 1 << 6,
                                   ffree = 1 << 7,
                                   namemax = 1 << 8,
                                   owner = 1 << 9,
                                   fsid = 1 << 10,
                                   fstypename = 1 << 11,
                                   mntfromname = 1 << 12,
                                   mntonname = 1 << 13,
                                   iosinprogress = 1 << 14,
                                   iosbusytime = 1 << 15,
                                   all = static_cast<unsigned>(-1)} QUICKCPPLIB_BITFIELD_END(want)
  //! Constructs a default initialised instance (all bits set)
  statfs_t()
  {
  }  // NOLINT  Cannot be constexpr due to lack of constexpe string default constructor :(
#ifdef __cpp_exceptions
  //! Constructs a filled instance, throwing as an exception any error which might occur
  explicit statfs_t(const handle &h, want wanted = want::all)
      : statfs_t()
  {
    auto v(fill(h, wanted));
    if(v.has_error())
    {
      v.error().throw_exception();
    }
  }
#endif
  //! Fills in the structure with metadata, returning number of items filled in
  LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_t> fill(const handle &h, want wanted = want::all) noexcept;

private:
  // Implemented in file_handle.ipp on Windows, otherwise in statfs.ipp
  static LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<std::pair<uint32_t, float>> _fill_ios(const handle &h, const std::string &mntfromname) noexcept;
};

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#ifdef _WIN32
#include "detail/impl/windows/statfs.ipp"
#else
#include "detail/impl/posix/statfs.ipp"
#endif
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
