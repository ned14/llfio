/* Integration test kernel for whether clone() works
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

#include "../test_kernel_decl.hpp"

#include <chrono>

#include "quickcpplib/algorithm/small_prng.hpp"
#include "quickcpplib/algorithm/string.hpp"

#ifndef _WIN32
#include <sys/resource.h>
#endif

static inline void TestCloneExtents()
{
  static constexpr int DURATION = 20;
  static constexpr size_t max_file_extent = (size_t) 100 * 1024 * 1024;
  namespace llfio = LLFIO_V2_NAMESPACE;
  using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
  static const auto &tempdirh = llfio::path_discovery::storage_backed_temporary_files_directory();
  small_prng rand;
  auto begin = std::chrono::steady_clock::now();
  for(size_t round = 0; std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() < DURATION; round++)
  {
    struct handle_t
    {
      std::vector<llfio::file_handle::extent_pair> extents_written;
      llfio::mapped_file_handle fh{llfio::mapped_file_handle::mapped_uniquely_named_file(0, tempdirh, llfio::mapped_file_handle::mode::write,
                                                                                         llfio::mapped_file_handle::caching::all,
                                                                                         llfio::mapped_file_handle::flag::unlink_on_first_close)
                                   .value()};
      llfio::mapped_file_handle::extent_type maximum_extent{0};
    } handles[2];
    std::vector<llfio::byte> shouldbe;
    handles[0].extents_written.reserve(128);
    handles[0].maximum_extent = rand() % (max_file_extent * 3 / 4);
    handles[0].fh.truncate(handles[0].maximum_extent).value();
    handles[1].extents_written.reserve(128);
    handles[1].maximum_extent = rand() % max_file_extent;
    handles[1].fh.truncate(handles[1].maximum_extent).value();
    for(uint8_t c = 1; c != 0; c++)
    {
      auto r = rand();
      handle_t &h = handles[c & 1];
      auto offset = r % (h.maximum_extent / 2);
      if(r & 30)  // cluster around two poles
      {
        offset += h.maximum_extent / 2;
      }
      auto size = rand() % std::min(h.maximum_extent - offset, h.maximum_extent / 256);
      llfio::byte buffer[65536];
      memset(&buffer, c, sizeof(buffer));
      h.extents_written.push_back({offset, size});
      for(unsigned n = 0; n < size; n += sizeof(buffer))
      {
        auto towrite = std::min((size_t) size - n, sizeof(buffer));
        h.fh.write(offset + n, {{buffer, towrite}}).value();
      }
    }
    for(auto &h : handles)
    {
      (void) h;
#ifdef _WIN32
      // On some filing systems, need to force block allocation
      h.fh.barrier(llfio::file_handle::barrier_kind::nowait_view_only).value();
#endif
#if 0
      std::cout << (&h - handles) << ":\n";
      std::sort(h.extents_written.begin(), h.extents_written.end());
      for(auto &i : h.extents_written)
      {
        std::cout << "  " << i.offset << "," << i.length << std::endl;
      }
#endif
    }
    shouldbe.resize(handles[1].maximum_extent);
    memcpy(shouldbe.data(), handles[1].fh.address(), shouldbe.size());

    // Choose some random extent in source to clone into dest. Make it big.
    llfio::mapped_file_handle::extent_pair srcregion{(rand() % (handles[0].maximum_extent / 2)), (rand() % (handles[0].maximum_extent / 2))};
    auto destoffset = rand() % (handles[1].maximum_extent / 2);
    std::cout << "\nRound " << (round + 1) << ": Cloning " << srcregion.offset << "-" << srcregion.length << " to offset " << destoffset << " ..." << std::endl;
    handles[0].fh.clone_extents_to(srcregion, handles[1].fh, destoffset).value();
    // Destination will be original maximum extent, or any overlap of extents copied
    auto maxtobecopied = std::min(srcregion.length, handles[0].maximum_extent - srcregion.offset);
    auto destshouldbe = std::max(handles[1].maximum_extent, destoffset + maxtobecopied);
    shouldbe.resize(destshouldbe);
    memcpy(shouldbe.data() + destoffset, handles[0].fh.address() + srcregion.offset, maxtobecopied);

    std::cout << "   Source file has " << handles[0].fh.maximum_extent().value() << " maximum extent. Destination file has "
              << handles[1].fh.maximum_extent().value() << " maximum extent (was " << handles[1].maximum_extent << ")." << std::endl;
    // Source maximum extent should be unchanged
    BOOST_CHECK(handles[0].fh.maximum_extent().value() == handles[0].maximum_extent);
    BOOST_REQUIRE(handles[1].fh.maximum_extent().value() == destshouldbe);
    llfio::stat_t src_stat(nullptr), dest_stat(nullptr);
    src_stat.fill(handles[0].fh).value();
    dest_stat.fill(handles[1].fh).value();
    std::cout << "   Source file has " << src_stat.st_blocks << " blocks allocated. Destination file has " << dest_stat.st_blocks << " blocks allocated."
              << std::endl;

    for(size_t n = 0; n < destshouldbe; n++)
    {
      if(shouldbe.data()[n] != handles[1].fh.address()[n])
      {
        std::cerr << "Byte at offset " << n << " is '" << (int) *(char *) &shouldbe.data()[n] << "' in source and is '"
                  << (int) *(char *) &handles[1].fh.address()[n] << "' in destination." << std::endl;
        BOOST_REQUIRE(shouldbe.data()[n] == handles[1].fh.address()[n]);
        break;
      }
    }
  }
}

static inline void TestCloneOrCopyFileWhole()
{
  static constexpr int DURATION = 20;
  static constexpr size_t max_file_extent = (size_t) 100 * 1024 * 1024;
  namespace llfio = LLFIO_V2_NAMESPACE;
  using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
  const auto &tempdirh = llfio::path_discovery::storage_backed_temporary_files_directory();
  small_prng rand;
  auto begin = std::chrono::steady_clock::now();
  for(size_t round = 0; std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() < DURATION; round++)
  {
    std::vector<llfio::file_handle::extent_pair> extents_written;
    extents_written.reserve(256);
    auto srcfh =
    llfio::mapped_file_handle::mapped_uniquely_named_file(0, tempdirh, llfio::mapped_file_handle::mode::write, llfio::mapped_file_handle::caching::all,
                                                          llfio::mapped_file_handle::flag::unlink_on_first_close)
    .value();
    auto maximum_extent = rand() % max_file_extent;
    srcfh.truncate(maximum_extent).value();
    for(uint8_t c = 1; c != 0; c++)
    {
      auto r = rand();
      auto offset = r % (maximum_extent / 2);
      if(r & 30)
      {
        offset += maximum_extent / 2;
      }
      auto size = rand() % std::min(maximum_extent - offset, maximum_extent / 256);
      llfio::byte buffer[65536];
      memset(&buffer, c, sizeof(buffer));
      extents_written.push_back({offset, size});
      for(unsigned n = 0; n < size; n += sizeof(buffer))
      {
        auto towrite = std::min((size_t) size - n, sizeof(buffer));
        srcfh.write(offset + n, {{buffer, towrite}}).value();
      }
    }
#ifdef _WIN32
    // On some filing systems, need to force block allocation
    srcfh.barrier(llfio::file_handle::barrier_kind::nowait_view_only).value();
#endif
    // std::sort(extents_written.begin(), extents_written.end());
    // for(auto &i : extents_written)
    //{
    //  std::cout << i.offset << "," << i.length << std::endl;
    //}

    auto randomname = llfio::utils::random_string(32);
    randomname.append(".random");
    llfio::algorithm::clone_or_copy(srcfh, tempdirh, randomname).value();

    auto destfh =
    llfio::mapped_file_handle::mapped_file(tempdirh, randomname, llfio::mapped_file_handle::mode::write, llfio::mapped_file_handle::creation::open_existing,
                                           llfio::mapped_file_handle::caching::all, llfio::mapped_file_handle::flag::unlink_on_first_close)
    .value();
    std::cout << "\nRound " << (round + 1) << ": Source file has " << srcfh.maximum_extent().value() << " maximum extent. Destination file has "
              << destfh.maximum_extent().value() << " maximum extent." << std::endl;
    BOOST_REQUIRE(srcfh.maximum_extent().value() == destfh.maximum_extent().value());
    llfio::stat_t src_stat(nullptr), dest_stat(nullptr);
    src_stat.fill(srcfh).value();
    dest_stat.fill(destfh).value();
    std::cout << "Source file has " << src_stat.st_blocks << " blocks allocated. Destination file has " << dest_stat.st_blocks << " blocks allocated."
              << std::endl;
    BOOST_CHECK(abs((long) src_stat.st_blocks - (long) dest_stat.st_blocks) < ((long) src_stat.st_blocks / 8));

    for(size_t n = 0; n < maximum_extent; n++)
    {
      if(srcfh.address()[n] != destfh.address()[n])
      {
        std::cerr << "Byte at offset " << n << " is '" << *(char *) &srcfh.address()[n] << "' in source and is '" << *(char *) &destfh.address()[n]
                  << "' in destination." << std::endl;
        BOOST_REQUIRE(srcfh.address()[n] == destfh.address()[n]);
        break;
      }
    }
  }
}

#if 0
static inline void TestCloneOrCopyTree()
{
  static constexpr size_t rounds = 10;
#if defined(_WIN32) || defined(__APPLE__)
  static constexpr size_t total_entries = 100;  // create 100 directories in each random directory tree
#else
  static constexpr size_t total_entries = 1000;  // create 1000 directories in each random directory tree
#endif
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::file_handle;
  using QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
  using QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string;
#ifndef _WIN32
  {
    struct rlimit r
    {
      1024 * 1024, 1024 * 1024
    };
    setrlimit(RLIMIT_NOFILE, &r);
  }
#endif
  small_prng rand;
  std::vector<directory_handle> dirhs;
  dirhs.reserve(total_entries);
  for(size_t round = 0; round < rounds; round++)
  {
    size_t entries_created = 1;
    dirhs.clear();
    dirhs.emplace_back(directory_handle::temp_directory().value());
    auto dirhpath = dirhs.front().current_path().value();
    std::cout << "\n\nCreating a random directory tree containing " << total_entries << " directories at " << dirhpath << " ..." << std::endl;
    auto begin = std::chrono::high_resolution_clock::now();
    for(size_t entries = 0; entries < total_entries; entries++)
    {
      const auto v = rand();
      const auto dir_idx = ((v >> 8) % dirhs.size());
      const auto file_entries = (v & 255);
      const auto &dirh = dirhs[dir_idx];
      filesystem::path::value_type buffer[10];
      {
        auto c = (uint32_t) entries;
        buffer[0] = 'd';
        to_hex_string(buffer + 1, 8, (const char *) &c, 4);
        buffer[9] = 0;
      }
      auto h = directory_handle::directory(dirh, path_view(buffer, 9, true), directory_handle::mode::write, directory_handle::creation::if_needed).value();
      entries_created++;
      for(size_t n = 0; n < file_entries; n++)
      {
        auto c = (uint8_t) n;
        buffer[0] = 'f';
        to_hex_string(buffer + 1, 2, (const char *) &c, 1);
        buffer[3] = 0;
        file_handle::file(h, path_view(buffer, 3, true), file_handle::mode::write, file_handle::creation::if_needed).value();
        entries_created++;
      }
      dirhs.emplace_back(std::move(h));
    }
    auto end = std::chrono::high_resolution_clock::now();
    dirhs.resize(1);
    std::cout << "Created " << entries_created << " filesystem entries in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (entries_created / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0))
              << " entries/sec).\n";

    auto summary = algorithm::summarize(dirhs.front()).value();
    std::cout << "Summary: " << summary.types[filesystem::file_type::regular] << " files and " << summary.types[filesystem::file_type::directory] << " directories created of " << summary.size << " bytes, " << summary.allocated << " bytes allocated in " << summary.blocks << " blocks with depth of " << summary.max_depth << "." << std::endl;
    BOOST_CHECK(summary.types[filesystem::file_type::regular] + summary.types[filesystem::file_type::directory] == entries_created);

    std::cout << "\nCalling llfio::algorithm::reduce() on that randomised directory tree ..." << std::endl;
    begin = std::chrono::high_resolution_clock::now();
    auto entries_removed = algorithm::reduce(std::move(dirhs.front())).value();
    end = std::chrono::high_resolution_clock::now();
    // std::cout << entries_removed << " " << entries_created << std::endl;
    BOOST_CHECK(entries_removed == entries_created);
    if(entries_removed != entries_created)
    {
      std::cout << "Entries created " << entries_created << ", entries removed " << entries_removed << std::endl;
    }
    std::cout << "Reduced " << entries_created << " filesystem entries in " << (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0) << " seconds (which is " << (entries_created / (std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() / 1000000.0))
              << " entries/sec).\n";

    log_level_guard g(log_level::fatal);
    auto r = directory_handle::directory({}, dirhpath);
    BOOST_REQUIRE(!r && r.error() == errc::no_such_file_or_directory);
  }
}
#endif

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, clone_extents, "Tests that llfio::file_handle::clone_extents() of partial extents works as expected",
                       TestCloneExtents())
KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, clone_or_copy_file_whole,
                       "Tests that llfio::algorithm::clone_or_copy(file_handle) of whole files works as expected", TestCloneOrCopyFileWhole())
