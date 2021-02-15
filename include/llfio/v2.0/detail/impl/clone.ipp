/* A filesystem algorithm which clones a directory tree
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: May 2020


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

#include "../../algorithm/clone.hpp"

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<file_handle::extent_type> clone_or_copy(file_handle &src, const path_handle &destdir, path_view destleaf,
                                                                              bool preserve_timestamps, bool force_copy_now, file_handle::creation creation,
                                                                              deadline d) noexcept
  {
    LLFIO_LOG_FUNCTION_CALL(&src);
    filesystem::path destleaf_;
    if(destleaf.empty())
    {
      OUTCOME_TRY(destleaf_, src.current_path());
      if(destleaf_.empty())
      {
        // Source has been deleted, so can't infer leafname
        return errc::no_such_file_or_directory;
      }
      destleaf = destleaf_;
    }
    stat_t stat(nullptr);
    OUTCOME_TRY(stat.fill(src));
    if(creation != file_handle::creation::always_new)
    {
      auto r = file_handle::file(destdir, destleaf, file_handle::mode::attr_read, file_handle::creation::open_existing);
      if(r)
      {
        stat_t deststat(nullptr);
        OUTCOME_TRY(deststat.fill(r.value()));
        if((stat.st_type == deststat.st_type) && (stat.st_mtim == deststat.st_mtim) && (stat.st_size == deststat.st_size)
#ifndef _WIN32
           && (stat.st_perms == deststat.st_perms) && (stat.st_uid == deststat.st_uid) && (stat.st_gid == deststat.st_gid) && (stat.st_rdev == deststat.st_rdev)
#endif
        )
        {
          return 0;  // nothing copied
        }
      }
    }
    OUTCOME_TRY(auto &&dest, file_handle::file(destdir, destleaf, file_handle::mode::write, creation, src.kernel_caching()));
    bool failed = true;
    auto undest = make_scope_exit([&]() noexcept {
      if(failed)
      {
        (void) dest.unlink(d);
      }
      else if(preserve_timestamps)
      {
        (void) stat.stamp(dest);
      }
      (void) dest.close();
    });
    (void) undest;
    {
      log_level_guard g(log_level::fatal);
      auto r = src.clone_extents_to(dest, d, force_copy_now, false);
      if(r)
      {
        failed = false;
        return r.assume_value().length;
      }
    }
    statfs_t statfs;
    OUTCOME_TRY(statfs.fill(src, statfs_t::want::bavail));
    if(stat.st_blocks > statfs.f_bavail)
    {
      return errc::no_space_on_device;
    }
    OUTCOME_TRY(auto &&copied, src.clone_extents_to(dest, d, force_copy_now, true));
    failed = false;
    return copied.length;
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
