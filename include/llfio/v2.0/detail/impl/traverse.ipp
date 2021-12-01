/* A filesystem algorithm which traverses a directory tree
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

#include "../../algorithm/traverse.hpp"

#include <condition_variable>
#include <iostream>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <sys/resource.h>
#include <sys/stat.h>
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
  LLFIO_HEADERS_ONLY_FUNC_SPEC result<size_t> traverse(const path_handle &_topdirh, traverse_visitor *visitor, size_t threads, void *data,
                                                       bool force_slow_path) noexcept
  {
    return visitor->finished(data, [&]() -> result<size_t> {
      try
      {
        LLFIO_LOG_FUNCTION_CALL(&_topdirh);
        std::shared_ptr<directory_handle> topdirh;
        {
          OUTCOME_TRY(auto &&dirh, directory_handle::directory(_topdirh, {}));
          topdirh = std::make_shared<directory_handle>(std::move(dirh));
        }
        bool use_slow_path = force_slow_path;
#ifndef _WIN32
        if(!use_slow_path)
        {
          struct rlimit r;
          if(getrlimit(RLIMIT_NOFILE, &r) >= 0)
          {
            if(r.rlim_cur - topdirh->native_handle().fd < 65536)
            {
              use_slow_path = true;
#ifndef NDEBUG
              std::cerr << "WARNING: llfio::traverse() is using slow path due to " << (r.rlim_cur - topdirh->native_handle().fd)
                        << " unused file descriptors remaining! Raise the limit using setrlimit(RLIMIT_NOFILE) if your application is > 1024 fd count safe."
                        << std::endl;
#endif
            }
          }
        }
#endif
        struct state_t
        {
          std::mutex lock;
          traverse_visitor *visitor{nullptr};
#if 0
        struct workitem
        {
          std::shared_ptr<directory_handle> dirh;
          filesystem::path _leaf;
          workitem() {}
          workitem(std::shared_ptr<directory_handle> _dirh, path_view leaf)
              : dirh(std::move(_dirh))
              , _leaf(leaf.path())
          {
          }
          path_view leaf() const noexcept { return _leaf; } 
        };
#else
          struct workitem
          {
            std::shared_ptr<directory_handle> dirh;
            bool using_sso{true};
            uint8_t _sso_length{0};
            union {
              filesystem::path::value_type _sso[64];
              filesystem::path _alloc;
            };
            workitem() {}
            workitem(std::shared_ptr<directory_handle> _dirh, path_view leaf)
                : dirh(std::move(_dirh))
            {
              if(!leaf.empty())
              {
                size_t bytes = (1 + leaf.native_size()) * sizeof(filesystem::path::value_type);
                if(bytes <= sizeof(_sso))
                {
                  using_sso = true;
                  visit(leaf, [&](auto sv) {
                    memcpy(_sso, sv.data(), bytes);
                    _sso_length = (uint8_t) sv.size();
                  });
                }
                else
                {
                  new(&_alloc) filesystem::path(leaf.path());
                  using_sso = false;
                }
              }
            }
            ~workitem()
            {
              if(!using_sso)
              {
                _alloc.~path();
                using_sso = true;
              }
            }
#ifdef _MSC_VER
            // MSVC's list::splice() always copies :(
            workitem(const workitem &o) noexcept
                : workitem(const_cast<workitem &&>(std::move(o)))
            {
            }
#else
            workitem(const workitem &) = delete;
#endif
            workitem &operator=(const workitem &) = delete;
            workitem(workitem &&o) noexcept
                : dirh(std::move(o.dirh))
                , using_sso(o.using_sso)
                , _sso_length(o._sso_length)
            {
              if(using_sso)
              {
                memcpy(_sso, o._sso, (1 + _sso_length) * sizeof(filesystem::path::value_type));
                o._sso_length = 0;
              }
              else
              {
                new(&_alloc) filesystem::path(std::move(o._alloc));
                o._alloc.~path();
                o.using_sso = true;
              }
            }
            workitem &operator=(workitem &&o) noexcept
            {
              if(this == &o)
              {
                return *this;
              }
              this->~workitem();
              new(this) workitem(std::move(o));
              return *this;
            }
            path_view leaf() const noexcept { return using_sso ? path_view(_sso, _sso_length, path_view::zero_terminated) : path_view(_alloc); }
          };
#endif
          std::vector<std::list<workitem>> workqueue;
          size_t workqueue_base{0};
          size_t dirs_processed{0}, known_dirs_remaining{0}, depth_processed{0}, threads_sleeping{0}, threads_running{0};

          explicit state_t(traverse_visitor *_visitor)
              : visitor(_visitor)
          {
          }
        } state(visitor);
        struct worker
        {
          state_t *state{nullptr};
          std::vector<directory_handle::buffer_type> entries{4096};
          directory_handle::buffers_type buffers;

          explicit worker(state_t *_state)
              : state(_state)
          {
          }

          result<void> run(std::unique_lock<std::mutex> &g, bool use_slow_path, std::shared_ptr<directory_handle> &topdirh, void *data)
          {
            typename state_t::workitem mywork;
            size_t mylevel = 0;
            assert(g.owns_lock());
            for(size_t n = state->workqueue_base; n < state->workqueue.size(); n++)
            {
              if(!state->workqueue[n].empty())
              {
                mywork = std::move(state->workqueue[n].front());
                state->workqueue[n].pop_front();
                mylevel = n;
                if(mylevel != state->depth_processed)
                {
                  state->depth_processed = mylevel;
                }
                break;
              }
            }
            if(!mywork.dirh)
            {
              return success();
            }
            if(mylevel > state->workqueue_base)
            {
              state->workqueue_base = mylevel;
            }
            state->dirs_processed++;
            state->known_dirs_remaining--;
            g.unlock();
            std::shared_ptr<directory_handle> mydirh;
            if(mywork.leaf().empty())
            {
              mydirh = mywork.dirh;
            }
            else
            {
              log_level_guard gg(log_level::fatal);
              auto r = directory_handle::directory(*mywork.dirh, mywork.leaf());
              if(!r)
              {
                OUTCOME_TRY(auto &&replacementh, state->visitor->directory_open_failed(data, std::move(r).error(), *mywork.dirh, mywork.leaf(), mylevel));
                mydirh = std::make_shared<directory_handle>(std::move(replacementh));
              }
              else
              {
                mydirh = std::make_shared<directory_handle>(std::move(r).value());
              }
            }
            if(mydirh->is_valid())
            {
              OUTCOME_TRY(auto &&do_enumerate, state->visitor->pre_enumeration(data, *mydirh, mylevel));
              if(do_enumerate)
              {
                for(;;)
                {
                  buffers = {entries, std::move(buffers)};
                  OUTCOME_TRY(buffers, mydirh->read({std::move(buffers), {}, directory_handle::filter::none}));
                  if(buffers.done())
                  {
                    break;
                  }
                  entries.resize(entries.size() << 1);
                }
                if(!(buffers.metadata() & stat_t::want::type))
                {
#ifdef _WIN32
                  abort();  // this should never occur on Windows
#else
                  for(auto &entry : buffers)
                  {
                    struct ::stat stat;
                    memset(&stat, 0, sizeof(stat));
                    path_view::zero_terminated_rendered_path<> zpath(entry.leafname);
                    if(::fstatat(mydirh->native_handle().fd, zpath.data(), &stat, AT_SYMLINK_NOFOLLOW) >= 0)
                    {
                      entry.stat.st_type = [](uint16_t mode) {
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
                      }(stat.st_mode);
                    }
                    else
                    {
                      return posix_error();
                    }
                  }
#endif
                }
                OUTCOME_TRY(state->visitor->post_enumeration(data, *mydirh, buffers, mylevel));
                std::list<state_t::workitem> newwork;
                for(auto &entry : buffers)
                {
                  int entry_type = 0;  // 0 = unknown, 1 = file, 2 = directory
                  switch(entry.stat.st_type)
                  {
                  case filesystem::file_type::directory:
                    entry_type = 2;
                    break;
                  case filesystem::file_type::regular:
                  case filesystem::file_type::symlink:
                  case filesystem::file_type::block:
                  case filesystem::file_type::character:
                  case filesystem::file_type::fifo:
                  case filesystem::file_type::socket:
                    entry_type = 1;
                    break;
                  default:
                    break;
                  }
                  if(2 == entry_type)
                  {
                    if(use_slow_path)
                    {
                      newwork.push_back(state_t::workitem(topdirh, mywork.leaf() / entry.leafname));
                    }
                    else
                    {
                      newwork.push_back(state_t::workitem(mydirh, entry.leafname));
                    }
                  }
                }
                g.lock();
                state->known_dirs_remaining += newwork.size();
                if(state->workqueue.size() < mylevel + 2)
                {
                  state->workqueue.emplace_back();
                }
                state->workqueue[mylevel + 1].splice(state->workqueue[mylevel + 1].end(), std::move(newwork));
                if(mylevel + 1 < state->workqueue_base)
                {
                  state->workqueue_base = mylevel + 1;
                }
                size_t dirs_processed = state->dirs_processed, known_dirs_remaining = state->known_dirs_remaining, depth_processed = state->depth_processed,
                       known_depth_remaining = state->workqueue.size();
                g.unlock();
                OUTCOME_TRY(state->visitor->stack_updated(data, dirs_processed, known_dirs_remaining, depth_processed, known_depth_remaining));
              }
            }
            return success();
          }
        };
        state.workqueue.emplace_back();
        state.workqueue.front().push_back(state_t::workitem(topdirh, {}));
        state.known_dirs_remaining = 1;
        worker firstworker(&state);
        {
          std::unique_lock<std::mutex> g(state.lock);
          for(size_t n = 0; state.known_dirs_remaining > 0 && (threads == 1 || n < 4); n++)
          {
            if(!g.owns_lock())
            {
              g.lock();
            }
            OUTCOME_TRY(firstworker.run(g, use_slow_path, topdirh, data));
          }
        }
        if(state.known_dirs_remaining > 0)
        {
          // Fire up the threadpool
          if(0 == threads)
          {
            // Filesystems are generally only concurrent to the real CPU count
            threads = std::thread::hardware_concurrency() / 2;
            if(threads < 4)
            {
              threads = 4;
            }
          }
          std::vector<worker> workers;
          workers.reserve(threads);
          workers.push_back(std::move(firstworker));
          for(size_t n = 1; n < threads; n++)
          {
            workers.push_back(worker(&state));
          }
          std::vector<std::thread> workerthreads;
          workerthreads.reserve(threads);
          std::condition_variable cond, maincond;
          bool done = false;
          optional<result<void>::error_type> run_error;
          {
            auto handle_failure = make_scope_fail([&]() noexcept {
              std::unique_lock<std::mutex> g(state.lock);
              done = true;
              while(state.threads_running > 0)
              {
                g.unlock();
                cond.notify_all();
                g.lock();
              }
              g.unlock();
              for(auto &i : workerthreads)
              {
                i.join();
              }
            });
            for(size_t n = 0; n < threads; n++)
            {
              workerthreads.push_back(std::thread(
              [&](worker *w) {
                std::unique_lock<std::mutex> g(state.lock);
                state.threads_running++;
                while(!done)
                {
                  if(state.known_dirs_remaining == 0)
                  {
                    // sleep
                    state.threads_sleeping++;
                    maincond.notify_all();
                    cond.wait(g);
                    state.threads_sleeping--;
                    if(done)
                    {
                      break;
                    }
                  }
                  else
                  {
                    // wake everybody
                    cond.notify_all();
                  }
                  auto r = w->run(g, use_slow_path, topdirh, data);
                  if(!g.owns_lock())
                  {
                    g.lock();
                  }
                  if(!r)
                  {
                    done = true;
                    if(!run_error)
                    {
                      run_error = std::move(r).error();
                    }
                    break;
                  }
                }
                state.threads_sleeping++;
                state.threads_running--;
                maincond.notify_all();
              },
              &workers[n]));
            }
          }
          {
            std::unique_lock<std::mutex> g(state.lock);
            while(state.threads_sleeping < threads && !done)
            {
              maincond.wait(g);
            }
            done = true;
            while(state.threads_running > 0)
            {
              g.unlock();
              cond.notify_all();
              g.lock();
            }
          }
          for(auto &i : workerthreads)
          {
            i.join();
          }
          if(run_error)
          {
            return std::move(*run_error);
          }
        }
#ifndef NDEBUG
        for(auto &i : state.workqueue)
        {
          assert(i.empty());
        }
#endif
        return state.dirs_processed;
      }
      catch(...)
      {
        return error_from_exception();
      }
    }());
  }
}  // namespace algorithm

LLFIO_V2_NAMESPACE_END
