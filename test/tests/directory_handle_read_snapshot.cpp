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

// Joins a std::thread on every exit path, including exceptions unwinding through a BOOST_REQUIRE
// failure. Without this, a failed assertion while a writer thread is still joinable would destroy
// the joinable std::thread, call std::terminate() and crash the process with no failure diagnostics.
struct thread_joiner
{
  std::thread _t;
  explicit thread_joiner(std::thread &&t) noexcept
      : _t(std::move(t))
  {
  }
  thread_joiner(const thread_joiner &) = delete;
  thread_joiner &operator=(const thread_joiner &) = delete;
  thread_joiner(thread_joiner &&o) noexcept
      : _t(std::move(o._t))
  {
  }
  thread_joiner &operator=(thread_joiner &&o) noexcept
  {
    if(this != &o)
    {
      if(_t.joinable())
      {
        _t.join();
      }
      _t = std::move(o._t);
    }
    return *this;
  }
  ~thread_joiner()
  {
    if(_t.joinable())
    {
      _t.join();
    }
  }
};

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
  // 2. STALESNP + buffers_type reuse: after a racy enumeration (is_snapshot() == false),
  // reusing the same buffers_type for a fresh single-syscall enumeration must report
  // is_snapshot() == true again. With flags::permit_racy_reads, a multi-syscall enumeration
  // always returns is_snapshot() == false (no retries are attempted for the NO_MORE_FILES path).
  //
  // This section has two distinct halves with different determinism:
  //   (a) The writer + span-restamp loop below is a DETERMINISTIC regression test for the
  //       documented buffers_type reuse pattern (racy = buffers_type(span(...), std::move(racy)),
  //       reusing the internal kernel buffer across reads). Without the span restamp, read()
  //       resizes the returned span to the enumerated count, so the next read on a grown
  //       directory returns done() == false; that regression would fail here on every kernel.
  //       This half does not depend on racy enumeration and must always pass.
  //   (b) The final STALESNP is_snapshot() assertion is inherently opportunistic: a buffers_type
  //       carrying _snapshot == false can only be obtained from a racy read, and no public API
  //       can force one on a kernel which fills the pass 2 buffer completely (see below). It is
  //       only meaningful when a racy enumeration was actually observed, so it is gated on
  //       got_racy rather than being a hard requirement.
  {
    // Pass 1 sizes the pass 2 kernel buffer to fit the whole directory, over-measuring every
    // entry by 8 bytes plus 1024 bytes of slop (see the pass 1 sizing in
    // windows/directory_handle.ipp), so on kernels which fill that buffer completely (no issue
    // #137 under-fill) a quiescent enumeration completes in a single NtQueryDirectoryFile()
    // call and correctly reports is_snapshot() == true. To try to force the multi-syscall
    // enumeration which reports is_snapshot() == false, a writer thread keeps adding entries
    // while read() runs so that the directory can outgrow the pass 2 buffer between pass 1 and
    // pass 2. This is best-effort, not a guarantee: the directory must grow by more than the
    // built-in 8 bytes/entry over-measurement within a single read() to become racy, so on a
    // completely-filling kernel no racy enumeration may be observed at all (see the
    // BOOST_WARN_MESSAGE below). The writer is bounded (MAX_ADD2) and joined on every exit path
    // via thread_joiner, so an assertion failure here cannot crash the process with
    // std::terminate() on a joinable std::thread.
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
    thread_joiner joiner2(std::move(writer2));
    std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
    buffers_type racy(bigbuf);
    bool got_racy = false;
    for(int attempt = 0; attempt < 30 && !got_racy; attempt++)
    {
      // read() resizes the returned buffers' span to the entries actually enumerated, so
      // reusing the returned buffers as-is would leave the next read with a span too small
      // for the writer-grown directory, guaranteeing done() == false (an incomplete
      // enumeration, which never reports is_snapshot() == false and so would not exercise
      // the racy path at all). Restamp the span back over the full headroom, reusing only
      // the internal kernel buffer, per the documented reuse pattern in read()'s \mallocs.
      racy = buffers_type(span<buffer_type>(bigbuf.data(), bigbuf.size()), std::move(racy));
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
    // joiner2 joins here on scope exit (and on any exception unwinding from the loop above).
    // With the writer growing the directory, a racy enumeration is observed on kernels which
    // under-fill (issue #137); on completely-filling kernels the pass 2 buffer over-measurement
    // absorbs the growth and every enumeration is a single-syscall snapshot. Both are valid, so
    // this is a warning, not a hard failure: only when a racy enumeration IS observed is the
    // STALESNP _snapshot reset below actually exercised.
    BOOST_WARN_MESSAGE(got_racy,
                       "No racy enumeration observed; the STALESNP _snapshot reset is not exercised on this kernel");
    std::vector<buffer_type> smallbuf(SMALL_ENTRIES);
    buffers_type reused(span<buffer_type>(smallbuf.data(), smallbuf.size()), std::move(racy));
    auto r2 = smalldirh.read({std::move(reused), {}, filter::none}, std::chrono::seconds(30));
    BOOST_REQUIRE(r2);
    BOOST_CHECK(r2.value().done());
    BOOST_CHECK_EQUAL(r2.value().size(), SMALL_ENTRIES);
    if(got_racy)
    {
      // STALESNP: the input buffers_type carried _snapshot == false from the racy read, so the
      // fresh single-syscall enumeration must report is_snapshot() == true. When got_racy is
      // false this check would be vacuous (the input never carried _snapshot == false), so it is
      // gated on got_racy.
      BOOST_CHECK(r2.value().is_snapshot());
    }
  }
#endif

  // 3. LIVELOCK + BUFOVRFL + invariants under concurrent modification: a writer thread keeps
  // adding entries while read() runs. This exercises the retry paths (directory changing
  // between pass 1 and pass 2) and the STATUS_BUFFER_OVERFLOW handling.
  {
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
    thread_joiner joiner(std::move(writer));
    // 3a. flags::none: the allowed errors are errc::operation_would_block (from a BUFFER_OVERFLOW
    // which outgrew the bounded retries) and errc::timed_out (the bounded retry loops may exceed
    // the 5s deadline under heavy load); must never hang, and every successful enumeration must
    // be complete.
    for(int i = 0; i < 10; i++)
    {
      std::vector<buffer_type> bigbuf(LARGE_ENTRIES + SPAN_HEADROOM);
      buffers_type req(bigbuf);
      auto r = largedirh.read({std::move(req), {}, filter::none}, std::chrono::seconds(5));
      BOOST_REQUIRE(r || r.error() == errc::operation_would_block || r.error() == errc::timed_out);
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
    // 3c. flags::permit_racy_reads with a user-supplied kernel buffer which is too small for the
    // directory: a BUFFER_OVERFLOW cannot be retried (the user buffer cannot be grown), so the
    // only allowed error is errc::no_buffer_space.
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
    // 3c success path: a user-supplied kernel buffer which IS big enough for a small directory
    // must succeed as a single-syscall snapshot without any allocation.
    {
      std::vector<buffer_type> smallbuf(SMALL_ENTRIES);
      std::vector<char> userbuf(256 * 1024);
      buffers_type req(smallbuf);
      auto r = smalldirh.read(
      {std::move(req), flags::permit_racy_reads, {}, filter::none, span<char>(userbuf.data(), userbuf.size())},
      std::chrono::seconds(30));
      BOOST_REQUIRE(r);
      BOOST_CHECK(r.value().done());
      BOOST_CHECK(r.value().is_snapshot());
      BOOST_CHECK_EQUAL(r.value().size(), SMALL_ENTRIES);
    }
    stop.store(true);
  }  // joiner joins here on scope exit (and on any exception unwinding from the loops above);
     // the writer is fully stopped before section 4.

  // 4. SPINLOCK: concurrent read()s on the same directory_handle must all succeed and
  // complete (the lock is acquired once per read(), serialising the kernel scan position).
  // The writers have fully stopped (joined in section 3), so the directory is stable and every
  // one of the original LARGE_ENTRIES files must be present in a complete enumeration. A broken
  // lock which lets concurrent scans interleave on one handle would corrupt the kernel scan
  // position (e.g. a premature end-of-scan); that most plausibly produces a complete-but-truncated
  // enumeration, which the completeness check below catches even though done() == true.
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
          if(!r || !r.value().done() || r.value().size() < LARGE_ENTRIES)
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
