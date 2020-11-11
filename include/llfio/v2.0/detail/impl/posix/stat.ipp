/* Information about a file
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (3 commits)
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

#include "../../../handle.hpp"
#include "../../../stat.hpp"

#include <sys/stat.h>

#ifdef __linux__
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<void> stat_from_symlink(struct stat &s, const handle &h) noexcept;
}

static inline filesystem::file_type to_st_type(uint16_t mode)
{
  switch(mode & S_IFMT)
  {
  case S_IFBLK:
    return filesystem::file_type::block;
  case S_IFCHR:
    return filesystem::file_type::character;
  case S_IFDIR:
    return filesystem::file_type::directory;
  case S_IFIFO:
    return filesystem::file_type::fifo;
  case S_IFLNK:
    return filesystem::file_type::symlink;
  case S_IFREG:
    return filesystem::file_type::regular;
  case S_IFSOCK:
    return filesystem::file_type::socket;
  default:
    return filesystem::file_type::unknown;
  }
}

static inline std::chrono::system_clock::time_point to_timepoint(struct timespec ts)
{
  // Need to have this self-adapt to the STL being used
  static constexpr unsigned long long STL_TICKS_PER_SEC =
  static_cast<unsigned long long>(std::chrono::system_clock::period::den) / std::chrono::system_clock::period::num;
  static constexpr unsigned long long multiplier = STL_TICKS_PER_SEC >= 1000000000ULL ? STL_TICKS_PER_SEC / 1000000000ULL : 1;
  static constexpr unsigned long long divider = STL_TICKS_PER_SEC >= 1000000000ULL ? 1 : 1000000000ULL / STL_TICKS_PER_SEC;
  // For speed we make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
  std::chrono::system_clock::duration duration(ts.tv_sec * STL_TICKS_PER_SEC + ts.tv_nsec * multiplier / divider);
  return std::chrono::system_clock::time_point(duration);
}
inline struct timespec from_timepoint(std::chrono::system_clock::time_point time)
{
  // Need to have this self-adapt to the STL being used
  static constexpr unsigned long long STL_TICKS_PER_SEC =
  static_cast<unsigned long long>(std::chrono::system_clock::period::den) / std::chrono::system_clock::period::num;
  static constexpr unsigned long long multiplier = STL_TICKS_PER_SEC >= 1000000000ULL ? STL_TICKS_PER_SEC / 1000000000ULL : 1;
  static constexpr unsigned long long divider = STL_TICKS_PER_SEC >= 1000000000ULL ? 1 : 1000000000ULL / STL_TICKS_PER_SEC;
  // For speed we make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
  std::chrono::system_clock::duration duration(time.time_since_epoch());
  return {static_cast<time_t>(duration.count() / STL_TICKS_PER_SEC), static_cast<long int>((duration.count() % STL_TICKS_PER_SEC) * divider / multiplier)};
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_t> stat_t::fill(const handle &h, stat_t::want wanted) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(&h);
  size_t ret = 0;
#ifdef __linux__
  {
    struct statx_timestamp
    {
      int64_t tv_sec;   /* Seconds since the Epoch (UNIX time) */
      uint32_t tv_nsec; /* Nanoseconds since tv_sec */
      uint32_t __reserved;
    };
    struct statx
    {
      uint32_t stx_mask;       /* Mask of bits indicating
                               filled fields */
      uint32_t stx_blksize;    /* Block size for filesystem I/O */
      uint64_t stx_attributes; /* Extra file attribute indicators */
      uint32_t stx_nlink;      /* Number of hard links */
      uint32_t stx_uid;        /* User ID of owner */
      uint32_t stx_gid;        /* Group ID of owner */
      uint16_t stx_mode;       /* File type and mode */
      uint16_t __spare0[1];
      uint64_t stx_ino;    /* Inode number */
      uint64_t stx_size;   /* Total size in bytes */
      uint64_t stx_blocks; /* Number of 512B blocks allocated */
      uint64_t stx_attributes_mask;
      /* Mask to show what's supported
         in stx_attributes */

      /* The following fields are file timestamps */
      struct statx_timestamp stx_atime; /* Last access */
      struct statx_timestamp stx_btime; /* Creation */
      struct statx_timestamp stx_ctime; /* Last status change */
      struct statx_timestamp stx_mtime; /* Last modification */

      /* If this file represents a device, then the next two
         fields contain the ID of the device */
      uint32_t stx_rdev_major; /* Major ID */
      uint32_t stx_rdev_minor; /* Minor ID */

      /* The next two fields contain the ID of the device
         containing the filesystem where the file resides */
      uint32_t stx_dev_major; /* Major ID */
      uint32_t stx_dev_minor; /* Minor ID */

      uint64_t __spare2[14];
    } s;
    memset(&s, 0, sizeof(s));
    unsigned mask = 0;
    if(wanted & want::dev)
    {
      mask |= 0x0100U /*STATX_INO*/;
    }
    if(wanted & want::ino)
    {
      mask |= 0x0100U /*STATX_INO*/;
    }
    if(wanted & want::type)
    {
      mask |= 0x0001U /*STATX_TYPE*/;
    }
    if(wanted & want::perms)
    {
      mask |= 0x0002U /*STATX_MODE*/;
    }
    if(wanted & want::nlink)
    {
      mask |= 0x0004U /*STATX_NLINK*/;
    }
    if(wanted & want::uid)
    {
      mask |= 0x0008U /*STATX_UID*/;
    }
    if(wanted & want::gid)
    {
      mask |= 0x0010U /*STATX_GID*/;
    }
    if(wanted & want::rdev)
    {
      mask |= 0x0100U /*STATX_INO*/;
    }
    if(wanted & want::atim)
    {
      mask |= 0x0020U /*STATX_ATIME*/;
    }
    if(wanted & want::mtim)
    {
      mask |= 0x0040U /*STATX_MTIME*/;
    }
    if(wanted & want::ctim)
    {
      mask |= 0x0080U /*STATX_CTIME*/;
    }
    if(wanted & want::size)
    {
      mask |= 0x0200U /*STATX_SIZE*/;
    }
    if(wanted & want::allocated)
    {
      mask |= 0x0200U /*STATX_SIZE*/;
    }
    if(wanted & want::blocks)
    {
      mask |= 0x0400U /*STATX_BLOCKS*/;
    }
    if(wanted & want::blksize)
    {
      mask |= 0x0400U /*STATX_BLOCKS*/;
    }
    if(wanted & want::birthtim)
    {
      mask |= 0x0800U /*STATX_BTIME*/;
    }
    int flags = AT_EMPTY_PATH | AT_NO_AUTOMOUNT | AT_SYMLINK_NOFOLLOW | 0x0000 /*AT_STATX_SYNC_AS_STAT*/;
    int fd = h.native_handle().fd;
    if(
#if defined __aarch64__
    syscall(291 /*__NR_statx*/, fd, "", flags, mask, &s)
#elif defined __arm__
    syscall(397 /*__NR_statx*/, fd, "", flags, mask, &s)
#elif defined __alpha__
    syscall(522 /*__NR_statx*/, fd, "", flags, mask, &s)
#elif defined __i386__ || defined __powerpc64__
    syscall(383 /*__NR_statx*/, fd, "", flags, mask, &s)
#elif defined __sparc__
    syscall(360 /*__NR_statx*/, fd, "", flags, mask, &s)
#elif defined __x86_64__
    syscall(332 /*__NR_statx*/, fd, "", flags, mask, &s)
#else
#error Unknown Linux platform
#endif
    >= 0)
    {
      if(wanted & want::dev)
      {
        st_dev = makedev(s.stx_dev_major, s.stx_dev_minor);
        ++ret;
      }
      if(wanted & want::ino)
      {
        st_ino = s.stx_ino;
        ++ret;
      }
      if(wanted & want::type)
      {
        st_type = to_st_type(s.stx_mode);
        ++ret;
      }
      if(wanted & want::perms)
      {
        st_perms = s.stx_mode & 0xfff;
        ++ret;
      }
      if(wanted & want::nlink)
      {
        st_nlink = s.stx_nlink;
        ++ret;
      }
      if(wanted & want::uid)
      {
        st_uid = s.stx_uid;
        ++ret;
      }
      if(wanted & want::gid)
      {
        st_gid = s.stx_gid;
        ++ret;
      }
      if(wanted & want::rdev)
      {
        st_rdev = makedev(s.stx_rdev_major, s.stx_rdev_minor);
        ++ret;
      }
      if(wanted & want::atim)
      {
        st_atim = to_timepoint(timespec{(time_t) s.stx_atime.tv_sec, (long) s.stx_atime.tv_nsec});
        ++ret;
      }
      if(wanted & want::mtim)
      {
        st_mtim = to_timepoint(timespec{(time_t) s.stx_mtime.tv_sec, (long) s.stx_mtime.tv_nsec});
        ++ret;
      }
      if(wanted & want::ctim)
      {
        st_ctim = to_timepoint(timespec{(time_t) s.stx_ctime.tv_sec, (long) s.stx_ctime.tv_nsec});
        ++ret;
      }
      if(wanted & want::size)
      {
        st_size = s.stx_size;
        ++ret;
      }
      if(wanted & want::allocated)
      {
        st_allocated = static_cast<handle::extent_type>(s.stx_blocks) * 512;
        ++ret;
      }
      if(wanted & want::blocks)
      {
        st_blocks = s.stx_blocks;
        ++ret;
      }
      if(wanted & want::blksize)
      {
        st_blksize = s.stx_blksize;
        ++ret;
      }
      if(wanted & want::birthtim)
      {
        st_birthtim = to_timepoint(timespec{(time_t) s.stx_btime.tv_sec, (long) s.stx_btime.tv_nsec});
        ++ret;
      }
      if(wanted & want::sparse)
      {
        st_sparse = static_cast<unsigned int>((static_cast<handle::extent_type>(s.stx_blocks) * 512) < static_cast<handle::extent_type>(s.stx_size));
        ++ret;
      }
      if(wanted & want::compressed)
      {
        st_compressed = static_cast<unsigned int>(s.stx_attributes & 0x0004 /*STATX_ATTR_COMPRESSED*/);
        ++ret;
      }
      return ret;
    }
    // std::cerr << "statx failed with " << strerror(errno) << std::endl;
  }
#endif
  {
    struct stat s
    {
    };
    memset(&s, 0, sizeof(s));

    if(-1 == ::fstat(h.native_handle().fd, &s))
    {
      if(!h.is_symlink() || EBADF != errno)
      {
        return posix_error();
      }
      // This is a hack, but symlink_handle includes this first so there is a chicken and egg dependency problem
      OUTCOME_TRY(detail::stat_from_symlink(s, h));
    }
    if(wanted & want::dev)
    {
      st_dev = s.st_dev;
      ++ret;
    }
    if(wanted & want::ino)
    {
      st_ino = s.st_ino;
      ++ret;
    }
    if(wanted & want::type)
    {
      st_type = to_st_type(s.st_mode);
      ++ret;
    }
    if(wanted & want::perms)
    {
      st_perms = s.st_mode & 0xfff;
      ++ret;
    }
    if(wanted & want::nlink)
    {
      st_nlink = s.st_nlink;
      ++ret;
    }
    if(wanted & want::uid)
    {
      st_uid = s.st_uid;
      ++ret;
    }
    if(wanted & want::gid)
    {
      st_gid = s.st_gid;
      ++ret;
    }
    if(wanted & want::rdev)
    {
      st_rdev = s.st_rdev;
      ++ret;
    }
#ifdef __ANDROID__
    if(wanted & want::atim)
    {
      st_atim = to_timepoint(*((struct timespec *) &s.st_atime));
      ++ret;
    }
    if(wanted & want::mtim)
    {
      st_mtim = to_timepoint(*((struct timespec *) &s.st_mtime));
      ++ret;
    }
    if(wanted & want::ctim)
    {
      st_ctim = to_timepoint(*((struct timespec *) &s.st_ctime));
      ++ret;
    }
#elif defined(__APPLE__)
    if(wanted & want::atim)
    {
      st_atim = to_timepoint(s.st_atimespec);
      ++ret;
    }
    if(wanted & want::mtim)
    {
      st_mtim = to_timepoint(s.st_mtimespec);
      ++ret;
    }
    if(wanted & want::ctim)
    {
      st_ctim = to_timepoint(s.st_ctimespec);
      ++ret;
    }
#else  // Linux and BSD
    if(wanted & want::atim)
    {
      st_atim = to_timepoint(s.st_atim);
      ++ret;
    }
    if(wanted & want::mtim)
    {
      st_mtim = to_timepoint(s.st_mtim);
      ++ret;
    }
    if(wanted & want::ctim)
    {
      st_ctim = to_timepoint(s.st_ctim);
      ++ret;
    }
#endif
    if(wanted & want::size)
    {
      st_size = s.st_size;
      ++ret;
    }
    if(wanted & want::allocated)
    {
      st_allocated = static_cast<handle::extent_type>(s.st_blocks) * 512;
      ++ret;
    }
    if(wanted & want::blocks)
    {
      st_blocks = s.st_blocks;
      ++ret;
    }
    if(wanted & want::blksize)
    {
      st_blksize = s.st_blksize;
      ++ret;
    }
#ifdef HAVE_STAT_FLAGS
    if(wanted & want::flags)
    {
      st_flags = s.st_flags;
      ++ret;
    }
#endif
#ifdef HAVE_STAT_GEN
    if(wanted & want::gen)
    {
      st_gen = s.st_gen;
      ++ret;
    }
#endif
#ifdef HAVE_BIRTHTIMESPEC
#if defined(__APPLE__)
    if(wanted & want::birthtim)
    {
      st_birthtim = to_timepoint(s.st_birthtimespec);
      ++ret;
    }
#else
    if(wanted & want::birthtim)
    {
      st_birthtim = to_timepoint(s.st_birthtim);
      ++ret;
    }
#endif
#endif
    if(wanted & want::sparse)
    {
      st_sparse = static_cast<unsigned int>((static_cast<handle::extent_type>(s.st_blocks) * 512) < static_cast<handle::extent_type>(s.st_size));
      ++ret;
    }
    return ret;
  }
}

LLFIO_HEADERS_ONLY_MEMFUNC_SPEC result<stat_t::want> stat_t::stamp(handle &h, stat_t::want wanted) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(&h);
  // Filter out the flags we don't support
  wanted &= (want::perms | want::uid | want::gid | want::atim | want::mtim
#ifdef HAVE_BIRTHTIMESPEC
             | want::birthtim
#endif
  );
  if(!wanted)
  {
    return wanted;
  }
  if(wanted & want::perms)
  {
    if(-1 == ::fchmod(h.native_handle().fd, st_perms))
    {
      return posix_error();
    }
  }
  if(wanted & (want::uid | want::gid))
  {
    if(-1 == ::fchown(h.native_handle().fd, (wanted & want::uid) ? st_uid : -1, (wanted & want::gid) ? st_gid : -1))
    {
      return posix_error();
    }
  }
  struct timespec times[2] = {{0, UTIME_OMIT}, {0, UTIME_OMIT}};
  if(wanted & want::atim)
  {
    times[0] = from_timepoint(st_atim);
  }
  if(wanted & want::mtim)
  {
    times[1] = from_timepoint(st_mtim);
  }
  if(wanted & want::birthtim)
  {
    if(!(wanted & want::mtim))
    {
      // Need to back up last modified time so it gets restored after
      struct stat s;
      if(-1 == ::fstat(h.native_handle().fd, &s))
      {
        return posix_error();
      }
#ifdef __ANDROID__
      times[1] = *((struct timespec *) &s.st_mtime);
#elif defined(__APPLE__)
      times[1] = s.st_mtimespec;
#else  // Linux and BSD
      times[1] = s.st_mtim;
#endif
    }
    // Set the modified date to the birth date, later we'll restore/set the modified date
    struct timespec btimes[2] = {{0, UTIME_OMIT}, from_timepoint(st_birthtim)};
    if(-1 == ::futimens(h.native_handle().fd, btimes))
    {
      return posix_error();
    }
  }
  if(wanted & (want::atim | want::mtim | want::birthtim))
  {
    if(-1 == ::futimens(h.native_handle().fd, times))
    {
      return posix_error();
    }
  }
  return wanted;
}

LLFIO_V2_NAMESPACE_END
