/* Test the performance of various file locking mechanisms
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Mar 2016


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

//! On exit dumps a CSV file of the AFIO log, one per child worker
#define DEBUG_CSV 1

//! Seconds to run the benchmark
#define BENCHMARK_DURATION 10

#define _CRT_SECURE_NO_WARNINGS 1

#include "../../include/afio/afio.hpp"
#include "kerneltest/include/kerneltest/v1.0/child_process.hpp"

#include <fstream>
#include <iostream>
#include <vector>

#ifdef _WIN32
#undef _CRT_NONSTDC_DEPRECATE
#define _CRT_NONSTDC_DEPRECATE(a)
#include <conio.h>  // for kbhit()
#else
#include <sys/ioctl.h>
#include <termios.h>

bool kbhit()
{
  termios term;
  tcgetattr(0, &term);

  termios term2 = term;
  term2.c_lflag &= ~ICANON;
  tcsetattr(0, TCSANOW, &term2);

  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);

  tcsetattr(0, TCSANOW, &term);

  return byteswaiting > 0;
}
#endif

namespace afio = LLFIO_V2_NAMESPACE;
namespace child_process = KERNELTEST_V1_NAMESPACE::child_process;

static volatile size_t *shared_memory;
static void initialise_shared_memory()
{
  auto fh = afio::file_handle::file({}, "shared_memory", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary).value();
  auto sh = afio::section_handle::section(fh, 8, afio::section_handle::flag::write).value();
  auto mp = afio::map_handle::map(sh).value();
  shared_memory = (size_t *) mp.address();
  if(!shared_memory)
    abort();
  *shared_memory = (size_t) -1;
}
static void child_locks(size_t id)
{
  size_t current = *shared_memory;
  if(current != (size_t) -1)
  {
    std::cerr << "FATAL: Lock algorithm is broken! " << current << " still holds the lock!" << std::endl;
    std::terminate();
  }
  *shared_memory = id;
}
static void child_unlocks(size_t id)
{
  size_t current = *shared_memory;
  if(current != id)
  {
    std::cerr << "FATAL: Lock algorithm is broken! " << current << " has stolen the lock!" << std::endl;
    std::terminate();
  }
  *shared_memory = (size_t) -1;
}

int main(int argc, char *argv[])
{
  if(argc < 4)
  {
    std::cerr << "Usage: " << argv[0] << " [!]<atomic_append|byte_ranges|lock_files|memory_map> <entities> <no of waiters>" << std::endl;
    return 1;
  }
  initialise_shared_memory();


  // ******** MASTER PROCESS BEGINS HERE ********
  if(strcmp(argv[1], "spawned") && strcmp(argv[1], "!spawned"))
  {
    size_t waiters = atoi(argv[3]);
    if(!waiters || !atoi(argv[2]))
    {
      std::cerr << "Usage: " << argv[0] << " [!]<atomic_append|byte_ranges|lock_files|memory_map> <entities> <no of waiters>" << std::endl;
      return 1;
    }

    std::vector<child_process::child_process> children;
    auto mypath = child_process::current_process_path();
#ifdef UNICODE
    std::vector<afio::filesystem::path::string_type> args = {L"spawned", L"", L"", L"", L"00"};
    args[1].resize(strlen(argv[1]));
    for(size_t n = 0; n < args[1].size(); n++)
      args[1][n] = argv[1][n];
    args[2].resize(strlen(argv[2]));
    for(size_t n = 0; n < args[2].size(); n++)
      args[2][n] = argv[2][n];
    args[3].resize(strlen(argv[3]));
    for(size_t n = 0; n < args[3].size(); n++)
      args[3][n] = argv[3][n];
#else
    std::vector<afio::filesystem::path::string_type> args = {"spawned", argv[1], argv[2], argv[3], "00"};
#endif
    auto env = child_process::current_process_env();
    std::cout << "Launching " << waiters << " copies of myself as a child process ..." << std::endl;
    for(size_t n = 0; n < waiters; n++)
    {
      if(n >= 10)
      {
        args[4][0] = (char) ('0' + (n / 10));
        args[4][1] = (char) ('0' + (n % 10));
      }
      else
      {
        args[4][0] = (char) ('0' + n);
        args[4][1] = 0;
      }
      auto child = child_process::child_process::launch(mypath, args, env, true);
      if(child.has_error())
      {
        std::cerr << "FATAL: Child " << n << " could not be launched due to " << child.error().message() << std::endl;
        return 1;
      }
      children.push_back(std::move(child.value()));
    }
    // Wait for all children to tell me they are ready
    char buffer[1024];
    std::cout << "Waiting for all children to become ready ..." << std::endl;
    for(auto &child : children)
    {
      auto &i = child.cout();
      if(!i.getline(buffer, sizeof(buffer)))
      {
        std::cerr << "ERROR: Child seems to have vanished!" << std::endl;
        return 1;
      }
      if(0 != strncmp(buffer, "READY", 5))
      {
        std::cerr << "ERROR: Child wrote unexpected output '" << buffer << "'" << std::endl;
        return 1;
      }
    }
#if 0
    std::cout << "Attach your debugger now and press Return" << std::endl;
    getchar();
#endif
#if 0
    auto begin = std::chrono::steady_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() < 2)
      ;
#endif
    std::cout << "Benchmarking for " << BENCHMARK_DURATION << " seconds ..." << std::endl;
    // Issue go command to all children
    for(auto &child : children)
      child.cin() << "GO" << std::endl;
    // Wait for benchmark to complete
    std::this_thread::sleep_for(std::chrono::seconds(BENCHMARK_DURATION));
    std::cout << "Stopping benchmark and telling children to report results ..." << std::endl;
    // Tell children to quit
    for(auto &child : children)
      child.cin() << "STOP" << std::endl;
    unsigned long long results = 0, result;
    std::cout << std::endl;
    std::ofstream oh("benchmark_locking.csv");
    for(size_t n = 0; n < children.size(); n++)
    {
      auto &child = children[n];
      if(!child.cout().getline(buffer, sizeof(buffer)))
      {
        std::cerr << "ERROR: Child seems to have vanished!" << std::endl;
        return 1;
      }
      if(0 != strncmp(buffer, "RESULTS(", 8))
      {
        std::cerr << "ERROR: Child wrote unexpected output '" << buffer << "'." << std::endl;
        return 1;
      }
      result = atol(&buffer[8]);
      std::cout << "Child " << n << " reports result " << result << std::endl;
      results += result;
      if(n)
        oh << ",";
      oh << result;
    }
    results /= BENCHMARK_DURATION;
    std::cout << "Total result: " << results << " ops/sec" << std::endl;
    oh << "\n" << results << std::endl;
    return 0;
  }


  // ******** CHILD PROCESS BEGINS HERE ********
  if(argc < 6)
  {
    std::cerr << "ERROR: args too short" << std::endl;
    return 1;
  }
  enum class lock_algorithm
  {
    unknown,
    atomic_append,
    byte_ranges,
    lock_files,
    memory_map
  } test = lock_algorithm::unknown;
  bool contended = true;
  if(!strcmp(argv[2], "atomic_append"))
    test = lock_algorithm::atomic_append;
  else if(!strcmp(argv[2], "byte_ranges"))
    test = lock_algorithm::byte_ranges;
  else if(!strcmp(argv[2], "lock_files"))
    test = lock_algorithm::lock_files;
  else if(!strcmp(argv[2], "memory_map"))
    test = lock_algorithm::memory_map;
  else if(!strcmp(argv[2], "!atomic_append"))
  {
    test = lock_algorithm::atomic_append;
    contended = false;
  }
  else if(!strcmp(argv[2], "!byte_ranges"))
  {
    test = lock_algorithm::byte_ranges;
    contended = false;
  }
  else if(!strcmp(argv[2], "!lock_files"))
  {
    test = lock_algorithm::lock_files;
    contended = false;
  }
  else if(!strcmp(argv[2], "!memory_map"))
  {
    test = lock_algorithm::memory_map;
    contended = false;
  }
  if(test == lock_algorithm::unknown)
  {
    std::cerr << "ERROR: unknown test requested" << std::endl;
    return 1;
  }
  size_t total_locks = atoi(argv[3]), waiters = atoi(argv[4]), this_child = atoi(argv[5]), count = 0;
  (void) waiters;
  if(!total_locks)
  {
    std::cerr << "ERROR: unknown total locks requested" << std::endl;
    return 1;
  }
  // I am a spawned child. Tell parent I am ready.
  std::cout << "READY(" << this_child << ")" << std::endl;
  // Wait for parent to let me proceed
  std::atomic<int> done(-1);
  std::thread worker([test, contended, total_locks, this_child, &done, &count] {
    std::unique_ptr<afio::algorithm::shared_fs_mutex::shared_fs_mutex> algorithm;
    auto base = afio::path_handle::path(".").value();
    switch(test)
    {
    case lock_algorithm::atomic_append:
    {
      auto v = afio::algorithm::shared_fs_mutex::atomic_append::fs_mutex_append({}, "lockfile");
      if(v.has_error())
      {
        std::cerr << "ERROR: Creation of lock algorithm returns " << v.error().message() << std::endl;
        return;
      }
      algorithm = std::make_unique<afio::algorithm::shared_fs_mutex::atomic_append>(std::move(v.value()));
      break;
    }
    case lock_algorithm::byte_ranges:
    {
      auto v = afio::algorithm::shared_fs_mutex::byte_ranges::fs_mutex_byte_ranges({}, "lockfile");
      if(v.has_error())
      {
        std::cerr << "ERROR: Creation of lock algorithm returns " << v.error().message() << std::endl;
        return;
      }
      algorithm = std::make_unique<afio::algorithm::shared_fs_mutex::byte_ranges>(std::move(v.value()));
      break;
    }
    case lock_algorithm::lock_files:
    {
      auto v = afio::algorithm::shared_fs_mutex::lock_files::fs_mutex_lock_files(base);
      if(v.has_error())
      {
        std::cerr << "ERROR: Creation of lock algorithm returns " << v.error().message() << std::endl;
        return;
      }
      algorithm = std::make_unique<afio::algorithm::shared_fs_mutex::lock_files>(std::move(v.value()));
      break;
    }
    case lock_algorithm::memory_map:
    {
      auto v = afio::algorithm::shared_fs_mutex::memory_map<QUICKCPPLIB_NAMESPACE::algorithm::hash::passthru_hash>::fs_mutex_map({}, "lockfile");
      if(v.has_error())
      {
        std::cerr << "ERROR: Creation of lock algorithm returns " << v.error().message() << std::endl;
        return;
      }
      algorithm = std::make_unique<afio::algorithm::shared_fs_mutex::memory_map<QUICKCPPLIB_NAMESPACE::algorithm::hash::passthru_hash>>(std::move(v.value()));
      break;
    }
    case lock_algorithm::unknown:
      break;
    }
    // Create entities named 0 to total_locks
    std::vector<afio::algorithm::shared_fs_mutex::shared_fs_mutex::entity_type> entities(total_locks);
    for(size_t n = 0; n < total_locks; n++)
    {
      if(contended)
      {
        entities[n].value = n;
        entities[n].exclusive = true;
      }
      else
      {
        entities[n].value = (this_child << 4) + n;  // guaranteed unique
        entities[n].exclusive = true;
      }
    }
    while(done == -1)
      std::this_thread::yield();
    while(!done)
    {
      auto result = algorithm->lock(entities, afio::deadline(), false);
      if(result.has_error())
      {
        std::cerr << "ERROR: Algorithm lock returns " << result.error().message() << std::endl;
        return;
      }
      if(contended)
        child_locks(this_child);
      ++count;
      auto guard = std::move(result.value());
      if(contended)
        child_unlocks(this_child);
      guard.unlock();
    }
  });
  if(!strcmp(argv[1], "!spawned"))
  {
    auto lastcount = count;
    size_t secs = 0;
    done = 0;
    while(!kbhit())
    {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      ++secs;
      std::cout << "\ncount=" << count << " (+" << (count - lastcount) << "), average=" << (count / secs) << std::endl;
      lastcount = count;
#if 1
      auto it = afio::log().cbegin();
      for(size_t n = 0; n < 10; n++)
      {
        if(it == afio::log().cend())
          break;
        std::cout << "   " << *it;
        ++it;
      }
#endif
    }
    done = 1;
    worker.join();
  }
  else
    for(;;)
    {
      char buffer[1024];
      // This blocks
      if(!std::cin.getline(buffer, sizeof(buffer)))
      {
        return 1;
      }
      if(0 == strcmp(buffer, "GO"))
      {
        // Launch worker thread
        done = 0;
      }
      else if(0 == strcmp(buffer, "STOP"))
      {
        done = 1;
        worker.join();
        std::cout << "RESULTS(" << count << ")" << std::endl;
#if DEBUG_CSV
        std::ofstream s("benchmark_locking_afio_log" + std::to_string(this_child) + ".csv");
        s << csv(afio::log());
#endif
        return 0;
      }
    }
}
