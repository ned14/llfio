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
    try
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
             && (stat.st_perms == deststat.st_perms) && (stat.st_uid == deststat.st_uid) && (stat.st_gid == deststat.st_gid) &&
             (stat.st_rdev == deststat.st_rdev)
#endif
          )
          {
            return 0;  // nothing copied
          }
        }
      }
      OUTCOME_TRY(auto &&dest, file_handle::file(destdir, destleaf, file_handle::mode::write, creation, src.kernel_caching()));
      bool failed = true;
      auto undest = make_scope_exit(
      [&]() noexcept
      {
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
      if(!force_copy_now)
      {
#if LLFIO_LOGGING_LEVEL
        log_level_guard g(log_level::fatal);
#endif
        auto r = src.clone_extents_to(dest, d, force_copy_now, false);
        if(r)
        {
          failed = false;
          return r.assume_value().length;
        }
      }
      statfs_t statfs;
      OUTCOME_TRY(statfs.fill(destdir, statfs_t::want::bavail));
      if(stat.st_blocks > statfs.f_bavail)
      {
        return errc::no_space_on_device;
      }
      OUTCOME_TRY(auto &&copied, src.clone_extents_to(dest, d, force_copy_now, true));
      failed = false;
      return copied.length;
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

  LLFIO_HEADERS_ONLY_FUNC_SPEC result<bool> relink_or_clone_copy_unlink(file_handle &src, const path_handle &destdir, path_view destleaf, bool atomic_replace,
                                                                        bool preserve_timestamps, bool force_copy_now, deadline d) noexcept
  {
    try
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
      LLFIO_DEADLINE_TO_SLEEP_INIT(d);
      {
        auto r = src.relink(destdir, destleaf, atomic_replace, d);
        if(r)
        {
          return false;
        }
        if(!atomic_replace && r.assume_error() == errc::file_exists)
        {
          OUTCOME_TRY(std::move(r));
        }
      }
      stat_t stat(nullptr);
      OUTCOME_TRY(stat.fill(src));

      OUTCOME_TRY(auto &&desth, file_handle::temp_inode(destdir, file_handle::mode::write, src.kernel_caching()));
      if(!atomic_replace)
      {
        auto r = file_handle::file(destdir, destleaf, file_handle::mode::none);
        if(r)
        {
          return errc::file_exists;
        }
      }
      {
#if LLFIO_LOGGING_LEVEL
        log_level_guard g(log_level::fatal);
#endif
        bool failed = true;
        if(!force_copy_now)
        {
          if(src.clone_extents_to(desth, {}, force_copy_now, false))
          {
            failed = false;
          }
        }
        if(failed)
        {
          statfs_t statfs;
          OUTCOME_TRY(statfs.fill(desth, statfs_t::want::bavail));
          if(stat.st_blocks > statfs.f_bavail)
          {
            return errc::no_space_on_device;
          }
          OUTCOME_TRY(src.clone_extents_to(desth, {}, force_copy_now, true));
        }
      }
      if(preserve_timestamps)
      {
        OUTCOME_TRY(stat.stamp(desth));
      }
      LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
      deadline nd;
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      auto r = desth.link(destdir, destleaf, nd);
      if(atomic_replace && !r && r.assume_error() == errc::file_exists)
      {
        // Have to link + relink
        for(;;)
        {
          LLFIO_DEADLINE_TO_TIMEOUT_LOOP(d);
          auto randomname = utils::random_string(32);
          randomname.append(".random");
          r = desth.link(destdir, randomname);
          if(!r && r.assume_error() != errc::file_exists)
          {
            OUTCOME_TRY(std::move(r));
          }
          if(r)
          {
            // Note that if this fails, we leak the randomly named linked file.
            OUTCOME_TRY(auto &&desth2,
                        file_handle::file(destdir, randomname, file_handle::mode::write, file_handle::creation::open_existing, src.kernel_caching()));
            if(desth.unique_id() == desth2.unique_id())
            {
              LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
              r = desth2.relink(destdir, destleaf, atomic_replace, nd);
              if(!r)
              {
                OUTCOME_TRY(desth2.unlink());
                OUTCOME_TRY(std::move(r));
              }
              desth.swap(desth2);
              r = success();
              break;
            }
          }
        }
      }
      OUTCOME_TRY(std::move(r));
      auto undesth = make_scope_exit([&]() noexcept { (void) desth.unlink(); });
      if(preserve_timestamps)
      {
        OUTCOME_TRY(stat.stamp(desth));
      }
      LLFIO_DEADLINE_TO_PARTIAL_DEADLINE(nd, d);
      OUTCOME_TRY(src.unlink(nd));
      undesth.release();
      // We can't use swap() here, because src may not be a file handle.
      OUTCOME_TRY(src._replace_handle(std::move(desth)));
      return true;
    }
    catch(...)
    {
      return error_from_exception();
    }
  }

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
