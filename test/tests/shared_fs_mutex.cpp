/* Integration testing for algorithm::shared_fs_mutex
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Aug 2016
*/

#define BOOST_CATCH_CUSTOM_MAIN_DEFINED

#include "../../include/boost/afio/afio.hpp"
#include "../kerneltest/include/boost/kerneltest.hpp"

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
      workers.push_back(child_process::child_process::launch(child_process::current_process_path(), std::move(args), child_process::current_process_env()).get());
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
      results[n].retcode = child.wait().get();
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

static auto TestSharedFSMutexCorrectnessChildWorker = BOOST_KERNELTEST_V1_NAMESPACE::register_child_worker("TestSharedFSMutexCorrectness", [](BOOST_KERNELTEST_V1_NAMESPACE::waitable_done &waitable, size_t childidx, const char *pars) -> std::string {
  waitable.wait_until_done();
  (void) childidx;
  (void) pars;
  return "ok";
});

// TestType = 0 for exclusive only, 1 for shared only, 2 for shared + two exclusives
void TestSharedFSMutexCorrectness(const char *fsmutex, size_t testtype)
{
  namespace kerneltest = BOOST_KERNELTEST_V1_NAMESPACE;
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  auto child_workers = kerneltest::launch_child_workers("TestSharedFSMutexCorrectness", afio::stl11::thread::hardware_concurrency(), fsmutex, testtype);
  child_workers.wait_until_ready();
  // I now have hardware_concurrency child worker processes spinning waiting to go
  child_workers.go();
  afio::stl11::this_thread::sleep_for(afio::stl11::chrono::seconds(5));
  child_workers.stop();
  child_workers.join();
  for(auto &i : child_workers.results)
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
      BOOST_CHECK(i.results == "ok");
    }
  }
}

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, shared_fs_mutex_memory_map, churn, "Tests that afio::algorithm::shared_fs_mutex::memory_map implementation handles user churn", [] {
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  afio::stl1z::filesystem::create_directory("shared_fs_mutex_testdir");
  // auto lock = afio::algorithm::shared_fs_mutex::lock_files::fs_mutex_lock_files("shared_fs_mutex_testdir");
  TestSharedFSMutexCorrectness("memory_map", 0);

  /*
  Test 1: X child processes use the lock for exclusive and shared usage. Verify we never allow exclusive with anything
  else. Verify shared allows other shared.

  Test 2: X child processes all try to construct the lock at once, lock something shared, unlock it and destruct. A
  counter determines if everybody locked the same thing. Important there is no blocking.

  Test 3: We have a constant ongoing stream of child processes constructing the lock, locking something, unlocking,
  and destructing the lock. This should find interesting races in the more complex lock constructors and destructors

  */
}())

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
  int result = Catch::Session().run(argc, argv);
  return result;
}
