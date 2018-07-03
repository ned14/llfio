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

AFIO_V2_NAMESPACE_BEGIN

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
  static constexpr unsigned long long STL_TICKS_PER_SEC = static_cast<unsigned long long>(std::chrono::system_clock::period::den) / std::chrono::system_clock::period::num;
  static constexpr unsigned long long multiplier = STL_TICKS_PER_SEC >= 1000000000ULL ? STL_TICKS_PER_SEC / 1000000000ULL : 1;
  static constexpr unsigned long long divider = STL_TICKS_PER_SEC >= 1000000000ULL ? 1 : 1000000000ULL / STL_TICKS_PER_SEC;
  // For speed we make the big assumption that the STL's system_clock is based on the time_t epoch 1st Jan 1970.
  std::chrono::system_clock::duration duration(ts.tv_sec * STL_TICKS_PER_SEC + ts.tv_nsec * multiplier / divider);
  return std::chrono::system_clock::time_point(duration);
}

AFIO_HEADERS_ONLY_MEMFUNC_SPEC result<size_t> stat_t::fill(const handle &h, stat_t::want wanted) noexcept
{
  AFIO_LOG_FUNCTION_CALL(&h);
  struct stat s
  {
  };
  memset(&s, 0, sizeof(s));
  size_t ret = 0;

  if(-1 == ::fstat(h.native_handle().fd, &s))
  {
    return posix_error();
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

AFIO_V2_NAMESPACE_END
