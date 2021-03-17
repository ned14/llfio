/* Test the performance of dynamic thread pool group
(C) 2021 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Feb 2021


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

//! Seconds to run the benchmark
static constexpr unsigned BENCHMARK_DURATION = 10;
//! Maximum work items to create
static constexpr unsigned MAX_WORK_ITEMS = 1024;
// Size of buffer to SHA256
static constexpr unsigned SHA256_BUFFER_SIZE = 4096;

#include "../../include/llfio/llfio.hpp"

#include "quickcpplib/algorithm/small_prng.hpp"

#include <cfloat>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <tuple>
#include <vector>

#if __has_include("../asio/asio/include/asio.hpp")
#define ENABLE_ASIO 1
#if defined(__clang__) && defined(_MSC_VER)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#endif
#include "../asio/asio/include/asio.hpp"
#if defined(__clang__) && defined(_MSC_VER)
#pragma clang diagnostic pop
#endif
#endif

namespace llfio = LLFIO_V2_NAMESPACE;

struct llfio_runner
{
  std::atomic<bool> cancel{false};
  llfio::dynamic_thread_pool_group_ptr group = llfio::make_dynamic_thread_pool_group().value();
  std::vector<llfio::dynamic_thread_pool_group::work_item *> workitems;

  ~llfio_runner()
  {
    for(auto *p : workitems)
    {
      delete p;
    }
  }
  template <class F> void add_workitem(F &&f)
  {
    struct workitem final : public llfio::dynamic_thread_pool_group::work_item
    {
      llfio_runner *parent;
      F f;
      workitem(llfio_runner *_parent, F &&_f)
          : parent(_parent)
          , f(std::move(_f))
      {
      }
      virtual intptr_t next(llfio::deadline & /*unused*/) noexcept override { return parent->cancel.load(std::memory_order_relaxed) ? -1 : 1; }
      virtual llfio::result<void> operator()(intptr_t /*unused*/) noexcept override
      {
        f();
        return llfio::success();
      }
    };
    workitems.push_back(new workitem(this, std::move(f)));
  }
  std::chrono::microseconds run(unsigned seconds)
  {
    group->submit(workitems).value();
    auto begin = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    cancel.store(true, std::memory_order_release);
    group->wait().value();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
  }
};


#if ENABLE_ASIO
struct asio_runner
{
  std::atomic<bool> cancel{false};
  asio::io_context ctx;

  template <class F> struct C
  {
    asio_runner *parent;
    F f;
    C(asio_runner *_parent, F &&_f)
        : parent(_parent)
        , f(std::move(_f))
    {
    }
    void operator()() const
    {
      f();
      if(!parent->cancel.load(std::memory_order_relaxed))
      {
        parent->ctx.post(*this);
      }
    }
  };
  template <class F> void add_workitem(F &&f) { ctx.post(C<F>(this, std::move(f))); }
  std::chrono::microseconds run(unsigned seconds)
  {
    std::vector<std::thread> threads;
    for(size_t n = 0; n < std::thread::hardware_concurrency() * 2; n++)
    {
      threads.emplace_back([&] { ctx.run(); });
    }
    auto begin = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::seconds(seconds));
    cancel.store(true, std::memory_order_release);
    for(auto &i : threads)
    {
      i.join();
    }
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
  }
};
#endif

template <class Runner> void benchmark(const char *name)
{
  std::cout << "\nBenchmarking " << name << " ..." << std::endl;
  struct shared_t
  {
    std::atomic<unsigned> concurrency{0};
    std::atomic<unsigned> max_concurrency{0};
  };
  struct worker
  {
    shared_t *shared;
    char buffer[SHA256_BUFFER_SIZE];
    QUICKCPPLIB_NAMESPACE::algorithm::hash::sha256_hash::result_type hash;
    uint64_t count{0};

    void operator()()
    {
      auto concurrency = shared->concurrency.fetch_add(1, std::memory_order_relaxed) + 1;
      if(concurrency > shared->max_concurrency.load(std::memory_order_relaxed))
      {
        shared->max_concurrency.store(concurrency, std::memory_order_relaxed);
      }
      hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::sha256_hash::hash(buffer, sizeof(buffer));
      count++;
      shared->concurrency.fetch_sub(1, std::memory_order_relaxed);
    }
    explicit worker(shared_t *_shared)
        : shared(_shared)
    {
    }
  };
  std::vector<worker> workers;
  std::vector<std::tuple<size_t, double, unsigned>> results;
  QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng rand;
  for(size_t items = 1; items <= MAX_WORK_ITEMS; items <<= 1)
  {
    shared_t shared;
    workers.clear();
    for(size_t n = 0; n < items; n++)
    {
      workers.emplace_back(&shared);
      for(size_t i = 0; i < sizeof(worker::buffer); i += 4)
      {
        auto *p = (uint32_t *) (workers.back().buffer + i);
        *p = rand();
      }
    }
    Runner runner;
    for(auto &i : workers)
    {
      runner.add_workitem([&] { i(); });
    }
    auto duration = runner.run(BENCHMARK_DURATION);
    uint64_t total = 0;
    for(auto &i : workers)
    {
      total += i.count;
    }
    results.emplace_back(items, 1000000.0 * total / duration.count(), shared.max_concurrency);
    std::cout << "   For " << std::get<0>(results.back()) << " work items got " << std::get<1>(results.back()) << " SHA256 hashes/sec with "
              << std::get<2>(results.back()) << " maximum concurrency." << std::endl;
  }
  std::ofstream out(std::string(name) + "_results.csv");
  out << R"("Work items","SHA256 hashes/sec","Max concurrency")";
  for(auto &i : results)
  {
    out << "\n" << std::get<0>(i) << "," << std::get<1>(i) << "," << std::get<2>(i);
  }
  out << std::endl;
}

int main(void)
{
  std::string llfio_name("llfio (");
  llfio_name.append(llfio::dynamic_thread_pool_group::implementation_description());
  llfio_name.push_back(')');
  benchmark<llfio_runner>(llfio_name.c_str());

#if ENABLE_ASIO
  benchmark<asio_runner>("asio");
#endif
  return 0;
}