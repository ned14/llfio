/* Test the performance of async i/o
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Jan 2020


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
static constexpr int BENCHMARK_DURATION = 10;

/* Note that the IOCP 1 thread does not use locking, and enables IOCP immediate completions.
Whereas the IOCP 2 thread does use locking, and disables IOCP immediate completions. This
makes only the IOCP 2 thread results comparable to ASIO.


Benchmarking Null i/o multiplexer unsynchronised with 1 handles ...
   per-handle create 7e-06 cancel 0 destroy 6e-06
   total i/o min 100 max 3.49408e+08 mean 152.97 stddev 66587.2
             @ 50% 100 @ 95% 200 @ 99% 300 @ 99.9% 1500 @ 99.99% 3800
   total results collected = 47732735

Benchmarking Null i/o multiplexer unsynchronised with 4 handles ...
   per-handle create 2.8e-06 cancel 2.5e-08 destroy 9.5e-07
   total i/o min 300 max 4.47648e+08 mean 457.242 stddev 87215.2
             @ 50% 400 @ 95% 625 @ 99% 950 @ 99.9% 3800 @ 99.99% 9250
   total results collected = 47996924

Benchmarking Null i/o multiplexer unsynchronised with 16 handles ...
   per-handle create 1.69375e-06 cancel 0 destroy 7.125e-07
   total i/o min 1200 max 387200 mean 1509.67 stddev 944.997
             @ 50% 1325 @ 95% 2131.25 @ 99% 3287.5 @ 99.9% 11725 @ 99.99% 27293.8
   total results collected = 54099952

Benchmarking Null i/o multiplexer unsynchronised with 64 handles ...
   per-handle create 7.10938e-07 cancel 1.5625e-09 destroy 1.84375e-07
   total i/o min 4700 max 379100 mean 5767.38 stddev 3311.88
             @ 50% 5142.19 @ 95% 8335.94 @ 99% 12454.7 @ 99.9% 45756.3 @ 99.99% 79676.6
   total results collected = 55771072

Benchmarking Null i/o multiplexer synchronised with 1 handles ...
   per-handle create 1.54e-05 cancel 1e-07 destroy 4.2e-06
   total i/o min 100 max 2.66671e+08 mean 217.766 stddev 62937.9
             @ 50% 200 @ 95% 300 @ 99% 400 @ 99.9% 1600 @ 99.99% 4100
   total results collected = 27370495

Benchmarking Null i/o multiplexer synchronised with 4 handles ...
   per-handle create 3.475e-06 cancel 2.5e-08 destroy 1.15e-06
   total i/o min 400 max 1.7367e+06 mean 557.487 stddev 503.662
             @ 50% 500 @ 95% 725 @ 99% 1100 @ 99.9% 3825 @ 99.99% 9950
   total results collected = 29659132

Benchmarking Null i/o multiplexer synchronised with 16 handles ...
   per-handle create 1.8125e-06 cancel 0 destroy 2.6875e-07
   total i/o min 1600 max 258800 mean 1959.67 stddev 958.701
             @ 50% 1800 @ 95% 2731.25 @ 99% 4150 @ 99.9% 12643.8 @ 99.99% 28087.5
   total results collected = 30474224

Benchmarking Null i/o multiplexer synchronised with 64 handles ...
   per-handle create 7.125e-07 cancel 1.5625e-09 destroy 1.42187e-07
   total i/o min 6400 max 680900 mean 7620.39 stddev 3574.9
             @ 50% 6873.44 @ 95% 10735.9 @ 99% 16279.7 @ 99.9% 49046.9 @ 99.99% 86120.3
   total results collected = 30605248

Warming up ...

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 1 handles ...
   per-handle create 8.48e-05 cancel 0 destroy 2.29e-05
   total i/o min 1800 max 1.7352e+06 mean 2797.39 stddev 1916.54
             @ 50% 2200 @ 95% 6000 @ 99% 7000 @ 99.9% 10900 @ 99.99% 36300
   total results collected = 3343359

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 4 handles ...
   per-handle create 2.6825e-05 cancel 0 destroy 8.15e-06
   total i/o min 3500 max 597300 mean 6067.99 stddev 2926.52
             @ 50% 5175 @ 95% 11650 @ 99% 14150 @ 99.9% 23175 @ 99.99% 60025
   total results collected = 4075516

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 16 handles ...
   per-handle create 1.4e-05 cancel 6.25e-09 destroy 4.5125e-06
   total i/o min 10100 max 296900 mean 20019.7 stddev 8824.73
             @ 50% 16875 @ 95% 37200 @ 99% 44187.5 @ 99.9% 62856.3 @ 99.99% 116219
   total results collected = 4243440

Benchmarking llfio::pipe_handle and IOCP unsynchronised with 64 handles ...
   per-handle create 1.20625e-05 cancel 1.5625e-09 destroy 3.55938e-06
   total i/o min 35200 max 718000 mean 82234.8 stddev 37723.6
             @ 50% 67831.3 @ 95% 144358 @ 99% 175428 @ 99.9% 237444 @ 99.99% 340753
   total results collected = 4063168

Benchmarking llfio::pipe_handle and IOCP synchronised with 1 handles ...
   per-handle create 4.45e-05 cancel 0 destroy 2.53e-05
   total i/o min 1900 max 2.0515e+06 mean 3307.03 stddev 2632.33
             @ 50% 2400 @ 95% 6400 @ 99% 7500 @ 99.9% 12900 @ 99.99% 41100
   total results collected = 2837503

Benchmarking llfio::pipe_handle and IOCP synchronised with 4 handles ...
   per-handle create 2.3275e-05 cancel 2.5e-08 destroy 9.35e-06
   total i/o min 3800 max 535800 mean 6425 stddev 2969.44
             @ 50% 5450 @ 95% 12125 @ 99% 14700 @ 99.9% 22900 @ 99.99% 57325
   total results collected = 3895292

Benchmarking llfio::pipe_handle and IOCP synchronised with 16 handles ...
   per-handle create 1.34688e-05 cancel 0 destroy 5.05e-06
   total i/o min 10900 max 493400 mean 20738.4 stddev 9001.79
             @ 50% 17337.5 @ 95% 37781.3 @ 99% 45787.5 @ 99.9% 64443.8 @ 99.99% 111206
   total results collected = 4095984

Benchmarking llfio::pipe_handle and IOCP synchronised with 64 handles ...
   per-handle create 1.05984e-05 cancel 1.5625e-09 destroy 3.80313e-06
   total i/o min 38900 max 3.5118e+06 mean 84476.3 stddev 40103.2
             @ 50% 69626.6 @ 95% 149853 @ 99% 179666 @ 99.9% 245831 @ 99.99% 328900
   total results collected = 3866560

Warming up ...

Benchmarking ASIO with pipes with 1 handles ...
   per-handle create 8.06e-05 cancel 3.47447 destroy 1.97703
   total i/o min 2500 max 1.1078e+06 mean 3850.29 stddev 2548.2
             @ 50% 3200 @ 95% 7400 @ 99% 9200 @ 99.9% 37800 @ 99.99% 75900
   total results collected = 1899519

Benchmarking ASIO with pipes with 4 handles ...
   per-handle create 3.1e-05 cancel 1.18692 destroy 0.667405
   total i/o min 4800 max 683700 mean 9804.63 stddev 5227.67
             @ 50% 7975 @ 95% 16350 @ 99% 21375 @ 99.9% 58775 @ 99.99% 105950
   total results collected = 2342908

Benchmarking ASIO with pipes with 16 handles ...
   per-handle create 1.8575e-05 cancel 0.322137 destroy 0.186772
   total i/o min 13900 max 1.8979e+06 mean 32992.2 stddev 14976.9
             @ 50% 27331.3 @ 95% 53318.8 @ 99% 71643.8 @ 99.9% 116231 @ 99.99% 184725
   total results collected = 2523120

Benchmarking ASIO with pipes with 64 handles ...
   per-handle create 1.145e-05 cancel 0.0913084 destroy 0.052539
   total i/o min 51700 max 2.6473e+06 mean 122408 stddev 51918.8
             @ 50% 105044 @ 95% 198478 @ 99% 253588 @ 99.9% 415041 @ 99.99% 748452
   total results collected = 2686912
*/

#define LLFIO_ENABLE_TEST_IO_MULTIPLEXERS 1

#include "../../include/llfio/llfio.hpp"

#include "quickcpplib/algorithm/small_prng.hpp"

#include <cfloat>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <typeinfo>
#include <vector>

#if __has_include("asio/asio/include/asio.hpp")
#define ENABLE_ASIO 1
#if defined(__clang__) && defined(_MSC_VER)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#endif
#include "asio/asio/include/asio.hpp"
#if defined(__clang__) && defined(_MSC_VER)
#pragma clang diagnostic pop
#endif
#endif

namespace llfio = LLFIO_V2_NAMESPACE;

struct test_results
{
  int handles{0};
  double creation{0}, cancel{0}, destruction{0};
  struct summary_t
  {
    double min{0}, mean{0}, max{0}, _50{0}, _95{0}, _99{0}, _999{0}, _9999{0}, variance{0};
  } initiate, completion, total;
  size_t total_readings{0};
};

LLFIO_TEMPLATE(class C, class... Args)
LLFIO_TREQUIRES(LLFIO_TPRED(std::is_constructible<C, int, Args...>::value))
test_results do_benchmark(int handles, Args &&... args)
{
  const bool doing_warm_up = (handles < 0);
  handles = abs(handles);
  struct timing_info
  {
    std::chrono::high_resolution_clock::time_point initiate, write, read;

    timing_info(std::chrono::high_resolution_clock::time_point i)
        : initiate(i)
    {
    }
    double ns_total(long long overhead) const { return (double) std::chrono::duration_cast<std::chrono::nanoseconds>(read - initiate).count() - overhead; }
    double ns_initiate(long long overhead) const { return (double) std::chrono::duration_cast<std::chrono::nanoseconds>(write - initiate).count() - overhead; }
    double ns_completion(long long overhead) const { return (double) std::chrono::duration_cast<std::chrono::nanoseconds>(read - write).count() - overhead; }
  };
  std::vector<std::vector<timing_info>> timings(handles);
  for(auto &i : timings)
  {
    i.reserve(10 * 1024 * 1024);
  }

  long long overhead = INT_MAX;
  auto create1 = std::chrono::high_resolution_clock::now();
  if(C::launch_writer_thread)
  {
    for(size_t n = 0; n < 1000; n++)
    {
      auto old = create1;
      create1 = std::chrono::high_resolution_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(create1 - old).count();
      if(diff != 0 && diff < overhead)
      {
        overhead = diff;
      }
    }
  }
  else
  {
    overhead = 0;
  }
  auto ios = C(handles, std::forward<Args>(args)...);
  auto create2 = std::chrono::high_resolution_clock::now();
  std::atomic<int> latch(1);
  std::thread writer_thread;
  if(C::launch_writer_thread)
  {
    writer_thread = std::thread([&]() {
      --latch;
      for(;;)
      {
        int v;
        while((v = latch.load(std::memory_order_relaxed)) == 0)
        {
          // std::this_thread::yield();
        }
        if(v < 0)
        {
          return;
        }
        latch.fetch_sub(1, std::memory_order_relaxed);
        for(int x = 0; x < handles; x++)
        {
          timings[x].back().write = std::chrono::high_resolution_clock::now();
          ios.write(x);
        }
      }
    });
    while(latch == 1)
    {
      std::this_thread::yield();
    }
  }

  for(;;)
  {
    for(size_t n = 0; n < 1024; n++)
    {
      // Begin all the reads
      for(int x = 0; x < handles; x++)
      {
        timings[x].emplace_back(std::chrono::high_resolution_clock::now());
        auto completion = ios.read(x);
        if(timings[x].size() > 1)
        {
          if(!C::launch_writer_thread)
          {
            timings[x][timings[x].size() - 2].write = completion;
          }
          timings[x][timings[x].size() - 2].read = completion;
        }
      }
      // Do the writes
      latch.store(1, std::memory_order_relaxed);
      // Reap the completions
      ios.check();
    }
    if(std::chrono::duration_cast<std::chrono::seconds>(timings[0].back().initiate - timings[0].front().initiate).count() >= (doing_warm_up ? 3U : BENCHMARK_DURATION))
    {
      latch = -1;
      break;
    }
  }
  auto cancel1 = std::chrono::high_resolution_clock::now();
  ios.cancel();
  auto cancel2 = std::chrono::high_resolution_clock::now();
  auto destroy1 = std::chrono::high_resolution_clock::now();
  ios.destroy();
  auto destroy2 = std::chrono::high_resolution_clock::now();
  if(C::launch_writer_thread)
  {
    writer_thread.join();
  }

  test_results ret;
  ret.creation = ((double) std::chrono::duration_cast<std::chrono::nanoseconds>(create2 - create1).count()) / handles / 1000000000.0;
  ret.cancel = ((double) std::chrono::duration_cast<std::chrono::nanoseconds>(cancel2 - cancel1).count()) / handles / 1000000000.0;
  ret.destruction = ((double) std::chrono::duration_cast<std::chrono::nanoseconds>(destroy2 - destroy1).count()) / handles / 1000000000.0;

  ret.initiate.min = DBL_MAX;
  ret.completion.min = DBL_MAX;
  ret.total.min = DBL_MAX;
  for(auto &reader : timings)
  {
    // Pop the last item, whose read value will be zero
    reader.pop_back();
    for(timing_info &timing : reader)
    {
      assert(timing.ns_total(overhead) >= 0);
      assert(timing.ns_initiate(overhead) >= 0);
      assert(timing.ns_completion(overhead) >= 0);
      if(timing.ns_total(overhead) < ret.total.min)
      {
        ret.total.min = timing.ns_total(overhead);
      }
      if(timing.ns_initiate(overhead) < ret.initiate.min)
      {
        ret.initiate.min = timing.ns_initiate(overhead);
      }
      if(timing.ns_completion(overhead) < ret.completion.min)
      {
        ret.completion.min = timing.ns_completion(overhead);
      }
      if(timing.ns_total(overhead) > ret.total.max)
      {
        ret.total.max = timing.ns_total(overhead);
      }
      if(timing.ns_initiate(overhead) > ret.initiate.max)
      {
        ret.initiate.max = timing.ns_initiate(overhead);
      }
      if(timing.ns_completion(overhead) > ret.completion.max)
      {
        ret.completion.max = timing.ns_completion(overhead);
      }
      ret.total.mean += timing.ns_total(overhead);
      ret.initiate.mean += timing.ns_initiate(overhead);
      ret.completion.mean += timing.ns_completion(overhead);
      ++ret.total_readings;
    }
    std::sort(reader.begin(), reader.end(), [&](const timing_info &a, const timing_info &b) { return a.ns_total(overhead) < b.ns_total(overhead); });
    ret.total._50 += reader[(size_t)((reader.size() - 1) * 0.5)].ns_total(overhead);
    ret.total._95 += reader[(size_t)((reader.size() - 1) * 0.95)].ns_total(overhead);
    ret.total._99 += reader[(size_t)((reader.size() - 1) * 0.99)].ns_total(overhead);
    ret.total._999 += reader[(size_t)((reader.size() - 1) * 0.999)].ns_total(overhead);
    ret.total._9999 += reader[(size_t)((reader.size() - 1) * 0.9999)].ns_total(overhead);
  }
  ret.total.mean /= ret.total_readings;
  ret.initiate.mean /= ret.total_readings;
  ret.completion.mean /= ret.total_readings;
  ret.total._50 /= timings.size();
  ret.total._95 /= timings.size();
  ret.total._99 /= timings.size();
  ret.total._999 /= timings.size();
  ret.total._9999 /= timings.size();
  for(auto &reader : timings)
  {
    for(timing_info &timing : reader)
    {
      ret.total.variance += (timing.ns_total(overhead) - ret.total.mean) * (timing.ns_total(overhead) - ret.total.mean);
      ret.initiate.variance += (timing.ns_initiate(overhead) - ret.initiate.mean) * (timing.ns_initiate(overhead) - ret.initiate.mean);
      ret.completion.variance += (timing.ns_completion(overhead) - ret.completion.mean) * (timing.ns_completion(overhead) - ret.completion.mean);
    }
  }
  ret.total.variance /= ret.total_readings;
  ret.initiate.variance /= ret.total_readings;
  ret.completion.variance /= ret.total_readings;
  return ret;
}

template <class C, class... Args> void benchmark(llfio::path_view csv, size_t max_handles, const char *desc, Args &&... args)
{
  std::vector<test_results> results;
  for(int n = 1; n <= max_handles; n <<= 2)
  {
    std::cout << "\nBenchmarking " << desc << " with " << n << " handles ..." << std::endl;
    auto res = do_benchmark<C>(n, std::forward<Args>(args)...);
    res.handles = n;
    results.push_back(res);
    std::cout << "   per-handle create " << res.creation << " cancel " << res.cancel << " destroy " << res.destruction;
    std::cout << "\n   total i/o min " << res.total.min << " max " << res.total.max << " mean " << res.total.mean << " stddev " << sqrt(res.total.variance);
    std::cout << "\n             @ 50% " << res.total._50 << " @ 95% " << res.total._95 << " @ 99% " << res.total._99 << " @ 99.9% " << res.total._999 << " @ 99.99% " << res.total._9999;
    std::cout << "\n   total results collected = " << res.total_readings << std::endl;
  }
  std::ofstream of(csv.path());
  of << "Handles";
  for(auto &i : results)
  {
    of << "," << i.handles;
  }
  of << "\nCreates";
  for(auto &i : results)
  {
    of << "," << i.creation;
  }
  of << "\nCancels";
  for(auto &i : results)
  {
    of << "," << i.cancel;
  }
  of << "\nDestroys";
  for(auto &i : results)
  {
    of << "," << i.destruction;
  }
  of << "\nMins";
  for(auto &i : results)
  {
    of << "," << i.total.min;
  }
  of << "\nMaxs";
  for(auto &i : results)
  {
    of << "," << i.total.max;
  }
  of << "\nMeans";
  for(auto &i : results)
  {
    of << "," << i.total.mean;
  }
  of << "\nStddevs";
  for(auto &i : results)
  {
    of << "," << sqrt(i.total.variance);
  }
  of << "\n50%s";
  for(auto &i : results)
  {
    of << "," << i.total._50;
  }
  of << "\n95%s";
  for(auto &i : results)
  {
    of << "," << i.total._95;
  }
  of << "\n99%s";
  for(auto &i : results)
  {
    of << "," << i.total._99;
  }
  of << "\n99.9%s";
  for(auto &i : results)
  {
    of << "," << i.total._999;
  }
  of << "\n99.99%s";
  for(auto &i : results)
  {
    of << "," << i.total._9999;
  }
  of << "\n";
}

struct NoHandle final : public llfio::io_handle
{
  using mode = typename llfio::io_handle::mode;
  using creation = typename llfio::io_handle::creation;
  using caching = typename llfio::io_handle::caching;
  using flag = typename llfio::io_handle::flag;
  using buffer_type = typename llfio::io_handle::buffer_type;
  using const_buffer_type = typename llfio::io_handle::const_buffer_type;
  using buffers_type = typename llfio::io_handle::buffers_type;
  using const_buffers_type = typename llfio::io_handle::const_buffers_type;
  template <class T> using io_request = typename llfio::io_handle::template io_request<T>;
  template <class T> using io_result = typename llfio::io_handle::template io_result<T>;

  NoHandle()
      : llfio::io_handle(llfio::native_handle_type(llfio::native_handle_type::disposition::nonblocking | llfio::native_handle_type::disposition::readable | llfio::native_handle_type::disposition::writable, -2 /* fake being open */), llfio::io_handle::caching::all, llfio::io_handle::flag::multiplexable, nullptr)
  {
  }
  ~NoHandle()
  {
    this->_v._init = -1;  // fake closed
  }
  NoHandle(const NoHandle &) = delete;
  NoHandle(NoHandle &&) = default;
  virtual llfio::result<void> close() noexcept override
  {
    this->_v._init = -1;  // fake closed
    return llfio::success();
  }
  virtual io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, llfio::deadline d) noexcept override
  {
    (void) d;
    return reqs.buffers;
  }
};
LLFIO_V2_NAMESPACE_BEGIN
template <> struct construct<NoHandle>
{
  pipe_handle::path_view_type _path;
  pipe_handle::mode _mode = pipe_handle::mode::read;
  pipe_handle::creation _creation = pipe_handle::creation::if_needed;
  pipe_handle::caching _caching = pipe_handle::caching::all;
  pipe_handle::flag flags = pipe_handle::flag::none;
  const path_handle &base = path_discovery::temporary_named_pipes_directory();
  result<NoHandle> operator()() const noexcept { return success(); }
};
LLFIO_V2_NAMESPACE_END

template <class HandleType = NoHandle> struct benchmark_llfio
{
  using mode = typename HandleType::mode;
  using creation = typename HandleType::creation;
  using caching = typename HandleType::caching;
  using flag = typename HandleType::flag;
  using buffer_type = typename HandleType::buffer_type;
  using const_buffer_type = typename HandleType::const_buffer_type;
  using buffers_type = typename HandleType::buffers_type;
  using const_buffers_type = typename HandleType::const_buffers_type;
  template <class T> using io_request = typename HandleType::template io_request<T>;
  template <class T> using io_result = typename HandleType::template io_result<T>;

  static constexpr bool launch_writer_thread = !std::is_same<HandleType, NoHandle>::value;

  struct receiver_type final : public llfio::io_multiplexer::io_operation_state_visitor
  {
    benchmark_llfio *parent{nullptr};
    HandleType read_handle;
    std::unique_ptr<llfio::byte[]> io_state_ptr;
    llfio::byte _buffer[sizeof(size_t)];
    buffer_type buffer;
    llfio::io_multiplexer::io_operation_state *io_state{nullptr};
    std::chrono::high_resolution_clock::time_point when_read_completed;

    explicit receiver_type(benchmark_llfio *_parent, HandleType &&h)
        : parent(_parent)
        , read_handle(std::move(h))
        , io_state_ptr(std::make_unique<llfio::byte[]>(read_handle.multiplexer()->io_state_requirements().first))
        , buffer(_buffer, sizeof(_buffer))
    {
      memset(_buffer, 0, sizeof(_buffer));
    }
    receiver_type(const receiver_type &) = delete;
    receiver_type(receiver_type &&o) noexcept
        : parent(o.parent)
        , read_handle(std::move(o.read_handle))
        , io_state_ptr(std::move(o.io_state_ptr))
    {
      if(o.io_state != nullptr)
      {
        abort();
      }
      o.parent = nullptr;
    }
    receiver_type &operator=(const receiver_type &) = delete;
    receiver_type &operator=(receiver_type &&) = default;
    ~receiver_type()
    {
      if(io_state != nullptr)
      {
        if(!is_finished(io_state->current_state()))
        {
          abort();
        }
        io_state->~io_operation_state();
        io_state = nullptr;
      }
    }

    // Initiate the read
    void begin_io()
    {
      if(io_state != nullptr)
      {
        if(!is_finished(io_state->current_state()))
        {
          abort();
        }
      }
      buffer = {_buffer, sizeof(_buffer)};
      io_state = read_handle.multiplexer()->construct_and_init_io_operation({io_state_ptr.get(), 4096 /*lies*/}, &read_handle, this, {}, {}, io_request<buffers_type>({&buffer, 1}, 0));
    }

    // Called when the read completes
    virtual bool read_completed(llfio::io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type /*former*/, io_result<buffers_type> &&res) override
    {
      when_read_completed = std::chrono::high_resolution_clock::now();
      if(!res)
      {
        abort();
      }
      if(res)
      {
        if(res.value().size() != 1)
        {
          abort();
        }
      }
      return true;
    }

    // Called when the state for the read can be disposed
    virtual void read_finished(llfio::io_multiplexer::io_operation_state::lock_guard & /*g*/, llfio::io_operation_state_type /*former*/) override
    {
      io_state->~io_operation_state();
      io_state = nullptr;
    }
  };

  llfio::io_multiplexer_ptr multiplexer;
  std::vector<HandleType> write_handles;
  std::vector<receiver_type> read_states;
  receiver_type *to_restart{nullptr};

  explicit benchmark_llfio(size_t count, llfio::io_multiplexer_ptr (*make_multiplexer)())
  {
    multiplexer = make_multiplexer();
    read_states.reserve(count);
    // Construct read and write sides of the pipes
    llfio::filesystem::path::value_type name[64] = {'l', 'l', 'f', 'i', 'o', '_'};
    for(size_t n = 0; n < count; n++)
    {
      name[QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(name + 6, 58, (const char *) &n, sizeof(n))] = 0;
      auto h = llfio::construct<HandleType>{name, mode::read, creation::if_needed, caching::all, flag::multiplexable}().value();
      h.set_multiplexer(multiplexer.get()).value();
      read_states.emplace_back(this, std::move(h));
      write_handles.push_back(llfio::construct<HandleType>{name, mode::write, creation::open_existing, caching::all, flag::multiplexable}().value());
    }
  }
  std::chrono::high_resolution_clock::time_point read(unsigned which)
  {
    auto ret = read_states[which].when_read_completed;
    read_states[which].when_read_completed = {};
    read_states[which].begin_io();
    return ret;
  }
  void check()
  {
    size_t done = 0;
    do
    {
#if 0
      done = 0;
      for(auto &i : read_states)
      {
        if(i.io_state == nullptr || is_finished(multiplexer->check_io_operation(i.io_state)))
        {
          done++;
        }
      }
#else
      done += multiplexer->check_for_any_completed_io().value().initiated_ios_finished;
#endif
    } while(done < read_states.size());
  }
  void write(unsigned which)
  {
    llfio::byte c = llfio::to_byte(78);
    const_buffer_type b(&c, 1);
    write_handles[which].write(io_request<const_buffers_type>({&b, 1}, 0)).value();
  }
  void cancel()
  {
    for(auto &i : read_states)
    {
      if(i.io_state != nullptr)
      {
        (void) multiplexer->cancel_io_operation(i.io_state);
      }
    }
  }
  void destroy()
  {
    read_states.clear();
    multiplexer.reset();
  }
};

#if ENABLE_ASIO
struct benchmark_asio_pipe
{
#ifdef _WIN32
  using handle_type = asio::windows::stream_handle;
#else
  using handle_type = asio::posix::stream_descriptor;
#endif
  static constexpr bool launch_writer_thread = true;

  struct read_state
  {
    char raw_buffer[1];
    handle_type read_handle;
    std::chrono::high_resolution_clock::time_point when_read_completed;

    explicit read_state(handle_type &&h)
        : read_handle(std::move(h))
    {
    }
    void begin_io()
    {
      read_handle.async_read_some(asio::buffer(raw_buffer, 1), [this](const auto & /*unused*/, const auto & /*unused*/) {
        when_read_completed = std::chrono::high_resolution_clock::now();
        begin_io();
      });
    }
  };

  llfio::optional<asio::io_context> multiplexer;
  std::vector<handle_type> write_handles;
  std::vector<read_state> read_states;

  explicit benchmark_asio_pipe(size_t count, int threads)
  {
    multiplexer.emplace(threads);
    read_states.reserve(count);
    write_handles.reserve(count);
    // Construct read and write sides of the pipes
    llfio::filesystem::path::value_type name[64] = {'l', 'l', 'f', 'i', 'o', '_'};
    for(size_t n = 0; n < count; n++)
    {
      using mode = typename llfio::pipe_handle::mode;
      using creation = typename llfio::pipe_handle::creation;
      using caching = typename llfio::pipe_handle::caching;
      using flag = typename llfio::pipe_handle::flag;

      name[QUICKCPPLIB_NAMESPACE::algorithm::string::to_hex_string(name + 6, 58, (const char *) &n, sizeof(n))] = 0;
      auto read_handle = llfio::construct<llfio::pipe_handle>{name, mode::read, creation::if_needed, caching::all, flag::multiplexable}().value();
      auto write_handle = llfio::construct<llfio::pipe_handle>{name, mode::write, creation::open_existing, caching::all, flag::multiplexable}().value();
#ifdef _WIN32
      read_states.emplace_back(handle_type(*multiplexer, read_handle.release().h));
      write_handles.emplace_back(handle_type(*multiplexer, write_handle.release().h));
#else
      read_states.emplace_back(handle_type(*multiplexer, read_handle.release().fd));
      write_handles.emplace_back(handle_type(*multiplexer, write_handle.release().fd));
#endif
    }
  }
  std::chrono::high_resolution_clock::time_point read(unsigned which)
  {
    auto ret = read_states[which].when_read_completed;
    read_states[which].when_read_completed = {};
    read_states[which].begin_io();
    return ret;
  }
  void check()
  {
    for(;;)
    {
      size_t count = 0;
      for(auto &i : read_states)
      {
        if(i.when_read_completed != std::chrono::high_resolution_clock::time_point())
        {
          count++;
        }
      }
      if(count == read_states.size())
      {
        break;
      }
      multiplexer->poll();  // supposedly does not return until no completions are pending
    }
  }
  void write(unsigned which)
  {
    char c = 78;
    write_handles[which].write_some(asio::buffer(&c, 1));
  }
  void cancel()
  {
    for(auto &i : read_states)
    {
      i.read_handle.cancel();
    }
    multiplexer->poll();  // does not return until no completions are pending
  }
  void destroy()
  {
    write_handles.clear();
    read_states.clear();
    multiplexer.reset();
  }
};
#endif

int main(void)
{
  std::cout << "Warming up ..." << std::endl;
  do_benchmark<benchmark_llfio<>>(-1, []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_null(2, true).value(); });
  benchmark<benchmark_llfio<>>("llfio-null-unsynchronised.csv", 64, "Null i/o multiplexer unsynchronised", []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_null(1, false).value(); });
  benchmark<benchmark_llfio<>>("llfio-null-synchronised.csv", 64, "Null i/o multiplexer synchronised", []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_null(2, true).value(); });

#ifdef _WIN32
  std::cout << "\nWarming up ..." << std::endl;
  do_benchmark<benchmark_llfio<llfio::pipe_handle>>(-1, []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(2, true).value(); });
  // No locking, enable IOCP immediate completions. ASIO can't compete with this.
  benchmark<benchmark_llfio<llfio::pipe_handle>>("llfio-pipe-handle-unsynchronised.csv", 64, "llfio::pipe_handle and IOCP unsynchronised", []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(1, false).value(); });
  // Locking enabled, disable IOCP immediate completions so it's a fair comparison with ASIO
  benchmark<benchmark_llfio<llfio::pipe_handle>>("llfio-pipe-handle-synchronised.csv", 64, "llfio::pipe_handle and IOCP synchronised", []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(2, true).value(); });
#endif

#if ENABLE_ASIO
  std::cout << "\nWarming up ..." << std::endl;
  do_benchmark<benchmark_asio_pipe>(-1, 2);
  benchmark<benchmark_asio_pipe>("asio-pipe-handle-synchronised.csv", 64, "ASIO with pipes synchronised", 2);
#endif

  return 0;
}