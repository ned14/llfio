/* Demonstration that path_view is faster than path for WG21
(C) 2024 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Dec 2024


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

/* For a randomly generated directory hierarchy of 128457372 items in 1000000
directories (average 128 items per directory):

echo 3 > /proc/sys/vm/drop_caches was done before each test

Traversal test for path (very short paths) ...
  Traversed 1000001 directories on "testdir" in 123.289 seconds (which is
8111.06 directories/sec, or 959.762 ns/item).

Traversal test for path_view (very short paths) ...
  Traversed 1000001 directories on "testdir" in 102.852 seconds (which is
9722.77 directories/sec, or 800.666 ns/item).

Traversal test for path (longer paths) ...
  Traversed 1000001 directories on "testdir" in 206.522 seconds (which is
4842.09 directories/sec, or 1607.71 ns/item).

Traversal test for path_view (longer paths) ...
  Traversed 1000001 directories on "testdir" in 18.7709 seconds (which is
53274.1 directories/sec, or 146.125 ns/item).
*/

#include "../include/llfio.hpp"
#include "llfio/v2.0/directory_handle.hpp"

#if !defined(_WIN32)

#include <atomic>
#include <chrono>
#include <iostream>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>

namespace llfio = LLFIO_V2_NAMESPACE;
namespace fs = llfio::filesystem;

struct base_visitor : public llfio::algorithm::traverse_visitor
{
  std::atomic<llfio::file_handle::extent_type> total_bytes{0};
  std::atomic<size_t> total_items{0};

  void clear()
  {
    total_bytes = 0;
    total_items = 0;
  }

  virtual llfio::result<llfio::directory_handle>
  directory_open_failed(void *, llfio::result<void>::error_type &&,
                        const llfio::directory_handle &, llfio::path_view,
                        size_t) noexcept override final
  {
    // Ignore failures to open a directory, likely owned by a different user
    return llfio::success();
  }
};

struct path_visitor1 final : public base_visitor
{
  static constexpr char name[] = "path (very short paths)";

  virtual llfio::result<void>
  post_enumeration(void *, const llfio::directory_handle &dirh,
                   llfio::directory_handle::buffers_type &contents,
                   size_t) noexcept override final
  {
    llfio::file_handle::extent_type bytes = 0;
    struct stat s;
    for(const llfio::directory_entry &entry : contents)
    {
      // This line is the only difference
      fs::path leafname(entry.leafname.path());
      if(-1 == fstatat(dirh.native_handle().fd, leafname.c_str(), &s,
                       AT_SYMLINK_NOFOLLOW))
      {
        const auto errcode = errno;
        std::cerr << "FATAL: Failed to fstatat "
                  << (dirh.current_path().value() / leafname) << " due to '"
                  << strerror(errcode) << "'." << std::endl;
        abort();
      }
      bytes += s.st_size;
    }
    this->total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    this->total_items.fetch_add(contents.size(), std::memory_order_relaxed);
    return llfio::success();
  }
};

struct path_view_visitor1 final : public base_visitor
{
  static constexpr char name[] = "path_view (very short paths)";

  virtual llfio::result<void>
  post_enumeration(void *, const llfio::directory_handle &dirh,
                   llfio::directory_handle::buffers_type &contents,
                   size_t) noexcept override final
  {
    llfio::file_handle::extent_type bytes = 0;
    struct stat s;
    for(const llfio::directory_entry &entry : contents)
    {
      // This line is the only difference
      auto leafname = entry.leafname.render_null_terminated();
      if(-1 == fstatat(dirh.native_handle().fd, leafname.c_str(), &s,
                       AT_SYMLINK_NOFOLLOW))
      {
        const auto errcode = errno;
        std::cerr << "FATAL: Failed to fstatat "
                  << (dirh.current_path().value() / entry.leafname)
                  << " due to '" << strerror(errcode) << "'." << std::endl;
        abort();
      }
      bytes += s.st_size;
    }
    this->total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    this->total_items.fetch_add(contents.size(), std::memory_order_relaxed);
    return llfio::success();
  }
};

struct path_visitor2 final : public base_visitor
{
  static constexpr char name[] = "path (longer paths)";

  virtual llfio::result<void>
  post_enumeration(void *, const llfio::directory_handle &dirh,
                   llfio::directory_handle::buffers_type &contents,
                   size_t) noexcept override final
  {
    llfio::file_handle::extent_type bytes = 0;
    struct stat s;
    auto dirpath = dirh.current_path().value();
    for(const llfio::directory_entry &entry : contents)
    {
      fs::path path(dirpath / entry.leafname.path());
      if(-1 == lstat(path.c_str(), &s))
      {
        const auto errcode = errno;
        std::cerr << "FATAL: Failed to lstat " << path << " due to '"
                  << strerror(errcode) << "'." << std::endl;
        abort();
      }
      bytes += s.st_size;
    }
    this->total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    this->total_items.fetch_add(contents.size(), std::memory_order_relaxed);
    return llfio::success();
  }
};

struct path_view_visitor2 final : public base_visitor
{
  static constexpr char name[] = "path_view (longer paths)";

  virtual llfio::result<void>
  post_enumeration(void *, const llfio::directory_handle &dirh,
                   llfio::directory_handle::buffers_type &contents,
                   size_t) noexcept override final
  {
    llfio::file_handle::extent_type bytes = 0;
    struct stat s;
    auto dirpath = dirh.current_path().value().native();
    fs::path::value_type path[PATH_MAX];
    memcpy(path, dirpath.data(), dirpath.size());
    char *endp = path + dirpath.size();
    *endp++ = '/';
    for(const llfio::directory_entry &entry : contents)
    {
      auto leafname = entry.leafname.render_null_terminated();
      memcpy(endp, leafname.data(), leafname.size() + 1);
      if(-1 == lstat(dirpath.data(), &s))
      {
        const auto errcode = errno;
        std::cerr << "FATAL: Failed to lstat " << dirpath.data()
                  << " due to '" << strerror(errcode) << "'." << std::endl;
        abort();
      }
      bytes += s.st_size;
    }
    this->total_bytes.fetch_add(bytes, std::memory_order_relaxed);
    this->total_items.fetch_add(contents.size(), std::memory_order_relaxed);
    return llfio::success();
  }
};

static constexpr llfio::path_view to_traverse_path("testdir");
static auto to_traverse = llfio::path_handle::path(to_traverse_path).value();

template <class T> void do_test(T &visitor)
{
  visitor.clear();
  std::cout << "\nTraversal test for " << visitor.name << " ..." << std::endl;
  const auto begin = std::chrono::high_resolution_clock::now();
  auto items_mt = llfio::algorithm::traverse(to_traverse, &visitor).value();
  const auto end = std::chrono::high_resolution_clock::now();
  std::cout
  << "  Traversed " << items_mt << " directories on " << to_traverse_path
  << " in "
  << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
      .count() /
      1000000.0)
  << " seconds (which is "
  << (items_mt /
      (std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
       .count() /
       1000000.0))
  << " directories/sec, or "
  << (double(std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin)
             .count()) /
      double(visitor.total_items))
  << " ns/item).\n";
}


int main(int argc, const char *argv[])
{
#ifndef _WIN32
  {
    struct rlimit r
    {
      1024 * 1024, 1024 * 1024
    };
    setrlimit(RLIMIT_NOFILE, &r);
  }
#endif
#if 0
  {
    static constexpr size_t total_entries =
    1000000;  // create 1000000 directories in each random directory tree
    using LLFIO_V2_NAMESPACE::directory_handle;
    using LLFIO_V2_NAMESPACE::file_handle;
    using LLFIO_V2_NAMESPACE::path_view;
    using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
    using QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string;
    small_prng rand;
    std::vector<directory_handle> dirhs;
    dirhs.reserve(total_entries);
    size_t entries_created = 1;
    dirhs.clear();
    dirhs.emplace_back(directory_handle::directory(
                       {}, to_traverse_path, directory_handle::mode::write)
                       .value());
    auto dirhpath = dirhs.front().current_path().value();
    std::cout << "\n\nCreating a random directory tree containing "
              << total_entries << " directories at " << dirhpath << " ..."
              << std::endl;
    auto begin = std::chrono::high_resolution_clock::now();
    for(size_t entries = 0; entries < total_entries; entries++)
    {
      const auto v = rand();
      const auto dir_idx = ((v >> 8) % dirhs.size());
      const auto file_entries = (v & 255);
      const auto &dirh = dirhs[dir_idx];
      fs::path::value_type buffer[10];
      {
        auto c = (uint32_t) entries;
        buffer[0] = 'd';
        to_hex_string(buffer + 1, 8, (const char *) &c, 4);
        buffer[9] = 0;
      }
      auto h =
      directory_handle::directory(
      dirh, path_view(buffer, 9, path_view::zero_terminated),
      directory_handle::mode::write, directory_handle::creation::if_needed)
      .value();
      for(size_t n = 0; n < file_entries; n++)
      {
        auto c = (uint8_t) n;
        buffer[0] = 'f';
        to_hex_string(buffer + 1, 2, (const char *) &c, 1);
        buffer[3] = 0;
        file_handle::file(h, path_view(buffer, 3, path_view::zero_terminated),
                          file_handle::mode::write,
                          file_handle::creation::if_needed)
        .value();
        entries_created++;
      }
      entries_created++;
      dirhs.emplace_back(std::move(h));
    }
    auto end = std::chrono::high_resolution_clock::now();
    dirhs.resize(1);
    std::cout
    << "Created " << entries_created << " filesystem entries in "
    << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
        .count() /
        1000000.0)
    << " seconds (which is "
    << (entries_created /
        (std::chrono::duration_cast<std::chrono::microseconds>(end - begin)
         .count() /
         1000000.0))
    << " entries/sec).\n";
  }
#endif
#if 0
  llfio::algorithm::reduce(
  llfio::directory_handle::directory({}, to_traverse_path,
                                     llfio::directory_handle::mode::write)
  .value())
  .value();
#endif

  if(argc < 2)
  {
    std::cerr << "FATAL: " << argv[0] << " <testidx>" << std::endl;
    return 1;
  }
  const auto testno = atoi(argv[1]);

  switch(testno)
  {
  case 1:
  {
    path_visitor1 v;
    do_test(v);
    break;
  }
  case 2:
  {
    path_visitor2 v;
    do_test(v);
    break;
  }
  case 3:
  {
    path_view_visitor1 v;
    do_test(v);
    break;
  }
  case 4:
  {
    path_view_visitor2 v;
    do_test(v);
    break;
  }
  }
  return 0;
}
#else
int main()
{
  return 0;
}
#endif
