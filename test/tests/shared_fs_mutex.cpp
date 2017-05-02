/* Integration testing for algorithm::shared_fs_mutex
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (13 commits)
File Created: Aug 2016


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

#define BOOSTLITE_BOOST_UNIT_TEST_CUSTOM_MAIN_DEFINED

#include "../../include/boost/afio/afio.hpp"
#include "../kerneltest/include/boost/kerneltest.hpp"

BOOST_KERNELTEST_TEST_KERNEL(unit, afio, shared_fs_mutex, entity_endian, "Tests that afio::algorithm::shared_fs_mutex::entity_type has the right endian", [] {
  BOOST_AFIO_V2_NAMESPACE::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type v(0, true);
  BOOST_REQUIRE(v._init != 1UL);
}())

#include <codecvt>
#include <unordered_map>

BOOST_KERNELTEST_V1_NAMESPACE_BEGIN
struct waitable_done
{
  stl11::atomic<int> done;

  stl11::mutex _m;
  stl11::condition_variable _cv;
  waitable_done(int v = 0)
      : done(v)
  {
  }
  template <class Rep, class Period> bool wait_until_done_for(const stl11::chrono::duration<Rep, Period> &rel)
  {
    stl11::unique_lock<std::mutex> h(_m);
    return _cv.wait_for(h, rel, [&] { return done != 0; });
  }
  template <class Clock, class Duration> bool wait_until_done_until(const stl11::chrono::time_point<Clock, Duration> &dl)
  {
    stl11::unique_lock<std::mutex> h(_m);
    return _cv.wait_until(h, dl, [&] { return done != 0; });
  }
  void wait_until_done()
  {
    stl11::unique_lock<std::mutex> h(_m);
    _cv.wait(h, [&] { return done != 0; });
  }
  void set_done(int v)
  {
    done = v;
    _cv.notify_all();
  }
};

static std::unordered_map<std::string, std::function<std::string(waitable_done &, size_t, const char *)>> kerneltest_child_worker_registry;

struct kerneltest_child_worker_registration
{
  std::string name;
  template <class U>
  kerneltest_child_worker_registration(std::string _name, U &&f)
      : name(std::move(_name))
  {
    kerneltest_child_worker_registry.insert(std::make_pair(name, std::move(f)));
  }
  kerneltest_child_worker_registration(kerneltest_child_worker_registration &&o) noexcept : name(std::move(o.name)) {}
  ~kerneltest_child_worker_registration()
  {
    if(!name.empty())
      kerneltest_child_worker_registry.erase(name);
  }
};
template <class U> kerneltest_child_worker_registration register_child_worker(std::string name, U &&f)
{
  return kerneltest_child_worker_registration(std::move(name), std::move(f));
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)  // conditional expression is constant
#endif
namespace detail
{
  void print_args(std::ostream &) {}
  template <class Arg, class... Args> void print_args(std::ostream &s, Arg &&arg, Args &&... args)
  {
    if(std::is_constructible<std::string, Arg>::value)
      s << "\"";
    s << arg;
    if(std::is_constructible<std::string, Arg>::value)
      s << "\"";
    s << ",";
    print_args(s, std::forward<Args>(args)...);
  }
  template <class Arg> void print_args(std::ostream &s, Arg &&arg) { s << arg; }
  void print_args(std::wostream &) {}
  template <class Arg, class... Args> void print_args(std::wostream &s, Arg &&arg, Args &&... args)
  {
    if(std::is_constructible<std::wstring, Arg>::value || std::is_constructible<std::string, Arg>::value)
      s << "\"";
    s << arg;
    if(std::is_constructible<std::wstring, Arg>::value || std::is_constructible<std::string, Arg>::value)
      s << "\"";
    s << ",";
    print_args(s, std::forward<Args>(args)...);
  }
  template <class Arg> void print_args(std::wostream &s, Arg &&arg) { s << arg; }
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct child_workers
{
  std::vector<child_process::child_process> workers;
  struct result
  {
    std::string cerr;
    intptr_t retcode;
    std::string results;
  };
  std::vector<result> results;

  template <class... Args> child_workers(std::string name, size_t workersno, Args &&... _args)
  {
#ifdef _UNICODE
    std::wstringstream ss1, ss2, ss3;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf16conv;
    auto convstr = [&](const std::string &v) { return utf16conv.from_bytes(v); };
#else
    std::stringstream ss1, ss2, ss3;
    auto convstr = [](const std::string &v) { return v; };
#endif
    ss1 << "--kerneltestchild," << convstr(name) << ",";
    ss3 << ",";
    detail::print_args(ss3, std::forward<Args>(_args)...);
    results.resize(workersno);
    for(size_t n = 0; n < workersno; n++)
    {
      ss2 = decltype(ss2)();
      ss2 << ss1.str() << n << ss3.str();
      std::vector<stl1z::filesystem::path::string_type> args{ss2.str()};
      auto newchild = child_process::child_process::launch(child_process::current_process_path(), std::move(args), child_process::current_process_env(), true);
#if 0
      if(!newchild)
      {
        fprintf(stderr, "Failed to launch new child process due to %s. Press Return to fatal exit.\n", newchild.error().message().c_str());
        getchar();
        abort();
      }
#endif
      workers.push_back(std::move(newchild).get());
      results[n].retcode = 0;
    }
  }
  void wait_until_ready()
  {
    char buffer[1024];
    for(size_t n = 0; n < workers.size(); n++)
    {
      auto &i = workers[n].cout();
      if(!i.getline(buffer, sizeof(buffer)))
      {
        results[n].cerr = "ERROR: Child seems to have vanished!";
        results[n].retcode = 99;
        continue;
      }
      if(0 != strncmp(buffer, "READY", 5))
      {
        results[n].cerr = "ERROR: Child wrote unexpected output '" + std::string(buffer) + "'";
        results[n].retcode = 98;
        continue;
      }
    }
  }
  void go()
  {
    for(size_t n = 0; n < workers.size(); n++)
    {
      if(!results[n].retcode)
        workers[n].cin() << "GO" << std::endl;
    }
  }
  void stop()
  {
    for(size_t n = 0; n < workers.size(); n++)
    {
      if(!results[n].retcode)
        workers[n].cin() << "STOP" << std::endl;
    }
  }
  void join()
  {
    char buffer[1024];
    for(size_t n = 0; n < workers.size(); n++)
    {
      auto &child = workers[n];
      if(!results[n].retcode)
      {
        if(!child.cout().getline(buffer, sizeof(buffer)))
        {
          results[n].cerr = "ERROR: Child seems to have vanished!";
          results[n].retcode = 99;
          continue;
        }
        if(0 != strncmp(buffer, "RESULTS ", 8))
        {
          results[n].cerr = "ERROR: Child wrote unexpected output '" + std::string(buffer) + "'";
          results[n].retcode = 98;
          continue;
        }
        results[n].results = buffer + 8;
        if(results[n].results.back() == '\r')
          results[n].results.resize(results[n].results.size() - 1);
      }
#ifdef NDEBUG 
      stl11::chrono::steady_clock::time_point deadline = stl11::chrono::steady_clock::now() + stl11::chrono::seconds(5);
#else
      stl11::chrono::steady_clock::time_point deadline;
#endif
      results[n].retcode = child.wait_until(deadline).get();
    }
  }
};
template <class... Args> inline child_workers launch_child_workers(std::string name, size_t workers, Args &&... args)
{
  return child_workers(std::move(name), workers, std::forward<Args>(args)...);
}


BOOST_KERNELTEST_V1_NAMESPACE_END

/*














*/

struct shared_memory
{
  BOOST_AFIO_V2_NAMESPACE::stl11::atomic<long> current_exclusive;
  char _cacheline1[64];
  enum mutex_kind_type
  {
    atomic_append,
    byte_ranges,
    lock_files,
    memory_map
  } mutex_kind;
  enum test_type
  {
    exclusive = 0,
    shared = 1,
    both = 2
  } testtype;
  char _cacheline2[64];
  BOOST_AFIO_V2_NAMESPACE::stl11::atomic<long> current_shared;
};

static inline void check_child_worker(const BOOST_KERNELTEST_V1_NAMESPACE::child_workers::result &i)
{
  if(i.cerr.size())
  {
    BOOST_CHECK(i.cerr.empty());
    std::cout << "Child worker failed with '" << i.cerr << "'" << std::endl;
  }
  else if(i.retcode != 0)
  {
    BOOST_CHECK(i.retcode == 0);
    std::cout << "Child worker process failed with return code " << i.retcode << std::endl;
  }
  else
  {
    BOOST_CHECK(i.retcode == 0);
    BOOST_CHECK((i.results[0] == 'o' && i.results[1] == 'k'));
    std::cout << "Child reports " << i.results << std::endl;
  }
};

static auto TestSharedFSMutexCorrectnessChildWorker = BOOST_KERNELTEST_V1_NAMESPACE::register_child_worker("TestSharedFSMutexCorrectness", [](BOOST_KERNELTEST_V1_NAMESPACE::waitable_done &waitable, size_t childidx, const char *) -> std::string {
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  auto shared_mem_file = afio::file_handle::file("shared_memory", afio::file_handle::mode::write, afio::file_handle::creation::open_existing, afio::file_handle::caching::temporary).get();
  auto shared_mem_file_section = afio::section_handle::section(sizeof(shared_memory), shared_mem_file, afio::section_handle::flag::readwrite).get();
  auto shared_mem_file_map = afio::map_handle::map(shared_mem_file_section).get();
  shared_memory *shmem = (shared_memory *) shared_mem_file_map.address();
  ++shmem->current_exclusive;
  while(-1 != shmem->current_exclusive)
    afio::stl11::this_thread::yield();
  std::unique_ptr<afio::algorithm::shared_fs_mutex::shared_fs_mutex> lock, fblock;
  switch(shmem->mutex_kind)
  {
  case shared_memory::mutex_kind_type::atomic_append:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::atomic_append>(afio::algorithm::shared_fs_mutex::atomic_append::fs_mutex_append("lockfile").get());
    break;
  case shared_memory::mutex_kind_type::byte_ranges:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::byte_ranges>(afio::algorithm::shared_fs_mutex::byte_ranges::fs_mutex_byte_ranges("lockfile").get());
    break;
  case shared_memory::mutex_kind_type::lock_files:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::lock_files>(afio::algorithm::shared_fs_mutex::lock_files::fs_mutex_lock_files(".").get());
    break;
  case shared_memory::mutex_kind_type::memory_map:
    fblock = std::make_unique<afio::algorithm::shared_fs_mutex::byte_ranges>(afio::algorithm::shared_fs_mutex::byte_ranges::fs_mutex_byte_ranges("fallbacklockfile").get());
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::memory_map<>>(afio::algorithm::shared_fs_mutex::memory_map<>::fs_mutex_map("lockfile", fblock.get()).get());
    break;
  }
  ++shmem->current_shared;
  while(0 != shmem->current_shared)
    afio::stl11::this_thread::yield();
  long maxreaders = 0;
  do
  {
    if(shmem->testtype == shared_memory::test_type::exclusive || (shmem->testtype == shared_memory::test_type::both && childidx < 2))
    {
      auto h = lock->lock(afio::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type(0, true)).get();
      long oldval = shmem->current_exclusive.exchange((long) childidx);
      if(oldval != -1)
        return "Child " + std::to_string(childidx) + " granted exclusive lock when child " + std::to_string(oldval) + " already has exclusive lock!";
      oldval = shmem->current_shared;
      if(oldval != 0)
        return "Child " + std::to_string(childidx) + " holding exclusive lock yet " + std::to_string(oldval) + " users hold the shared lock!";
      oldval = shmem->current_exclusive.exchange(-1);
      if(oldval != (long) childidx)
        return "Child " + std::to_string(childidx) + " released exclusive lock to find child " + std::to_string(oldval) + " had stolen my lock!";
    }
    else if(shmem->testtype == shared_memory::test_type::shared || shmem->testtype == shared_memory::test_type::both)
    {
      auto h = lock->lock(afio::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type(0, false)).get();
      long oldval = ++shmem->current_shared;
      if(oldval > maxreaders)
        maxreaders = oldval;
      oldval = shmem->current_exclusive;
      if(oldval != -1)
        return "Child " + std::to_string(childidx) + " granted shared lock when child " + std::to_string(oldval) + " already has exclusive lock!";
      --shmem->current_shared;
    }
  } while(!waitable.done);
  return "ok, max concurrent readers was " + std::to_string(maxreaders);
});


/*
Test 1: X child processes use the lock for exclusive and shared usage. Verify we never allow exclusive with anything
else. Verify shared allows other shared.
*/

void TestSharedFSMutexCorrectness(shared_memory::mutex_kind_type mutex_kind, shared_memory::test_type testtype)
{
  namespace kerneltest = BOOST_KERNELTEST_V1_NAMESPACE;
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  auto shared_mem_file = afio::file_handle::file("shared_memory", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).get();
  shared_mem_file.truncate(sizeof(shared_memory));
  auto shared_mem_file_section = afio::section_handle::section(sizeof(shared_memory), shared_mem_file, afio::section_handle::flag::readwrite).get();
  auto shared_mem_file_map = afio::map_handle::map(shared_mem_file_section).get();
  shared_memory *shmem = (shared_memory *) shared_mem_file_map.address();
  shmem->current_shared = -(long) afio::stl11::thread::hardware_concurrency();
  shmem->current_exclusive = -(long) afio::stl11::thread::hardware_concurrency() - 1;
  shmem->mutex_kind = mutex_kind;
  shmem->testtype = testtype;

  auto child_workers = kerneltest::launch_child_workers("TestSharedFSMutexCorrectness", afio::stl11::thread::hardware_concurrency());
  child_workers.wait_until_ready();
// I now have hardware_concurrency child worker processes ready to go, so execute the child worker
#if 0
  std::cout << "Please attach debuggers and press Enter" << std::endl;
  getchar();
#endif
  child_workers.go();
  // They will all open the shared memory and gate on current_exclusive until all reach the exact same point
  // They will then all create the lock concurrently and gate on current_shared until all reach the exact same point
  // They then will iterate locking and unlocking the same entity using the shared memory to verify it never happens
  // that more than one ever holds an exclusive lock, or any shared lock occurs when an exclusive lock is held
  afio::stl11::this_thread::sleep_for(afio::stl11::chrono::seconds(5));
  child_workers.stop();
  child_workers.join();
  for(auto &i : child_workers.results)
    check_child_worker(i);
}

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_byte_ranges, exclusives, "Tests that afio::algorithm::shared_fs_mutex::byte_ranges implementation implements exclusive locking", [] { TestSharedFSMutexCorrectness(shared_memory::byte_ranges, shared_memory::exclusive); }())
BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_byte_ranges, shared, "Tests that afio::algorithm::shared_fs_mutex::byte_ranges implementation implements shared locking", [] { TestSharedFSMutexCorrectness(shared_memory::byte_ranges, shared_memory::shared); }())
BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_byte_ranges, both, "Tests that afio::algorithm::shared_fs_mutex::byte_ranges implementation implements a mixture of exclusive and shared locking", [] { TestSharedFSMutexCorrectness(shared_memory::byte_ranges, shared_memory::both); }())

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_memory_map, exclusives, "Tests that afio::algorithm::shared_fs_mutex::memory_map implementation implements exclusive locking", [] { TestSharedFSMutexCorrectness(shared_memory::memory_map, shared_memory::exclusive); }())
BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_memory_map, shared, "Tests that afio::algorithm::shared_fs_mutex::memory_map implementation implements shared locking", [] { TestSharedFSMutexCorrectness(shared_memory::memory_map, shared_memory::shared); }())
BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_memory_map, both, "Tests that afio::algorithm::shared_fs_mutex::memory_map implementation implements a mixture of exclusive and shared locking", [] { TestSharedFSMutexCorrectness(shared_memory::memory_map, shared_memory::both); }())

/*

Test 2: X child processes all try to construct the lock at once, lock something shared, unlock it and destruct. A
counter determines if everybody locked the same thing. Important there is no blocking.

*/

// TODO


/*

Test 3: We have a constant ongoing stream of child processes constructing the lock, locking something, unlocking,
and destructing the lock. This should find interesting races in the more complex lock constructors and destructors

*/
static void TestSharedFSMutexConstructDestruct(shared_memory::mutex_kind_type mutex_kind)
{
  namespace kerneltest = BOOST_KERNELTEST_V1_NAMESPACE;
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  auto shared_mem_file = afio::file_handle::file("shared_memory", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).get();
  auto shared_mem_file_section = afio::section_handle::section(sizeof(shared_memory), shared_mem_file, afio::section_handle::flag::readwrite).get();
  auto shared_mem_file_map = afio::map_handle::map(shared_mem_file_section).get();
  shared_memory *shmem = (shared_memory *) shared_mem_file_map.address();
  shmem->current_shared = -2;
  shmem->current_exclusive = -3;
  shmem->mutex_kind = mutex_kind;
  shmem->testtype = shared_memory::test_type::exclusive;

  // Launch two child workers who constantly lock and unlock, checking correctness
  auto child_workers1 = kerneltest::launch_child_workers("TestSharedFSMutexCorrectness", 2);
  child_workers1.wait_until_ready();
  child_workers1.go();

  // Now repeatedly launch hardware concurrency child workers who constantly open the lock, lock once, unlock and close the lock
  auto begin = afio::stl11::chrono::steady_clock::now();
  while(afio::stl11::chrono::duration_cast<afio::stl11::chrono::seconds>(afio::stl11::chrono::steady_clock::now() - begin).count() < 5)
  {
    auto child_workers = kerneltest::launch_child_workers("TestSharedFSMutexConstructDestruct", afio::stl11::thread::hardware_concurrency(), (size_t) mutex_kind);
    child_workers.wait_until_ready();
    child_workers.go();
    child_workers.stop();
    child_workers.join();
    for(auto &i : child_workers.results)
      check_child_worker(i);
  }
  child_workers1.stop();
  child_workers1.join();
  for(auto &i : child_workers1.results)
    check_child_worker(i);
}

static auto TestSharedFSMutexConstructDestructChildWorker = BOOST_KERNELTEST_V1_NAMESPACE::register_child_worker("TestSharedFSMutexConstructDestruct", [](BOOST_KERNELTEST_V1_NAMESPACE::waitable_done &, size_t, const char *params) -> std::string {
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  std::unique_ptr<afio::algorithm::shared_fs_mutex::shared_fs_mutex> lock;
  shared_memory::mutex_kind_type mutex_kind = (shared_memory::mutex_kind_type) atoi(params);
  switch(mutex_kind)
  {
  case shared_memory::mutex_kind_type::atomic_append:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::atomic_append>(afio::algorithm::shared_fs_mutex::atomic_append::fs_mutex_append("lockfile").get());
    break;
  case shared_memory::mutex_kind_type::byte_ranges:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::byte_ranges>(afio::algorithm::shared_fs_mutex::byte_ranges::fs_mutex_byte_ranges("lockfile").get());
    break;
  case shared_memory::mutex_kind_type::lock_files:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::lock_files>(afio::algorithm::shared_fs_mutex::lock_files::fs_mutex_lock_files(".").get());
    break;
  case shared_memory::mutex_kind_type::memory_map:
    lock = std::make_unique<afio::algorithm::shared_fs_mutex::memory_map<>>(afio::algorithm::shared_fs_mutex::memory_map<>::fs_mutex_map("lockfile").get());
    break;
  }
  // Take a shared lock of a different entity
  auto h = lock->lock(afio::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type(1, false)).get();
  return "ok";
});

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_memory_map, construct_destruct, "Tests that afio::algorithm::shared_fs_mutex::memory_map constructor and destructor are race free", [] { TestSharedFSMutexConstructDestruct(shared_memory::memory_map); }())

/*

Test 4: Does memory_map safely fall back onto its backup lock if a networked drive user turns up?

*/

static void TestMemoryMapFallback()
{
  // Launch hardware concurrency users of the map
  namespace kerneltest = BOOST_KERNELTEST_V1_NAMESPACE;
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  auto shared_mem_file = afio::file_handle::file("shared_memory", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).get();
  auto shared_mem_file_section = afio::section_handle::section(sizeof(shared_memory), shared_mem_file, afio::section_handle::flag::readwrite).get();
  auto shared_mem_file_map = afio::map_handle::map(shared_mem_file_section).get();
  shared_memory *shmem = (shared_memory *) shared_mem_file_map.address();
  auto begin = afio::stl11::chrono::steady_clock::now();
  while(afio::stl11::chrono::duration_cast<afio::stl11::chrono::seconds>(afio::stl11::chrono::steady_clock::now() - begin).count() < 20)
  {
    shmem->current_shared = -(long) afio::stl11::thread::hardware_concurrency();
    shmem->current_exclusive = -(long) afio::stl11::thread::hardware_concurrency() - 1;
    shmem->mutex_kind = shared_memory::mutex_kind_type::memory_map;
    shmem->testtype = shared_memory::test_type::both;

    auto child_workers = kerneltest::launch_child_workers("TestSharedFSMutexCorrectness", afio::stl11::thread::hardware_concurrency());
    child_workers.wait_until_ready();
    child_workers.go();
    // Wait until children get to lock
    {
      auto lk = afio::algorithm::shared_fs_mutex::memory_map<>::fs_mutex_map("lockfile").get();
      bool done;
      do
      {
        auto _ = lk.try_lock(afio::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type(0, false));
        done = _.has_error();
      } while(!done);
    }
    // Zomp the lock file pretending to be a networked user
    {
      auto lf = afio::file_handle::file("lockfile", afio::file_handle::mode::write, afio::file_handle::creation::open_existing, afio::file_handle::caching::reads).get();
      char buffer[4096];
      memset(buffer, 0, sizeof(buffer));
      lf.write(0, buffer, sizeof(buffer));
    }
    // Open the lock, should always return EBUSY but only AFTER all mapped users have fallen back
    auto fblk = afio::algorithm::shared_fs_mutex::byte_ranges::fs_mutex_byte_ranges("fallbacklockfile").get();
    auto lk = afio::algorithm::shared_fs_mutex::memory_map<>::fs_mutex_map("lockfile", &fblk);
    BOOST_CHECK(lk.has_error());
    if(lk.has_error())
    {
      BOOST_CHECK(lk.get_error().value() == EBUSY);
    }
    {
      auto fblkh = fblk.lock(afio::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type(0, false)).get();
      long oldval = ++shmem->current_shared;
      oldval = shmem->current_exclusive;
      if(oldval != -1)
      {
        std::cerr << "Fallback lock granted shared lock when child " + std::to_string(oldval) + " already has exclusive lock!" << std::endl;
        BOOST_CHECK(oldval == -1);
      }
      --shmem->current_shared;
    }
    child_workers.stop();
    child_workers.join();
    for(auto &i : child_workers.results)
      check_child_worker(i);
  }
}
BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_memory_map, fallback, "Tests that afio::algorithm::shared_fs_mutex::memory_map falls back onto its backup lock when a networked user arrives", TestMemoryMapFallback())

/*













*/

int main(int argc, char *argv[])
{
  using namespace BOOST_KERNELTEST_V1_NAMESPACE;
  for(int n = 1; n < argc; n++)
  {
    if(!strncmp(argv[n], "--kerneltestchild,", 18))
    {
      // Format is --kerneltestchild,name,childidx,pars...
      char *comma = strchr(argv[n] + 18, ',');
      if(!comma)
        return 1;
      std::string name(argv[n] + 18, comma - (argv[n] + 18));
      size_t thischild = strtoul(comma + 1, nullptr, 10);
      comma = strchr(comma + 1, ',');
      if(!comma)
        return 1;
      const char *param = comma + 1;
      auto it = kerneltest_child_worker_registry.find(name);
      if(it == kerneltest_child_worker_registry.end())
      {
        std::cerr << "KernelTest child worker " << thischild << " fails to find work '" << name << "', exiting" << std::endl;
        return 1;
      }
      try
      {
        // Fire up a worker thread and get him to block
        waitable_done waitable(-1);
        std::string result;
        std::thread worker([&] {
          while(waitable.done == -1)
            std::this_thread::yield();
          result = it->second(waitable, thischild, param);
        });
        std::cout << "READY(" << thischild << ")" << std::endl;
        for(;;)
        {
          char buffer[1024];
          // This blocks
          if(!std::cin.getline(buffer, sizeof(buffer)))
          {
            waitable.set_done(1);
            worker.join();
            return 1;
          }
          if(0 == strcmp(buffer, "GO"))
          {
            // Launch worker thread
            waitable.set_done(0);
          }
          else if(0 == strcmp(buffer, "STOP"))
          {
            waitable.set_done(1);
            worker.join();
            std::cout << "RESULTS " << result << std::endl;
            return 0;
          }
        }
      }
      catch(const std::exception &e)
      {
        std::cerr << "KernelTest child worker " << thischild << " throws exception '" << e.what() << "'" << std::endl;
        return 1;
      }
      catch(...)
      {
        std::cerr << "KernelTest child worker " << thischild << " throws exception 'unknown'" << std::endl;
        return 1;
      }
    }
  }
  BOOST_AFIO_V2_NAMESPACE::stl1z::filesystem::create_directory("shared_fs_mutex_testdir");
  BOOST_AFIO_V2_NAMESPACE::stl1z::filesystem::current_path("shared_fs_mutex_testdir");
  int result = BOOSTLITE_BOOST_UNIT_TEST_RUN_TESTS(argc, argv);
  return result;
}
