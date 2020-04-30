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

/*
Benchmarking llfio::pipe_handle and IOCP 1 thread with 1 handles ...
   per-handle create 5.35e-05 cancel 0 destroy 1.82e-05
   total i/o min 1800 max 344200 mean 2484.7 stddev 1126.92
             @ 50% 2200 @ 95% 3800 @ 99% 6700 @ 99.9% 8700 @ 99.99% 32100
   total results collected = 3745791

Benchmarking llfio::pipe_handle and IOCP 1 thread with 4 handles ...
   per-handle create 3.415e-05 cancel 0 destroy 7.025e-06
   total i/o min 3500 max 321200 mean 5770.29 stddev 2508.58
             @ 50% 4900 @ 95% 12100 @ 99% 13725 @ 99.9% 18650 @ 99.99% 42800
   total results collected = 4300796

Benchmarking llfio::pipe_handle and IOCP 1 thread with 16 handles ...
   per-handle create 1.35562e-05 cancel 6.25e-09 destroy 4.675e-06
   total i/o min 10100 max 494200 mean 17392.2 stddev 6125.96
             @ 50% 15675 @ 95% 26325 @ 99% 38337.5 @ 99.9% 51443.8 @ 99.99% 78737.5
   total results collected = 4882416

Benchmarking llfio::pipe_handle and IOCP 1 thread with 64 handles ...
   per-handle create 1.11453e-05 cancel 1.5625e-09 destroy 3.25625e-06
   total i/o min 35500 max 550900 mean 68961.4 stddev 26275.6
             @ 50% 61817.2 @ 95% 135423 @ 99% 152256 @ 99.9% 203555 @ 99.99% 282969
   total results collected = 4784064

Benchmarking llfio::pipe_handle and IOCP 2 threads with 1 handles ...
   per-handle create 7.49e-05 cancel 1e-07 destroy 1.77e-05
   total i/o min 2000 max 325100 mean 2322.99 stddev 674.913
             @ 50% 2200 @ 95% 2800 @ 99% 5500 @ 99.9% 7500 @ 99.99% 22500
   total results collected = 3996671

Benchmarking llfio::pipe_handle and IOCP 2 threads with 4 handles ...
   per-handle create 2.52e-05 cancel 2.5e-08 destroy 7.525e-06
   total i/o min 3900 max 2.0285e+06 mean 5438.3 stddev 2669.87
             @ 50% 5025 @ 95% 7200 @ 99% 12525 @ 99.9% 16775 @ 99.99% 38750
   total results collected = 4595708

Benchmarking llfio::pipe_handle and IOCP 2 threads with 16 handles ...
   per-handle create 1.4175e-05 cancel 6.25e-09 destroy 4.1125e-06
   total i/o min 11200 max 196800 mean 17265.3 stddev 3723.17
             @ 50% 16675 @ 95% 21118.8 @ 99% 26143.8 @ 99.9% 38668.8 @ 99.99% 61212.5
   total results collected = 4964336

Benchmarking llfio::pipe_handle and IOCP 2 threads with 64 handles ...
   per-handle create 9.15313e-06 cancel 1.5625e-09 destroy 4.40781e-06
   total i/o min 39700 max 513000 mean 67858.5 stddev 18901.7
             @ 50% 64514.1 @ 95% 87418.8 @ 99% 147325 @ 99.9% 192634 @ 99.99% 247348
   total results collected = 4849600

Warming up ...

Benchmarking ASIO with pipes with 1 handles ...
   per-handle create 6.99e-05 cancel 3.79796 destroy 2.14015
   total i/o min 2500 max 195300 mean 3315.1 stddev 1842.59
             @ 50% 3000 @ 95% 6100 @ 99% 7700 @ 99.9% 36200 @ 99.99% 61100
   total results collected = 2197503

Benchmarking ASIO with pipes with 4 handles ...
   per-handle create 2.81e-05 cancel 1.37405 destroy 0.803539
   total i/o min 4800 max 279900 mean 7914.51 stddev 3221.7
             @ 50% 7300 @ 95% 10550 @ 99% 16375 @ 99.9% 48000 @ 99.99% 76675
   total results collected = 2871292

Benchmarking ASIO with pipes with 16 handles ...
   per-handle create 1.90375e-05 cancel 0.383017 destroy 0.236401
   total i/o min 14100 max 383600 mean 26501.8 stddev 8173.35
             @ 50% 24875 @ 95% 34231.3 @ 99% 57575 @ 99.9% 85012.5 @ 99.99% 115744
   total results collected = 3145712

Benchmarking ASIO with pipes with 64 handles ...
   per-handle create 1.15359e-05 cancel 0.0977114 destroy 0.0637886
   total i/o min 52600 max 664000 mean 103353 stddev 30469.6
             @ 50% 95793.8 @ 95% 143414 @ 99% 195903 @ 99.9% 248303 @ 99.99% 343616
   total results collected = 3145664
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
  for(size_t n = 0; n < 1000; n++)
  {
    auto old = create1;
    create1 = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::nanoseconds>(create1 - old).count();
    if(diff != 0 && diff < overhead)
    {
      overhead = diff;
    }
  };
  auto ios = C(handles, std::forward<Args>(args)...);
  auto create2 = std::chrono::high_resolution_clock::now();
  std::atomic<int> latch(1);
  std::thread writer_thread([&]() {
    --latch;
    for(;;)
    {
      int v;
      while((v = latch.load(std::memory_order_relaxed)) == 0)
      {
        //std::this_thread::yield();
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
  writer_thread.join();

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

template <class HandleType> struct benchmark_llfio
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

  struct receiver_type final : public llfio::io_multiplexer::io_operation_state_visitor
  {
    benchmark_llfio *parent{nullptr};
    HandleType read_handle;
    std::unique_ptr<llfio::byte[]> io_state_ptr;
    llfio::byte _buffer[sizeof(size_t)];
    llfio::pipe_handle::buffer_type buffer;
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
      done += multiplexer->check_for_any_completed_io().value().initiated_ios_completed;
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
#ifdef _WIN32
  std::cout << "Warming up ..." << std::endl;
  do_benchmark<benchmark_llfio<llfio::pipe_handle>>(-1, []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(2, true).value(); });
  benchmark<benchmark_llfio<llfio::pipe_handle>>("llfio-pipe-handle-1-readers.csv", 64, "llfio::pipe_handle and IOCP 1 thread", []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(1, false).value(); });
  benchmark<benchmark_llfio<llfio::pipe_handle>>("llfio-pipe-handle-2-readers.csv", 64, "llfio::pipe_handle and IOCP 2 threads", []() -> llfio::io_multiplexer_ptr { return llfio::test::multiplexer_win_iocp(2, true).value(); });
#endif
#if ENABLE_ASIO
  std::cout << "\nWarming up ..." << std::endl;
  do_benchmark<benchmark_asio_pipe>(-1, 2);
  benchmark<benchmark_asio_pipe>("asio-pipe-handle-readers.csv", 64, "ASIO with pipes", 2);
#endif
  return 0;
}