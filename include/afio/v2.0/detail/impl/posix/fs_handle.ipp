/* A filing system handle
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#include "../../../fs_handle.hpp"
#include "../../../stat.hpp"
#include "../../../utils.hpp"
#include "import.hpp"

#include <limits.h>  // for PATH_MAX

AFIO_V2_NAMESPACE_BEGIN

result<void> fs_handle::_fetch_inode() noexcept
{
  stat_t s;
  OUTCOME_TRYV(s.fill(_get_handle(), stat_t::want::dev | stat_t::want::ino));
  _devid = s.st_dev;
  _inode = s.st_ino;
  return success();
}

inline result<path_handle> containing_directory(optional<std::reference_wrapper<filesystem::path>> out_filename, const handle &h, const fs_handle &fsh, deadline d) noexcept
{
#ifdef AFIO_DISABLE_RACE_FREE_PATH_FUNCTIONS
  return std::errc::function_not_supported;
#endif
  std::chrono::steady_clock::time_point began_steady;
  std::chrono::system_clock::time_point end_utc;
  if(d)
  {
    if(d.steady)
      began_steady = std::chrono::steady_clock::now();
    else
      end_utc = d.to_time_point();
  }
  try
  {
    for(;;)
    {
      // Get current path for handle and open its containing dir
      auto currentpath_ = h.current_path();
      if(!currentpath_)
        continue;
      filesystem::path _currentpath = std::move(currentpath_.value());
      // If current path is empty, it's been deleted
      if(_currentpath.empty())
        return std::errc::no_such_file_or_directory;
      // Split the path into root and leafname
      path_view currentpath(_currentpath);
      path_view filename = currentpath.filename();
      currentpath.remove_filename();
      // Zero terminate the root path so it doesn't get copied later
      const_cast<filesystem::path::string_type &>(_currentpath.native())[currentpath.native_size()] = 0;
      auto currentdirh_ = path_handle::path(currentpath);
      if(!currentdirh_)
        continue;
      path_handle currentdirh = std::move(currentdirh_.value());
      if(h.flags() & handle::flag::disable_safety_unlinks)
        return success(std::move(currentdirh));
      // Open the same file name, and compare dev and inode
      path_view::c_str zpath(filename);
      int fd = ::openat(currentdirh.native_handle().fd, filename.buffer, 0);
      if(fd == -1)
        continue;
      auto unfd = undoer([fd] { ::close(fd); });
      (void) unfd;
      struct stat s;
      if(-1 == ::fstat(fd, &s))
        continue;
      // If the same, we know for a fact that this is the correct containing dir for now at least
      if(s.st_dev == fsh.st_dev() && s.st_ino == fsh.st_ino())
      {
        if(out_filename)
          out_filename->get() = filename.path();
        return success(std::move(currentdirh));
      }
      // Check timeout
      if(d)
      {
        if(d.steady)
        {
          if(std::chrono::steady_clock::now() >= (began_steady + std::chrono::nanoseconds(d.nsecs)))
            return std::errc::timed_out;
        }
        else
        {
          if(std::chrono::system_clock::now() >= end_utc)
            return std::errc::timed_out;
        }
      }
    }
  }
  catch(...)
  {
    return error_from_exception();
  }
}

result<path_handle> fs_handle::parent_path_handle(deadline d) const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();
  return containing_directory({}, h, *this, d);
}

result<void> fs_handle::relink(const path_handle &base, path_view_type path, deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();
  path_view::c_str zpath(path);
#ifdef O_TMPFILE
  // If the handle was created with O_TMPFILE, we need a different approach
  if(path.empty() && (h.kernel_caching() == handle::caching::temporary))
  {
    char _path[PATH_MAX];
    snprintf(_path, PATH_MAX, "/proc/self/fd/%d", h.native_handle().fd);
    if(-1 == ::linkat(AT_FDCWD, _path, base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer, AT_SYMLINK_FOLLOW))
      return {errno, std::system_category()};
  }
#endif
  // Open our containing directory
  filesystem::path filename;
  OUTCOME_TRY(dirh, containing_directory(filename, h, *this, d));
  if(-1 == ::renameat(dirh.native_handle().fd, filename.c_str(), base.is_valid() ? base.native_handle().fd : AT_FDCWD, zpath.buffer))
    return {errno, std::system_category()};
  return success();
}

result<void> fs_handle::unlink(deadline d) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  auto &h = _get_handle();
  // Open our containing directory
  filesystem::path filename;
  OUTCOME_TRY(dirh, containing_directory(filename, h, *this, d));
  if(-1 == ::unlinkat(dirh.native_handle().fd, filename.c_str(), 0))
    return {errno, std::system_category()};
  return success();
}

AFIO_V2_NAMESPACE_END
