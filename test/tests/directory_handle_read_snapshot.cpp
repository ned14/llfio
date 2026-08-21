/* Unit test kernel for the Windows directory_handle::read() bug fixes
(C) 2026 Niall Douglas <http://www.nedproductions.biz/> (1 commit)

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

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

// Regression tests for the fixes to directory_handle::read() on Windows:
// OPWBLOCK  - multi-syscall enumerations no longer return errc::operation_would_block with flags::none
// LIVELOCK  - the "directory changed, retry" loops are bounded (MAX_RETRIES) instead of unbounded
// BUFOVRFL  - STATUS_BUFFER_OVERFLOW is mapped to bounded retries / operation_would_block / no_buffer_space
// STALESNP  - _snapshot is reset at the top of read(), so buffers_type reuse no longer reports stale is_snapshot()
// SPINLOCK  - the internal lock is a proper spinlock acquired once per read() (correct memory ordering, backoff)
// SLFDEADL  - the retry path no longer re-acquires the lock (previously a guaranteed self-deadlock)

static inline void TestDirectoryHandleReadSnapshot()
{
  using namespace LLFIO_V2_NAMESPACE;
  using directory_handle = LLFIO_V2_NAMESPACE::directory_handle;
  using directory_entry = LLFIO_V2_NAMESPACE::directory_entry;
  using file_handle = LLFIO_V2_NAMESPACE::file_handle;
  using buffer_type = LLFIO_V2_NAMESPACE::directory_handle::buffer_type;
  using buffers_type = LLFIO_V2_NAMESPACE::directory_handle::buffers_type;
  using flags = LLFIO_V2_NAMESPACE::directory_handle::flags;
  using filter = LLFIO_V2_NAMESPACE::directory_handle::filter;
  using creation = LLFIO_V2_NAMESPACE::directory_handle::creation;
  using mode = LLFIO_V2_NAMESPACE::directory_handle::mode;
  using caching = LLFIO_V2_NAMESPACE::directory_handle::caching;
  using errc = LLFIO_V2_NAMESPACE::errc;

  static constexpr size_t LARGE_ENTRIES = 4000;  // entries in the large directory; with the writer thread running, pass
                                                 // 2 must span more than one NtQueryDirectoryFile() call
  static constexpr size_t SMALL_ENTRIES = 5;     // fits into a single syscall
  static constexpr size_t SPAN_HEADROOM = 65536;  // headroom in the span for entries added by the writer thread
  static constexpr const char *LARGE_DIR_NAME = "testdir_readsnapshot_large";
  static constexpr const char *SMALL_DIR_NAME = "testdir_readsnapshot_small";

  // Remove any leftover state from a previous crashed run
  {
    auto cleanup = directory_handle::directory({}, LARGE_DIR_NAME, mode::write, creation::open_existing, caching::all);
    if(cleanup)
    {
      (void) algorithm::reduce(std::move(cleanup.value()));
    }
    cleanup = directory_handle::directory({}, SMALL_DIR_NAME, mode::write, creation::open_existing, caching::all);
    if(cleanup)
    {
      (void) algorithm::reduce(std::move(cleanup.value()));
    }
  }

  auto largedirh =
  directory_handle::directory({}, LARGE_DIR_NAME, mode::write, creation::if_needed, caching::all).value();
  auto smalldirh =
  directory_handle::directory({}, SMALL_DIR_NAME, mode::write, creation::if_needed, caching::all).value();

  // Populate the large directory
  for(size_t n = 0; n < LARGE_ENTRIES; n++)
  {
    char name[32];
    snprintf(name, sizeof(name), "file%06zu", n);
    auto fh = file_handle::file(largedirh, name, file_handle::mode::write, file_handle::creation::if_needed,
                                file_handle::caching::none);
    BOOST_REQUIRE(fh);
  }
  // Populate the small directory
  for(size_t n = 0; n < SMALL_ENTRIES; n++)
  {
    char name[32];
    snprintf(name, sizeof(name), "small%02zu", n);
    auto fh = file_handle::file(smalldirh, name, file_handle::mode::write, file_handle::creation::if_needed,
                                file_handle::caching::none);
    BOOST_REQUIRE(fh);
  }

  // 1. OPWBLOCK + SLFDEADL + LIVELOCK: a quiescent large directory must enumerate completely
  // with flags::none. Before the fixes this either returned errc::operation_would_block
  // (multi-syscall enumeration) or self-deadlocked/hung on the unbounded retry loop.
  {
    std::vector<buffer_type> bigbuf(LARGE_ENTRIES + 1024);
    buffers_type req(bigbuf);
    auto r = largedirh.read({std::move(req), {}, filter::none}, std::chrono::seconds(30));
    BOOST_REQUIRE(r);
    BOOST_CHECK(r.value().done());
    BOOST_CHECK_EQUAL(r.value().size(), LARGE_ENTRIES);
  }

#ifdef _WIN32
  // 2. STALESNP: after a racy enumeration (is_snapshot() == false), reusing the same
  // buffers_type for a fresh single-syscall enumeration must report is_snapshot() == true
  // again. With flags::permit_racy_reads, a multi-syscall enumeration always returns
  // is_snapshot() == false (no retries are attempted for the NO_MORE_FILES path).
  {
    // A quiescent directory can enumerate in a single NtQueryDirectoryFile() syscall: the
    // pass 2 buffer is sized from pass 1 to fit the whole directory, and kernels which fill
    // that buffer completely (no issue #137 under-fill) return everything in one call, which
    // correctly reports is_snapshot() == true as a single syscall is an atomic snapshot. To
    // force the multi-syscall enumeration needed here deterministically, a writer thread keeps
    // adding entries so that pass 2 always scans a directory larger than pass 1 measured,
    // guaranteeing a multi-syscall enumeration and hence is_snapshot() == false regardless of
    // kernel under-fill behaviour. The writer is bounded so that the directory can never
    // outgrow the span headroom: an enumeration which ran out of span (done() == false) would
    // not exercise the racy path at all.
    std::atomic<bool> stop2{false};
    std::atomic<size_t> added2{0};
    static constexpr size_t MAX_ADD2 = 8192;  // far below SPAN_HEADROOM, but > the pass 2 sizing slop
    std::thread writer2(
    [&]()
    {
      size_t n = 0;
      while(!stop2.load(std::memory_order_relaxed) && added2.load(std::memory_order_relaxed) < MAX_ADD2)
      {
        char name[32];
        snprintf(name, sizeof(name), "stale%06zu", n++);
        auto fh = file_handle::file(largedirh, name, file_handle::mode::write, file_handle::creation::if_needed,
                                    file_handle::caching::none);
        (void) fh;
        added2.fetch_add(1, std::memory_order_relaxed);
        std::this_thread::yield();
      }
    });
    std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
    buffers_type racy(bigbuf);
    bool got_racy = false;
    for(int attempt = 0; attempt < 30 && !got_racy; attempt++)
    {
      auto r = largedirh.read({std::move(racy), flags::permit_racy_reads, {}, filter::none}, std::chrono::seconds(30));
      BOOST_REQUIRE(r);               // permit_racy_reads must never error out on a snapshot failure
      BOOST_CHECK(r.value().done());  // permit_racy_reads must never return an incomplete enumeration
      if(!r.value().is_snapshot())
      {
        got_racy = true;
      }
      racy = std::move(r.value());
    }
    stop2.store(true);
    writer2.join();
    // With the writer growing the directory, the enumeration must need more than one syscall on Windows
    BOOST_REQUIRE(got_racy);
    std::vector<buffer_type> smallbuf(SMALL_ENTRIES);
    buffers_type reused(span<buffer_type>(smallbuf.data(), smallbuf.size()), std::move(racy));
    auto r2 = smalldirh.read({std::move(reused), {}, filter::none}, std::chrono::seconds(30));
    BOOST_REQUIRE(r2);
    BOOST_CHECK(r2.value().is_snapshot());  // STALESNP: must not be sticky-false from the previous racy read
    BOOST_CHECK(r2.value().done());
    BOOST_CHECK_EQUAL(r2.value().size(), SMALL_ENTRIES);
  }
#endif

  // 3. LIVELOCK + BUFOVRFL + invariants under concurrent modification: a writer thread keeps
  // adding entries while read() runs. This exercises the retry paths (directory changing
  // between pass 1 and pass 2) and the STATUS_BUFFER_OVERFLOW handling.
  std::atomic<bool> stop{false};
  std::atomic<size_t> added{0};
  static constexpr size_t MAX_ADD3 = 8192;  // far below SPAN_HEADROOM, so the span can never be exhausted
  std::thread writer(
  [&]()
  {
    size_t n = 0;
    while(!stop.load(std::memory_order_relaxed) && added.load(std::memory_order_relaxed) < MAX_ADD3)
    {
      char name[32];
      snprintf(name, sizeof(name), "extra%06zu", n++);
      auto fh = file_handle::file(largedirh, name, file_handle::mode::write, file_handle::creation::if_needed,
                                  file_handle::caching::none);
      (void) fh;
      added.fetch_add(1, std::memory_order_relaxed);
      std::this_thread::yield();
    }
  });
  {
    // 3a. flags::none: the only allowed error is errc::operation_would_block (from a
    // BUFFER_OVERFLOW which outgrew the bounded retries); must never be timed_out or hang,
    // and every successful enumeration must be complete.
    for(int i = 0; i < 10; i++)
    {
      std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
      buffers_type req(bigbuf);
      auto r = largedirh.read({std::move(req), {}, filter::none}, std::chrono::seconds(5));
      BOOST_REQUIRE(r || r.error() == errc::operation_would_block);
      if(r)
      {
        BOOST_CHECK(r.value().done());
      }
    }
    // 3b. flags::permit_racy_reads: never error out (in particular never
    // errc::operation_would_block) and never return an incomplete enumeration.
    for(int i = 0; i < 10; i++)
    {
      std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
      buffers_type req(bigbuf);
      auto r = largedirh.read({std::move(req), flags::permit_racy_reads, {}, filter::none}, std::chrono::seconds(30));
      BOOST_REQUIRE(r);
      BOOST_CHECK(r.value().done());
    }
    // 3c. flags::permit_racy_reads with a user-supplied kernel buffer: a BUFFER_OVERFLOW
    // cannot be retried (the user buffer cannot be grown), so the only allowed error is
    // errc::no_buffer_space.
    for(int i = 0; i < 10; i++)
    {
      std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
      std::vector<char> userbuf(256 * 1024);
      buffers_type req(bigbuf);
      auto r = largedirh.read(
      {std::move(req), flags::permit_racy_reads, {}, filter::none, span<char>(userbuf.data(), userbuf.size())},
      std::chrono::seconds(30));
      BOOST_REQUIRE(r || r.error() == errc::no_buffer_space);
      if(r)
      {
        BOOST_CHECK(r.value().done());
      }
    }
  }
  stop.store(true);
  writer.join();

  // 4. SPINLOCK: concurrent read()s on the same directory_handle must all succeed and
  // complete (the lock is acquired once per read(), serialising the kernel scan position).
  {
    std::atomic<size_t> failures{0};
    std::vector<std::thread> threads;
    for(int t = 0; t < 4; t++)
    {
      threads.emplace_back(
      [&]()
      {
        for(int i = 0; i < 5; i++)
        {
          std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
          buffers_type req(bigbuf);
          auto r = largedirh.read({std::move(req), {}, filter::none}, std::chrono::seconds(30));
          if(!r || !r.value().done())
          {
            failures.fetch_add(1, std::memory_order_relaxed);
          }
        }
      });
    }
    for(auto &t : threads)
    {
      t.join();
    }
    BOOST_CHECK_EQUAL(failures.load(), 0u);
  }

  // 5. NOSUCHFL: a glob which matches nothing must return an empty, done, snapshot
  // enumeration. The Windows kernel can answer the first query of such a scan with
  // STATUS_NO_SUCH_FILE (fastfat/ntfs drivers, InitialQuery/First) rather than
  // STATUS_NO_MORE_FILES; both must be treated as end-of-scan.
  {
    std::vector<buffer_type> smallbuf(SMALL_ENTRIES);
    buffers_type req(smallbuf);
    auto r = smalldirh.read({std::move(req), "no_such_glob_*", filter::none}, std::chrono::seconds(30));
    BOOST_REQUIRE(r);
    BOOST_CHECK(r.value().done());
    BOOST_CHECK(r.value().is_snapshot());
    BOOST_CHECK_EQUAL(r.value().size(), 0u);
  }

  // Cleanup
  auto r = algorithm::reduce(std::move(largedirh));
  BOOST_CHECK(r);
  r = algorithm::reduce(std::move(smalldirh));
  BOOST_CHECK(r);
}

KERNELTEST_TEST_KERNEL(unit, llfio, directory_handle_read_snapshot, directory_handle,
                       "Tests the directory_handle::read() snapshot and retry fixes (OPWBLOCK, LIVELOCK, BUFOVRFL, "
                       "STALESNP, SPINLOCK, SLFDEADL)",
                       TestDirectoryHandleReadSnapshot())
