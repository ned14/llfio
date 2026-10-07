/* Integration test kernel for atomic_file
(C) 2026 d3fault <d3fault@d3fault.net> (1 commit)
File Created: Oct 2026


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
#include <cerrno>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
// For `mode_t` and the process umask, used by the permission preservation test, and for the FIFO
// and directory permissions used by the fallback setup failure test.
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifdef __linux__
// For the fsync() interposition which injects commit() barrier failures, and for the private
// mount namespaces in which the fallback failure tests run.
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifdef __linux__
// ---------------------------------------------------------------------------------------------
// Fault injection for the two barrier steps of commit().
//
// LLFIO has no fault injection mechanism of its own, so this test supplies one at the system call
// boundary, which needs no change to the library and does not weaken it in any way. Defining
// fsync() in the test executable interposes it for the whole program: the linker resolves the
// references LLFIO emits - whether LLFIO was linked statically, header only or as a shared library
// - to the definition below, because the executable comes first in the symbol lookup scope and no
// `-Bsymbolic` was used. Both barrier steps of commit() reach the system through fsync():
// `byte_io_handle::barrier(barrier_kind::wait_all)` calls fsync() on the temporary file, and
// `directory_handle::barrier()` calls fsync() on the containing directory.
//
// The override is completely transparent until armed, and once armed it fails only one *kind* of
// descriptor, told apart with fstat(), so that the temporary file's barrier and the directory's
// barrier can be failed independently of one another. The real work is done with the fsync system
// call directly, so that this definition cannot recurse into itself.
enum class fail_fsync
{
  none,       //!< Transparent: pass everything through to the real fsync.
  regular,    //!< Fail only barriers of regular files, i.e. step (ii) of commit().
  directory   //!< Fail only barriers of directories, i.e. step (iv) of commit().
};

static fail_fsync g_fail_fsync = fail_fsync::none;
static int g_fail_fsync_errno = EIO;
static unsigned g_fail_fsync_injected = 0;

extern "C" int fsync(int fd)
{
  if(g_fail_fsync != fail_fsync::none)
  {
    struct stat s
    {
    };
    if(::fstat(fd, &s) == 0)
    {
      const bool is_directory = S_ISDIR(s.st_mode) != 0;
      const bool fail_this_one = (g_fail_fsync == fail_fsync::regular) ? !is_directory : is_directory;
      if(fail_this_one)
      {
        g_fail_fsync_injected++;
        errno = g_fail_fsync_errno;
        return -1;
      }
    }
  }
  return static_cast<int>(::syscall(SYS_fsync, fd));
}
#endif

namespace llfio = LLFIO_V2_NAMESPACE;

using atomic_file = llfio::atomic_file;

static llfio::byte_io_handle::const_buffer_type buf(const std::string &s)
{
  return llfio::byte_io_handle::const_buffer_type(reinterpret_cast<const llfio::byte *>(s.data()), s.size());
}

static std::string read_file(const llfio::path_handle &base, llfio::path_view p)
{
  auto fh = llfio::file_handle::file(base, p);
  BOOST_REQUIRE(fh);
  auto size = fh.value().maximum_extent();
  BOOST_REQUIRE(size);
  std::string ret(size.value(), '\0');
  if(!ret.empty())
  {
    auto readed = llfio::read(fh.value(), 0, {{reinterpret_cast<llfio::byte *>(ret.data()), ret.size()}});
    BOOST_REQUIRE(readed);
    ret.resize(readed.value());
  }
  return ret;
}

static size_t count_random_files(const llfio::filesystem::path &p)
{
  size_t ret = 0;
  std::error_code ec;
  for(auto it = llfio::filesystem::directory_iterator(p, ec); !ec && it != llfio::filesystem::directory_iterator();
      it.increment(ec))
  {
    const std::string name = it->path().filename().string();
    if(name.size() > 7 && name.compare(name.size() - 7, 7, ".random") == 0)
    {
      ret++;
    }
  }
  return ret;
}

// Create a brand new file, and check its content is what we wrote.
static void TestCreateNewFile(const llfio::directory_handle &dirh)
{
  {
    atomic_file f = atomic_file::open(dirh, "newfile.txt").value();
    BOOST_CHECK(f.current_state() == atomic_file::state::open);
    BOOST_CHECK(!f.is_fallback());
    BOOST_CHECK(!f.replacement_performed());
    BOOST_CHECK(f.write(buf("hello world")).value() == 11);
    BOOST_CHECK(f.current_state() == atomic_file::state::open);
    // The destination must not exist before commit
    BOOST_CHECK(!llfio::file_handle::file(dirh, "newfile.txt"));
    BOOST_CHECK(f.commit());
    BOOST_CHECK(f.current_state() == atomic_file::state::committed);
    BOOST_CHECK(f.replacement_performed());
  }
  BOOST_CHECK(read_file(dirh, "newfile.txt") == "hello world");
}

// Overwrite an existing file, including with a shorter content (so stale trailing bytes must go).
static void TestOverwriteExisting(const llfio::directory_handle &dirh)
{
  {
    atomic_file f = atomic_file::open(dirh, "existing.txt").value();
    BOOST_CHECK(f.write(buf("0123456789")).value() == 10);
    BOOST_CHECK(f.commit());
  }
  BOOST_CHECK(read_file(dirh, "existing.txt") == "0123456789");
  {
    atomic_file f = atomic_file::open(dirh, "existing.txt").value();
    BOOST_CHECK(f.write(buf("abc")).value() == 3);
    BOOST_CHECK(f.commit());
  }
  BOOST_CHECK(read_file(dirh, "existing.txt") == "abc");
}

#ifndef _WIN32
// The full path of a leaf within the test directory.
static std::string path_of(const llfio::directory_handle &dirh, llfio::path_view p)
{
  return (dirh.current_path().value() / p.path()).string();
}

// Reads a path's permission bits with a plain POSIX stat, deliberately independently of the
// mechanism `atomic_file` uses, so that this checks the observable outcome rather than repeating the
// implementation's own logic. Note this needs no permission on the file itself, only on its parent.
static uint16_t perms_of(const llfio::directory_handle &dirh, llfio::path_view p)
{
  const std::string full = path_of(dirh, p);
  struct stat s
  {
  };
  BOOST_REQUIRE(::stat(full.c_str(), &s) == 0);
  return static_cast<uint16_t>(s.st_mode & 0xfff);
}

// Creates a file if needed, then sets its permission bits with a plain POSIX chmod.
static bool set_perms(const llfio::directory_handle &dirh, llfio::path_view p, uint16_t mode)
{
  auto fh = llfio::file_handle::file(dirh, p, llfio::file_handle::mode::write, llfio::file_handle::creation::if_needed);
  if(!fh)
  {
    return false;
  }
  BOOST_REQUIRE(fh.value().close());
  return ::chmod(path_of(dirh, p).c_str(), static_cast<mode_t>(mode)) == 0;
}

// Replacing an existing file must leave its permission bits alone, as QSaveFile does. Without that
// an atomic save silently changes the mode of the destination: it drops the executable bit off a
// script, and it widens a file the caller had tightened.
static void TestPreservePermissions(const llfio::directory_handle &dirh)
{
  /* Read the process umask using the standard round trip. A temporary file is created with mode
  0660 *before* the umask is applied, so that is the mode a replaced destination would be left with
  were its own permissions not preserved. Every mode tested below is unequal to it for any umask, so
  a failure here means a failure to preserve, and not something else.
  */
  const mode_t um = ::umask(0);
  ::umask(um);
  const uint16_t unpreserved = static_cast<uint16_t>(0660 & ~um);
  // 0644 and 0755 set "other" bits, and 0700 sets an owner execute bit, none of which the creation
  // mode 0660 can ever set. 0600 and 0640 are the realistic tightened modes.
  const uint16_t modes[] = {0600, 0640, 0644, 0700, 0755};
  // Precondition: this filing system must be able to report a mode at all. Note this is a skip, not
  // a failure: a filing system with no permissions cannot have lost any.
  if(!set_perms(dirh, "perms_probe.txt", 0644) || perms_of(dirh, "perms_probe.txt") != 0644)
  {
    BOOST_TEST_MESSAGE("this filing system does not support permissions, skipping");
    return;
  }
  size_t index = 0;
  for(const uint16_t mode : modes)
  {
    const std::string name = "perms" + std::to_string(index++) + ".txt";
    if(mode == unpreserved)
    {
      continue;  // this one would be preserved by accident, so it could not detect a failure
    }
    BOOST_REQUIRE(set_perms(dirh, name, mode));
    {
      atomic_file f = atomic_file::open(dirh, name).value();
      BOOST_CHECK(f.write(buf("contents")).value() == 8);
      BOOST_CHECK(f.commit());
    }
    BOOST_CHECK(read_file(dirh, name) == "contents");
    const uint16_t got = perms_of(dirh, name);
    if(got != mode)
    {
      /* NOTE: in this test framework BOOST_CHECK_MESSAGE only prints, it does not register a
      failure. So print the diagnosis here, and register the failure with a plain BOOST_CHECK. */
      BOOST_TEST_MESSAGE("destination mode " << std::oct << mode << " became " << std::oct << got);
    }
    BOOST_CHECK(got == mode);
  }
  {
    // A destination which the caller cannot even read must still keep its mode: it is read through
    // an anchor handle, which needs no permission on the destination itself. Its contents cannot be
    // read back here for that same reason, so this part checks the mode alone.
    BOOST_REQUIRE(set_perms(dirh, "perms0000.txt", 0000));
    BOOST_CHECK_EQUAL(perms_of(dirh, "perms0000.txt"), 0000);
    atomic_file f = atomic_file::open(dirh, "perms0000.txt").value();
    BOOST_CHECK(f.write(buf("contents")).value() == 8);
    BOOST_CHECK(f.commit());
    BOOST_CHECK_EQUAL(perms_of(dirh, "perms0000.txt"), 0000);
  }
  {
    // A destination which does not exist has no mode to preserve, so the new file keeps the mode the
    // temporary file was created with. This feature deliberately does not invent permissions for a
    // file which was not there before.
    atomic_file f = atomic_file::open(dirh, "perms_new.txt").value();
    BOOST_CHECK(f.write(buf("fresh")).value() == 5);
    BOOST_CHECK(f.commit());
    BOOST_CHECK(read_file(dirh, "perms_new.txt") == "fresh");
    BOOST_CHECK_EQUAL(perms_of(dirh, "perms_new.txt"), unpreserved);
  }
}
#endif

// Multiple sequential writes must concatenate, and must go through the tracked offset.
static void TestMultipleWrites(const llfio::directory_handle &dirh)
{
  {
    atomic_file f = atomic_file::open(dirh, "multi.txt").value();
    for(unsigned n = 0; n < 1000; n++)
    {
      std::string s = std::to_string(n) + ",";
      BOOST_CHECK(f.write(buf(s)).value() == s.size());
    }
    BOOST_CHECK(f.commit());
  }
  std::string expected;
  for(unsigned n = 0; n < 1000; n++)
  {
    expected += std::to_string(n) + ",";
  }
  BOOST_CHECK(read_file(dirh, "multi.txt") == expected);
}

// A zero length write is a no-op, not an error.
static void TestEmptyWrite(const llfio::directory_handle &dirh)
{
  {
    atomic_file f = atomic_file::open(dirh, "empty.txt").value();
    BOOST_CHECK(f.write(buf("")).value() == 0);
    BOOST_CHECK(f.commit());
  }
  BOOST_CHECK(read_file(dirh, "empty.txt").empty());
}

// Destroying without committing must leave the destination untouched, and must clean up the temp.
static void TestDiscardOnDestruction(const llfio::directory_handle &dirh)
{
  {
    atomic_file f = atomic_file::open(dirh, "discarded.txt").value();
    BOOST_CHECK(f.write(buf("this should never appear")).value() > 0);
    BOOST_CHECK(!f.replacement_performed());
    // No commit: destructor discards
  }
  BOOST_CHECK(!llfio::file_handle::file(dirh, "discarded.txt"));
  // And no temporary files may be left behind
  BOOST_CHECK(count_random_files(dirh.current_path().value()) == 0);
}

// Cancelling must prevent publication, and committing afterwards must fail.
static void TestCancel(const llfio::directory_handle &dirh)
{
  {
    atomic_file f = atomic_file::open(dirh, "cancelled.txt").value();
    BOOST_CHECK(f.write(buf("nope")).value() == 4);
    BOOST_CHECK(f.cancel());
    BOOST_CHECK(f.current_state() == atomic_file::state::cancelled);
    BOOST_CHECK(!f.replacement_performed());
    // Writing after cancellation is not permitted
    auto w = f.write(buf("more"));
    BOOST_REQUIRE(w.has_error());
    BOOST_CHECK(w.error() == llfio::errc::operation_not_permitted);
    // Committing after cancellation must not publish
    auto c = f.commit();
    BOOST_REQUIRE(c.has_error());
    BOOST_CHECK(c.error() == llfio::errc::operation_canceled);
    // Cancelling twice is harmless
    BOOST_CHECK(f.cancel());
  }
  BOOST_CHECK(!llfio::file_handle::file(dirh, "cancelled.txt"));
}

// Committing twice is not permitted.
static void TestDoubleCommit(const llfio::directory_handle &dirh)
{
  atomic_file f = atomic_file::open(dirh, "double.txt").value();
  BOOST_CHECK(f.write(buf("x")).value() == 1);
  BOOST_CHECK(f.commit());
  auto c = f.commit();
  BOOST_REQUIRE(c.has_error());
  BOOST_CHECK(c.error() == llfio::errc::operation_not_permitted);
  // And writing after commit is not permitted
  auto w = f.write(buf("y"));
  BOOST_REQUIRE(w.has_error());
  BOOST_CHECK(w.error() == llfio::errc::operation_not_permitted);
  BOOST_CHECK(read_file(dirh, "double.txt") == "x");
}

// A path with a directory component is resolved relative to the base handle.
static void TestPathWithDirectory(const llfio::directory_handle &dirh)
{
  std::error_code ec;
  llfio::filesystem::create_directory(dirh.current_path().value() / "subdir", ec);
  BOOST_REQUIRE(!ec);
  {
    atomic_file f = atomic_file::open(dirh, "subdir/leaf.txt").value();
    BOOST_CHECK(f.write(buf("in a subdirectory")).value() == 17);
    BOOST_CHECK(f.commit());
  }
  BOOST_CHECK(read_file(dirh, "subdir/leaf.txt") == "in a subdirectory");
}

// Opening into a directory which does not exist must fail.
static void TestOpenIntoNonexistentDirectory(const llfio::directory_handle &dirh)
{
  auto f = atomic_file::open(dirh, "no/such/dir/leaf.txt");
  BOOST_REQUIRE(f.has_error());
}

// Replacing a path which is actually a directory must fail at commit, and leave the directory
// alone. This is the natural way to make the atomic replacement itself fail: renaming a file over
// a directory is refused by the filing system.
static void TestDestinationIsDirectory(const llfio::directory_handle &dirh)
{
  std::error_code ec;
  llfio::filesystem::create_directory(dirh.current_path().value() / "adir", ec);
  BOOST_REQUIRE(!ec);
  {
    atomic_file f = atomic_file::open(dirh, "adir").value();
    BOOST_CHECK(f.write(buf("not allowed")).value() == 11);
    // Step (ii), the temporary file's barrier, must have succeeded, so what fails below is the
    // replacement itself and not something earlier.
    auto c = f.commit();
    BOOST_REQUIRE(c.has_error());
    // The destination has not been replaced, the transaction is failed rather than committed, and
    // it is still the atomic transaction it was - a commit failure must never quietly turn into a
    // non-atomic write of the destination.
    BOOST_CHECK(!f.replacement_performed());
    BOOST_CHECK(f.current_state() == atomic_file::state::failed);
    BOOST_CHECK(!f.is_fallback());
    // A second commit() must not try the replacement again: the transaction is failed, so it is
    // reported as such rather than re-attempting anything.
    auto c2 = f.commit();
    BOOST_REQUIRE(c2.has_error());
    BOOST_CHECK(c2.error() == llfio::errc::operation_canceled);
  }
  // The destination directory is untouched, and the temporary file has been cleaned up.
  BOOST_CHECK(llfio::filesystem::is_directory(dirh.current_path().value() / "adir", ec));
  BOOST_CHECK(count_random_files(dirh.current_path().value()) == 0);
}

#ifdef __linux__
// Step (ii) of commit(), the durability barrier of the temporary file, must fail the transaction
// without touching the destination, and the temporary file must be cleaned up.
static void TestCommitBarrierFails(const llfio::directory_handle &dirh)
{
  const llfio::filesystem::path dirpath = dirh.current_path().value();
  {
    std::ofstream(dirpath / "barrier.txt") << "original";
  }
  BOOST_REQUIRE(llfio::filesystem::exists(dirpath / "barrier.txt"));
  {
    atomic_file f = atomic_file::open(dirh, "barrier.txt").value();
    BOOST_CHECK(f.write(buf("replacement")).value() == 11);
    g_fail_fsync_injected = 0;
    g_fail_fsync = fail_fsync::regular;
    auto c = f.commit();
    g_fail_fsync = fail_fsync::none;
    BOOST_REQUIRE(c.has_error());
    BOOST_CHECK(c.error() == llfio::errc::io_error);
    // Exactly the temporary file's barrier was failed - not the directory's, and not something
    // else entirely.
    BOOST_CHECK_EQUAL(g_fail_fsync_injected, 1);
    // The destination has not been replaced, and the transaction is failed rather than committed.
    BOOST_CHECK(f.current_state() == atomic_file::state::failed);
    BOOST_CHECK(!f.replacement_performed());
    BOOST_CHECK(!f.is_fallback());
    auto c2 = f.commit();
    BOOST_REQUIRE(c2.has_error());
    BOOST_CHECK(c2.error() == llfio::errc::operation_canceled);
  }
  // The destination still holds its original contents, and the temporary file has been discarded.
  BOOST_CHECK(read_file(dirh, "barrier.txt") == "original");
  BOOST_CHECK(count_random_files(dirpath) == 0);
}

// Step (iv) of commit(), the flush of the containing directory, is the one step which can fail
// *after* the destination has already been replaced. The replacement cannot be undone, so what
// matters is that the object reports the failure without pretending the destination is untouched,
// that it reports the error, and that it neither undoes nor unlinks what it has published, nor
// attempts the replacement a second time.
static void TestDirectoryBarrierFails(const llfio::directory_handle &dirh)
{
  const llfio::filesystem::path dirpath = dirh.current_path().value();
  {
    std::ofstream(dirpath / "dirbarrier.txt") << "original";
  }
  BOOST_REQUIRE(llfio::filesystem::exists(dirpath / "dirbarrier.txt"));
  {
    atomic_file f = atomic_file::open(dirh, "dirbarrier.txt").value();
    BOOST_CHECK(f.write(buf("replacement")).value() == 11);
    BOOST_CHECK(!f.replacement_performed());
    g_fail_fsync_injected = 0;
    g_fail_fsync = fail_fsync::directory;
    auto c = f.commit();
    g_fail_fsync = fail_fsync::none;
    // The directory's barrier failed, and it was the only injected failure, so the temporary
    // file's own barrier and the replacement itself really did succeed.
    BOOST_REQUIRE(c.has_error());
    BOOST_CHECK(c.error() == llfio::errc::io_error);
    BOOST_CHECK_EQUAL(g_fail_fsync_injected, 1);
    // The rename is a single atomic operation and happened before the directory flush, so the
    // destination *has* been replaced even though commit() failed. replacement_performed() must
    // say so, so that the caller can tell this apart from a failure which did not publish.
    BOOST_CHECK(f.replacement_performed());
    BOOST_CHECK(f.current_state() == atomic_file::state::committed);
    BOOST_CHECK(!f.is_fallback());
    BOOST_CHECK(read_file(dirh, "dirbarrier.txt") == "replacement");
    // A subsequent commit() must not attempt the replacement again. It reports the transaction as
    // already committed, rather than re-renaming anything.
    auto c2 = f.commit();
    BOOST_REQUIRE(c2.has_error());
    BOOST_CHECK(c2.error() == llfio::errc::operation_not_permitted);
    BOOST_CHECK_EQUAL(g_fail_fsync_injected, 1);
  }
  // Destroying the transaction must not undo or unlink the destination it has just published.
  BOOST_CHECK(read_file(dirh, "dirbarrier.txt") == "replacement");
  BOOST_CHECK(count_random_files(dirpath) == 0);
}
#else
static void TestCommitBarrierFails(const llfio::directory_handle &)
{
  BOOST_TEST_MESSAGE("Commit barrier failure test needs to interpose fsync(), which is Linux only. So skipping this test.");
}
static void TestDirectoryBarrierFails(const llfio::directory_handle &)
{
  BOOST_TEST_MESSAGE("Directory barrier failure test needs to interpose fsync(), which is Linux only. So skipping this test.");
}
#endif

// A concurrent reader must never observe a partially written destination.
static void TestReaderNeverSeesPartial(const llfio::directory_handle &dirh)
{
  const size_t len = 256 * 1024;
  {
    atomic_file f = atomic_file::open(dirh, "concurrent.txt").value();
    std::string s(len, 'A');
    BOOST_CHECK(f.write(buf(s)).value() == len);
    BOOST_CHECK(f.commit());
  }
  std::atomic<bool> stop{false};
  std::atomic<size_t> reads{0};
  std::atomic<bool> torn{false};
  std::thread reader(
  [&]
  {
    while(!stop.load(std::memory_order_acquire))
    {
      auto fh = llfio::file_handle::file(dirh, "concurrent.txt");
      if(!fh)
      {
        continue;
      }
      std::string s(len, '\0');
      auto r = llfio::read(fh.value(), 0, {{reinterpret_cast<llfio::byte *>(s.data()), s.size()}});
      if(!r)
      {
        continue;
      }
      s.resize(r.value());
      if(!s.empty())
      {
        const char c = s[0];
        for(const char x : s)
        {
          if(x != c)
          {
            torn.store(true, std::memory_order_relaxed);
            break;
          }
        }
      }
      reads.fetch_add(1, std::memory_order_relaxed);
    }
  });
  // Stop and join the reader before any `BOOST_REQUIRE` is allowed to return from this function:
  // an early return would leave `reader` joinable, and its destructor would call `std::terminate`,
  // masking the real failure with a crash.
  auto stop_and_join = [&]
  {
    stop.store(true, std::memory_order_release);
    reader.join();
  };
  for(unsigned gen = 1; gen <= 200 && !torn.load(std::memory_order_relaxed); gen++)
  {
    auto opened = atomic_file::open(dirh, "concurrent.txt");
    if(!opened)
    {
      stop_and_join();
      BOOST_REQUIRE(opened);
      return;
    }
    atomic_file f = std::move(opened).value();
    std::string s(len, static_cast<char>('A' + (gen % 26)));
    auto written = f.write(buf(s));
    if(!written)
    {
      stop_and_join();
      BOOST_REQUIRE(written);
      return;
    }
    auto committed = f.commit();
    if(!committed)
    {
      stop_and_join();
      BOOST_REQUIRE(committed);
      return;
    }
  }
  stop.store(true, std::memory_order_release);
  reader.join();
  BOOST_CHECK(!torn.load());
  BOOST_CHECK(reads.load() > 0);
  {
    std::string s = read_file(dirh, "concurrent.txt");
    BOOST_REQUIRE(s.size() == len);
    const char c = s[0];
    for(const char x : s)
    {
      BOOST_REQUIRE(x == c);
    }
  }
}

// The non-atomic fallback must be used when a temp file cannot be created, must write the
// destination directly, and must refuse to pretend cancellation succeeded.
static void TestFallback(const llfio::directory_handle &dirh)
{
  const llfio::filesystem::path dirpath = dirh.current_path().value();
  const llfio::filesystem::path ropath = dirpath / "ro";
  std::error_code ec;
  llfio::filesystem::create_directory(ropath, ec);
  BOOST_REQUIRE(!ec);
  {
    std::ofstream(ropath / "target.txt") << "original";
  }
  // Make the directory unwritable, but leave the file writable.
  llfio::filesystem::permissions(ropath, llfio::filesystem::perms::owner_read | llfio::filesystem::perms::owner_exec,
                                 llfio::filesystem::perm_options::replace, ec);
  BOOST_REQUIRE(!ec);
  auto rodirh = llfio::directory_handle::directory(dirh, "ro", llfio::file_handle::mode::read).value();
  {
    // Without fallback, this must fail because the temporary file cannot be created.
    auto f = atomic_file::open(rodirh, "target.txt");
    BOOST_REQUIRE(f.has_error());
    // The class of failure this test relies on is exactly the one the fallback exists for: the
    // directory refuses to let this process add a new name to it. Pinning the error here keeps the
    // fallback's trigger honest - if error classification changed, this would stop matching.
    BOOST_CHECK(f.error() == llfio::errc::permission_denied);
    // With fallback, it must succeed, and report that it is in fallback mode.
    atomic_file g = atomic_file::open(rodirh, "target.txt", atomic_file::options(true)).value();
    BOOST_CHECK(g.is_fallback());
    // Fallback opens the destination for direct writing and truncates it immediately, so the
    // destination has already been modified before a single byte is written, and the previous
    // contents are already gone. replacement_performed() must report that honestly rather than
    // claiming the destination is untouched because commit() has not run yet.
    BOOST_CHECK(g.replacement_performed());
    BOOST_CHECK(read_file(rodirh, "target.txt").empty());
    BOOST_CHECK(g.write(buf("replaced")).value() == 8);
    // In fallback mode, cancellation cannot be implemented honestly.
    auto c = g.cancel();
    BOOST_REQUIRE(c.has_error());
    BOOST_CHECK(c.error() == llfio::errc::not_supported);
    // Cancellation is refused and the state is unchanged, so commit still works.
    BOOST_CHECK(g.commit());
    BOOST_CHECK(g.current_state() == atomic_file::state::committed);
    BOOST_CHECK(g.replacement_performed());
  }
  BOOST_CHECK(read_file(rodirh, "target.txt") == "replaced");
  // Restore permissions so cleanup can proceed.
  llfio::filesystem::permissions(ropath, llfio::filesystem::perms::owner_all, llfio::filesystem::perm_options::replace,
                                 ec);
}

// `options(true)` means "falling back is permitted", not "this transaction is in fallback mode".
// When the temporary file *can* be created, an opted-in transaction must be an entirely ordinary
// atomic one, and must report itself as such.
//
// This is a regression test: `open()` used to initialise `_fallback` from `opts.fallback` before it
// had even tried to create the temporary file, so an opted-in transaction in an ordinary writable
// directory believed it was in fallback mode. `is_fallback()` returned true, `cancel()` refused as
// unimplementable, and `_write_target()` aimed at `_destination` - which at that point had never
// been opened - so every write went to an invalid handle instead of the temporary file.
static void TestFallbackPermittedButUnused(const llfio::directory_handle &dirh)
{
  const llfio::filesystem::path dirpath = dirh.current_path().value();
  {
    std::ofstream(dirpath / "fallback_allowed.txt") << "original";
  }
  BOOST_REQUIRE(llfio::filesystem::exists(dirpath / "fallback_allowed.txt"));
  const size_t temps_before = count_random_files(dirpath);
  {
    atomic_file f = atomic_file::open(dirh, "fallback_allowed.txt", atomic_file::options(true)).value();
    // The temporary file was created, so the fallback was not taken: this is a normal transaction
    // which merely happened to be *allowed* to fall back.
    BOOST_CHECK(!f.is_fallback());
    BOOST_CHECK(!f.replacement_performed());
    BOOST_CHECK(f.current_state() == atomic_file::state::open);
    // An ordinary transaction owns a newly created temporary file, and has not touched the
    // destination yet.
    BOOST_CHECK_EQUAL(count_random_files(dirpath), temps_before + 1);
    auto w = f.write(buf("replacement"));
    BOOST_REQUIRE(!w.has_error());
    BOOST_CHECK(w.value() == 11);
    // The bytes went to the temporary file, not to the destination: the destination still holds its
    // previous contents. Had `_fallback` been copied from the caller's option, the write would have
    // bypassed the temporary file and the destination would already read "replacement" here.
    BOOST_CHECK(read_file(dirh, "fallback_allowed.txt") == "original");
    BOOST_CHECK(!f.replacement_performed());
    BOOST_CHECK(f.commit());
    BOOST_CHECK(f.current_state() == atomic_file::state::committed);
    BOOST_CHECK(f.replacement_performed());
    // And it is refused a second time, exactly as for any other transaction.
    auto c2 = f.commit();
    BOOST_REQUIRE(c2.has_error());
    BOOST_CHECK(c2.error() == llfio::errc::operation_not_permitted);
  }
  // The temporary file really was relinked onto the destination: the new contents are there and no
  // temporary is left behind.
  BOOST_CHECK(read_file(dirh, "fallback_allowed.txt") == "replacement");
  BOOST_CHECK_EQUAL(count_random_files(dirpath), temps_before);
  // The same holds when the destination does not exist yet.
  {
    atomic_file f = atomic_file::open(dirh, "fallback_allowed_new.txt", atomic_file::options(true)).value();
    BOOST_CHECK(!f.is_fallback());
    auto w = f.write(buf("fresh"));
    BOOST_REQUIRE(!w.has_error());
    BOOST_CHECK(w.value() == 5);
    // Nothing is published until commit(), so the new destination must not exist yet.
    BOOST_CHECK(!llfio::file_handle::file(dirh, "fallback_allowed_new.txt"));
    BOOST_CHECK(!f.replacement_performed());
    BOOST_CHECK(f.commit());
    BOOST_CHECK(f.replacement_performed());
  }
  BOOST_CHECK(read_file(dirh, "fallback_allowed_new.txt") == "fresh");
  BOOST_CHECK_EQUAL(count_random_files(dirpath), temps_before);
  // Cancelling an opted-in but non-fallback transaction must work, because it is an ordinary
  // transaction and cancellation is perfectly implementable for it. This is the other visible
  // symptom of the bug: with `_fallback` wrongly true, `cancel()` returned `not_supported`.
  {
    atomic_file f = atomic_file::open(dirh, "fallback_allowed_cancel.txt", atomic_file::options(true)).value();
    BOOST_CHECK(!f.is_fallback());
    auto w = f.write(buf("doomed"));
    BOOST_REQUIRE(!w.has_error());
    BOOST_CHECK(w.value() == 6);
    BOOST_CHECK(f.cancel());
    BOOST_CHECK(f.current_state() == atomic_file::state::cancelled);
    auto c = f.commit();
    BOOST_REQUIRE(c.has_error());
    BOOST_CHECK(c.error() == llfio::errc::operation_canceled);
  }
  // Cancellation was honoured: the destination was never created, and the temporary file is gone.
  BOOST_CHECK(!llfio::file_handle::file(dirh, "fallback_allowed_cancel.txt"));
  BOOST_CHECK_EQUAL(count_random_files(dirpath), temps_before);
}

// Regression test for a fallback-mode transaction damaging the caller's destination.
//
// `open()` can fail part way through setting up the fallback, after it has taken a handle to the
// real destination but before it has recorded that the destination was modified. The partially
// constructed transaction is then destroyed, and its destructor must not unlink the destination.
//
// The reachable way to get there needs a destination which opens for writing but cannot be
// truncated, so use a FIFO with its read end held open: opening it for writing succeeds,
// `ftruncate()` fails, and `open()` then reports that failure and destroys the transaction. A
// symlink to the null device would also fail to truncate, but it hides any wrongful unlink,
// because LLFIO's safety unlink refuses to remove an entry which does not match the handle.
//
// This test used to run in a *writable* directory, reaching the fallback by exhausting the
// filesystem's inodes, which is what let a wrongful unlink actually succeed and so be observable.
// Now that the fallback trigger is narrowed to `errc::permission_denied` (see atomic_file.hpp) the
// only way into fallback setup is a directory which refuses new names - and such a directory
// refuses unlinks too, so a wrongful unlink there cannot be observed even if it existed. The
// scenario is kept because the invariant is still worth guarding, but on its own it no longer
// proves the destructor is safe: what does is the `_destination`/`_tempfile` split in
// atomic_file.hpp. Observing it again would need the fallback trigger set to be widened first.
static void TestFallbackSetupFailure(const llfio::directory_handle &dirh)
{
#ifndef _WIN32
  if(::geteuid() == 0)
  {
    // Folder permissions do not apply to root, so the fallback could not be triggered this way.
    BOOST_TEST_MESSAGE("Fallback setup failure test cannot run as root. So skipping this test.");
    return;
  }
  const llfio::filesystem::path dirpath = dirh.current_path().value();
  const llfio::filesystem::path fdir = dirpath / "fallback_setup";
  std::error_code ec;
  llfio::filesystem::create_directory(fdir, ec);
  BOOST_REQUIRE(!ec);
  const std::string target = (fdir / "target.txt").string();
  BOOST_REQUIRE(::mkfifo(target.c_str(), 0644) == 0);
  // Hold the read end open: that is what stops opening the FIFO for writing from blocking.
  const int fifo_reader = ::open(target.c_str(), O_RDONLY | O_NONBLOCK);
  BOOST_REQUIRE(fifo_reader >= 0);
  {
    // A directory which refuses to let this process add a new name to it. That is the only failure
    // which reaches the fallback, and it is what makes the temporary file uncreatable here.
    BOOST_REQUIRE(::chmod(fdir.string().c_str(), 0500) == 0);
    auto fdirh = llfio::directory_handle::directory(dirh, "fallback_setup", llfio::file_handle::mode::read);
    BOOST_REQUIRE(fdirh);
    auto f = atomic_file::open(fdirh.value(), "target.txt", atomic_file::options(true));
    BOOST_REQUIRE(f.has_error());
    // The destination is a FIFO, so it opens for writing but cannot be truncated: the failure must
    // be that truncation, not some earlier step of the fallback setup.
    BOOST_CHECK(f.error() == llfio::errc::invalid_argument);
    // `open()` destroys the partially constructed transaction before returning to us.
  }
  ::close(fifo_reader);
  struct stat st
  {
  };
  BOOST_REQUIRE(::lstat(target.c_str(), &st) == 0);
  BOOST_CHECK(S_ISFIFO(st.st_mode));
  // Let the test directory be cleaned up again.
  BOOST_REQUIRE(::chmod(fdir.string().c_str(), 0700) == 0);
#else
  (void) dirh;
  BOOST_TEST_MESSAGE("Fallback setup failure test needs POSIX FIFOs. So skipping this test.");
#endif
}

// The fallback must activate for exactly one class of failure to create the temporary file, and
// must not activate for any other - even when direct writing *would* have worked. This is the
// case which makes that distinction observable: a filing system with no free inodes left refuses
// to create the temporary file with `ENOSPC`, while the destination's own existing inode can
// still be opened for writing. The broad "fall back for any reason" behaviour would therefore
// silently truncate the destination here; the documented behaviour reports the `ENOSPC` and
// leaves the destination intact.
//
// Like all such tests here it needs a filesystem whose inodes we control, so it runs in a child
// process inside a private mount namespace, and skips when neither root nor usable unprivileged
// user namespaces are available.
#ifndef __linux__
static void TestFallbackErrorClass(const llfio::directory_handle &)
{
  BOOST_TEST_MESSAGE(
  "Fallback error class test needs tmpfs and mount namespaces, which are Linux only. So skipping this test.");
}
#else
// Write a small string to a pseudo-file, used to configure user namespace id maps.
static bool write_pseudofile(const char *path, const char *contents)
{
  const int fd = ::open(path, O_WRONLY);
  if(fd < 0)
  {
    return false;
  }
  const size_t len = ::strlen(contents);
  const bool ok = ::write(fd, contents, len) == (ssize_t) len;
  ::close(fd);
  return ok;
}

// Joins a private mount namespace, which is what lets a test mount its own tiny tmpfs without
// affecting anything else. This needs either root or usable unprivileged user namespaces, so the
// child reports 77 when it cannot get one. Runs in the child process; never returns.
static void enter_private_mount_namespace(int userns_ready_fd, int userns_go_fd)
{
  if(::geteuid() != 0)
  {
    // Become root inside a new user namespace, which is enough to mount tmpfs.
    if(::unshare(CLONE_NEWUSER) != 0)
    {
      ::_exit(77);
    }
    // The id maps have to be written by some *other* process: having just called unshare(CLONE_NEWUSER)
    // we no longer hold the capability to write our own. Hand over to the parent and wait for it.
    if(::write(userns_ready_fd, "1", 1) != 1)
    {
      ::_exit(77);
    }
    char go = 0;
    if(::read(userns_go_fd, &go, 1) != 1)
    {
      ::_exit(77);
    }
  }
  if(::unshare(CLONE_NEWNS) != 0)
  {
    ::_exit(77);
  }
  // Stop the mount below propagating back into the parent's namespace.
  (void) ::mount("none", "/", nullptr, MS_REC | MS_PRIVATE, nullptr);
}

// Runs `child` in a forked process which is given a private mount namespace and a mountpoint to
// mount a tmpfs on. Returns the child's exit code, or -1 when the environment cannot host such a
// test at all, in which case the caller must skip rather than report a result it did not obtain.
static int run_in_private_mount_namespace(int (*child)(int, int, const char *), const std::string &mountpoint)
{
  // Writing to a pipe whose reader has already gone would raise SIGPIPE, which must not abort the run.
  (void) ::signal(SIGPIPE, SIG_IGN);
  int userns_ready[2] = {-1, -1};
  int userns_go[2] = {-1, -1};
  BOOST_REQUIRE(::pipe(userns_ready) == 0);
  BOOST_REQUIRE(::pipe(userns_go) == 0);
  const pid_t pid = ::fork();
  BOOST_REQUIRE(pid >= 0);
  if(pid == 0)
  {
    ::close(userns_ready[0]);
    ::close(userns_go[1]);
    ::_exit(child(userns_ready[1], userns_go[0], mountpoint.c_str()));
  }
  ::close(userns_ready[1]);
  ::close(userns_go[0]);
  const bool need_userns = ::geteuid() != 0;
  bool maps_written = false;
  if(need_userns)
  {
    char ready = 0;
    // Only write the id maps, and only release the child, if it actually got as far as asking.
    if(::read(userns_ready[0], &ready, 1) == 1)
    {
      char procpath[64];
      char map[64];
      ::snprintf(procpath, sizeof(procpath), "/proc/%d/setgroups", (int) pid);
      (void) write_pseudofile(procpath, "deny");
      ::snprintf(map, sizeof(map), "0 %u 1", (unsigned) ::getuid());
      ::snprintf(procpath, sizeof(procpath), "/proc/%d/uid_map", (int) pid);
      maps_written = write_pseudofile(procpath, map);
      if(maps_written)
      {
        ::snprintf(map, sizeof(map), "0 %u 1", (unsigned) ::getgid());
        ::snprintf(procpath, sizeof(procpath), "/proc/%d/gid_map", (int) pid);
        maps_written = write_pseudofile(procpath, map);
      }
      (void) ::write(userns_go[1], "1", 1);
    }
  }
  ::close(userns_ready[0]);
  ::close(userns_go[1]);
  int status = 0;
  BOOST_REQUIRE(::waitpid(pid, &status, 0) == pid);
  BOOST_REQUIRE(WIFEXITED(status));
  const int code = WEXITSTATUS(status);
  if(code == 77 || (need_userns && !maps_written))
  {
    return -1;
  }
  return code;
}

static int fallback_error_class_child(int userns_ready_fd, int userns_go_fd, const char *mountpoint)
{
  enter_private_mount_namespace(userns_ready_fd, userns_go_fd);
  // Enough inodes for the directory, the destination and a handful of fillers, and nothing more.
  if(::mount("tmpfs", mountpoint, "tmpfs", 0, "size=4M,nr_inodes=8") != 0)
  {
    return 77;
  }
  const std::string sub = std::string(mountpoint) + "/sub";
  if(::mkdir(sub.c_str(), 0755) != 0)
  {
    return 1;
  }
  const std::string target = sub + "/target.txt";
  {
    const int fd = ::open(target.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if(fd < 0)
    {
      return 1;
    }
    const bool written = ::write(fd, "original", 8) == 8;
    ::close(fd);
    if(!written)
    {
      return 1;
    }
  }
  // Use up every remaining inode, so that creating a new file must now fail.
  int fillers = 0;
  for(; fillers < 64; fillers++)
  {
    const std::string filler = sub + "/filler" + std::to_string(fillers);
    const int fd = ::open(filler.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if(fd < 0)
    {
      break;
    }
    ::close(fd);
  }
  if(fillers == 0 || fillers == 64)
  {
    return 77;  // inodes not exhausted, or not exhausted within a sane number of files
  }
  {
    // Precondition: creating a *new* name must now fail, and it must fail for the reason this test
    // is about. Any other error would mean the test was measuring something else.
    const int probe = ::open((sub + "/probe").c_str(), O_CREAT | O_EXCL | O_WRONLY, 0644);
    if(probe >= 0)
    {
      ::close(probe);
      (void) ::unlink((sub + "/probe").c_str());
      return 77;  // creating a file still works, so no failure to classify
    }
    if(errno != ENOSPC)
    {
      return 77;  // out of inodes for some reason other than ENOSPC
    }
  }
  {
    auto dirh = llfio::directory_handle::directory({}, sub);
    if(!dirh)
    {
      return 2;
    }
    {
      // Precondition: direct writing to the destination would have worked, since no new inode is
      // needed to open an existing file. Without this, not falling back would be indistinguishable
      // from falling back and failing, and the test would prove nothing.
      auto direct = llfio::file_handle::file(dirh.value(), "target.txt", llfio::file_handle::mode::write);
      if(!direct)
      {
        return 7;
      }
      if(!direct.value().close())
      {
        return 7;
      }
    }
    // The temporary file cannot be created, and the reason is resource exhaustion rather than a
    // refusal of new names by the directory. So even with fallback explicitly enabled this must be
    // an error, and it must be the temporary file's own `ENOSPC` rather than the fallback's.
    auto f = atomic_file::open(dirh.value(), "target.txt", atomic_file::options(true));
    if(!f.has_error())
    {
      return 3;  // fell back, which is exactly what must not happen
    }
    if(f.error() != llfio::errc::no_space_on_device)
    {
      return 4;  // and it must be the failure to create the temporary file which is reported
    }
  }
  {
    // The destination must still hold its original contents. A fallback would have truncated it
    // while opening, before any of this could be observed.
    const int fd = ::open(target.c_str(), O_RDONLY);
    if(fd < 0)
    {
      return 5;
    }
    char contents[16] = {0};
    const ssize_t got = ::read(fd, contents, sizeof(contents) - 1);
    ::close(fd);
    if(got != 8 || ::strncmp(contents, "original", 8) != 0)
    {
      return 6;  // the destination was destroyed or changed
    }
  }
  return 0;
}

static void TestFallbackErrorClass(const llfio::directory_handle &dirh)
{
  const llfio::filesystem::path mountpoint = dirh.current_path().value() / "fallback_errclass";
  std::error_code ec;
  llfio::filesystem::create_directory(mountpoint, ec);
  BOOST_REQUIRE(!ec);
  const int code = run_in_private_mount_namespace(fallback_error_class_child, mountpoint.string());
  if(code < 0)
  {
    BOOST_TEST_MESSAGE(
    "Fallback error class test needs root or usable unprivileged user namespaces. So skipping this test.");
    return;
  }
  if(code != 0)
  {
    BOOST_TEST_MESSAGE("fallback error class test: child process exited "
                       << code << " (3 means it fell back, 6 means it destroyed the destination)");
  }
  BOOST_CHECK(code == 0);
}
#endif

// Move construction and move assignment must transfer ownership of the transaction.
static void TestMove(const llfio::directory_handle &dirh)
{
  {
    atomic_file a = atomic_file::open(dirh, "moved.txt").value();
    BOOST_CHECK(a.write(buf("abc")).value() == 3);
    atomic_file b = std::move(a);
    BOOST_CHECK(b.write(buf("def")).value() == 3);
    BOOST_CHECK(b.commit());
  }
  BOOST_CHECK(read_file(dirh, "moved.txt") == "abcdef");
  {
    atomic_file a = atomic_file::open(dirh, "moved2.txt").value();
    BOOST_CHECK(a.write(buf("ghi")).value() == 3);
    atomic_file b = atomic_file::open(dirh, "moved3.txt").value();
    b = std::move(a);
    BOOST_CHECK(b.commit());
  }
  BOOST_CHECK(read_file(dirh, "moved2.txt") == "ghi");
  // moved3 must not exist, and its temp must have been cleaned up by the move assignment
  BOOST_CHECK(!llfio::file_handle::file(dirh, "moved3.txt"));
  BOOST_CHECK(count_random_files(dirh.current_path().value()) == 0);
}

static inline void TestAtomicFile()
{
  // The scratch directory must be unique to this process. This same test binary exists in several
  // configurations (exceptions/status_code, sl/hl/dl, asan/tsan) and ctest may run them at the
  // same time; with a fixed name two instances would delete each other's files and the test would
  // fail non-deterministically, which is worse than not testing at all.
  const llfio::filesystem::path dirpath = llfio::filesystem::temp_directory_path() / ("llfio_atomic_file_test-" + llfio::utils::random_string(16));
  std::error_code ec;
  llfio::filesystem::remove_all(dirpath, ec);
  BOOST_REQUIRE(llfio::filesystem::create_directory(dirpath, ec));
  BOOST_REQUIRE(!ec);
  {
    auto dirh = llfio::directory_handle::directory({}, dirpath).value();

    TestCreateNewFile(dirh);
    TestOverwriteExisting(dirh);
#ifndef _WIN32
    TestPreservePermissions(dirh);
#endif
    TestMultipleWrites(dirh);
    TestEmptyWrite(dirh);
    TestDiscardOnDestruction(dirh);
    TestCancel(dirh);
    TestDoubleCommit(dirh);
    TestPathWithDirectory(dirh);
    TestOpenIntoNonexistentDirectory(dirh);
    TestDestinationIsDirectory(dirh);
    TestCommitBarrierFails(dirh);
    TestDirectoryBarrierFails(dirh);
    TestReaderNeverSeesPartial(dirh);
    TestFallback(dirh);
    TestFallbackPermittedButUnused(dirh);
    TestFallbackSetupFailure(dirh);
    TestFallbackErrorClass(dirh);
    TestMove(dirh);
  }
  llfio::filesystem::remove_all(dirpath, ec);
}

KERNELTEST_TEST_KERNEL(integration, llfio, atomic_file, atomic_file, "Tests that llfio::atomic_file works as expected",
                       TestAtomicFile())
