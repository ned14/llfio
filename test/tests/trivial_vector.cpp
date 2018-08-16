/* Integration test kernel for section allocator adapters
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Nov 2017


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

static uint64_t trivial_vector_udts_constructed = 78;
static inline void TestTrivialVector()
{
  // Test udt without default constructor, complex constructor and trivial everything else
  struct udt
  {
    uint64_t v, _space[7];  // 64 bytes total
    udt() = delete;
    explicit udt(int /*unused*/) : v(trivial_vector_udts_constructed++)
    , _space{ 1, 2, 3, 4, 5, 6, 7 }
    {
    }
  };
  using udt_vector = LLFIO_V2_NAMESPACE::algorithm::trivial_vector<udt>;

  constexpr size_t _4kb = 4096 / sizeof(udt), _16kb = 16384 / sizeof(udt), _64kb = 65536 / sizeof(udt);
  udt_vector v;
  BOOST_CHECK(v.empty());
  BOOST_CHECK(v.size() == 0);  // NOLINT
  std::cout << "Resizing to 4Kb ..." << std::endl;
  v.push_back(udt(5));         // first allocation of 4Kb
  BOOST_CHECK(v.size() == 1);
  BOOST_CHECK(v.capacity() == LLFIO_V2_NAMESPACE::utils::page_size() / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  std::cout << "Resizing to capacity ..." << std::endl;
  v.resize(_4kb, udt(6));       // ought to be precisely 4Kb
  BOOST_CHECK(v.size() == _4kb);
  BOOST_CHECK(v.capacity() == LLFIO_V2_NAMESPACE::utils::round_up_to_page_size(4096) / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  BOOST_REQUIRE(v[1].v == 79);
  std::cout << "Resizing to 16Kb ..." << std::endl;
  v.resize(_16kb, udt(7));     // 16Kb
  BOOST_CHECK(v.size() == _16kb);
  BOOST_CHECK(v.capacity() == LLFIO_V2_NAMESPACE::utils::round_up_to_page_size(16384) / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  BOOST_REQUIRE(v[1].v == 79);
  BOOST_REQUIRE(v[_4kb].v == 80);
  std::cout << "Resizing to 64Kb ..." << std::endl;
  v.resize(_64kb, udt(8));     // 64Kb
  BOOST_CHECK(v.size() == _64kb);
  BOOST_CHECK(v.capacity() == LLFIO_V2_NAMESPACE::utils::round_up_to_page_size(65536) / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  BOOST_REQUIRE(v[1].v == 79);
  BOOST_REQUIRE(v[_4kb].v == 80);
  BOOST_REQUIRE(v[_16kb].v == 81);
  for(size_t n = 0; n < _64kb; n++)
  {
    if(0 == n)
    {
      BOOST_CHECK(v[n].v == 78);
    }
    else if(n < _4kb)
    {
      BOOST_CHECK(v[n].v == 79);
    }
    else if(n < _16kb)
    {
      BOOST_CHECK(v[n].v == 80);
    }
    else if(n < _64kb)
    {
      BOOST_CHECK(v[n].v == 81);
    }
  }
  auto it = v.begin();
  for(size_t n = 0; n < _64kb; n++, ++it)
  {
    if(0 == n)
    {
      BOOST_CHECK(it->v == 78);
    }
    else if(n < _4kb)
    {
      BOOST_CHECK(it->v == 79);
    }
    else if(n < _16kb)
    {
      BOOST_CHECK(it->v == 80);
    }
    else if(n < _64kb)
    {
      BOOST_CHECK(it->v == 81);
    }
  }
  BOOST_CHECK(it == v.end());
}

inline std::string printKb(size_t bytes)
{
  if(bytes >= 1024 * 1024 * 1024)
    return std::to_string(bytes / 1024 / 1024 / 1024) + " Gb";
  if(bytes >= 1024 * 1024)
    return std::to_string(bytes / 1024 / 1024) + " Mb";
  if(bytes >= 1024 * 1024 * 1024)
    return std::to_string(bytes / 1024) + " Kb";
  return std::to_string(bytes) + " bytes";
}

template <class T> unsigned long long BenchmarkVector(T &v, size_t no)
{
  typename T::value_type i;
  auto begin = std::chrono::high_resolution_clock::now();
  for(size_t n = 0; n < no; n++)
  {
    v.push_back(i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
}

static inline void BenchmarkTrivialVector1()
{
  struct udt
  {
    uint64_t v[8];  // 64 bytes total
    constexpr udt()  // NOLINT
    : v{ 1, 2, 3, 4, 5, 6, 7, 8 }
    {
    }
  };
  std::ofstream csv("trivial_vector2.csv");
  unsigned long long times[32][3];
  {
    auto begin = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 3)
    {
    }
  }
  size_t idx = 0;
  for(size_t items = 128; items < 16 * 1024 * 1024; items *= 2)
  {
    times[idx][0] = items * sizeof(udt);
    std::vector<udt> v1;
    v1.reserve(1);
    times[idx][1] = BenchmarkVector(v1, items);
    ++idx;
  }
  {
    auto begin = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 3)
    {
    }
  }
  idx = 0;
  for(size_t items = 128; items < 16 * 1024 * 1024; items *= 2)
  {
    LLFIO_V2_NAMESPACE::algorithm::trivial_vector<udt> v2;
    v2.reserve(1);
    times[idx][2] = BenchmarkVector(v2, items);
    ++idx;
  }
  for(size_t n = 0; n < idx; n++)
  {
    csv << times[n][0] << "," << times[n][1] << "," << times[n][2] << std::endl;
    std::cout << "                    std::vector<udt> inserts " << printKb(times[n][0]) << " in " << times[n][1] << " microseconds" << std::endl;
    std::cout << "llfio::algorithm::trivial_vector<udt> inserts " << printKb(times[n][0]) << " in " << times[n][2] << " microseconds" << std::endl;
  }
}

static inline void BenchmarkTrivialVector2()
{
  struct udt
  {
    uint64_t v[8];  // 64 bytes total
    constexpr udt(int /*unused*/)  // NOLINT
    : v{ 1, 2, 3, 4, 5, 6, 7, 8 }
    {
    }
  };
  std::ofstream csv("trivial_vector3.csv");
  unsigned long long times[32][3];
  std::chrono::high_resolution_clock::time_point begin, end;
  {
    begin = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 3)
    {
    }
  }
  size_t idx = 0;
  for(size_t items = 128; items < 16 * 1024 * 1024; items *= 2)
  {
    times[idx][0] = items * sizeof(udt);
    {
      std::vector<udt> v1;
      v1.resize(items, 5);
      begin = std::chrono::high_resolution_clock::now();
      v1.resize(items * 2, 6);
      end = std::chrono::high_resolution_clock::now();
    }
    times[idx][1] = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    ++idx;
  }
  {
    begin = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 3)
    {
    }
  }
  idx = 0;
  for(size_t items = 128; items < 16 * 1024 * 1024; items *= 2)
  {
    {
      LLFIO_V2_NAMESPACE::algorithm::trivial_vector<udt> v1;
      v1.resize(items, 5);
      begin = std::chrono::high_resolution_clock::now();
      v1.resize(items * 2, 6);
      end = std::chrono::high_resolution_clock::now();
    }
    times[idx][2] = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    ++idx;
  }
  for(size_t n = 0; n < idx; n++)
  {
    csv << times[n][0] << "," << times[n][1] << "," << times[n][2] << std::endl;
    std::cout << "                    std::vector<udt> resizes " << printKb(times[n][0]) << " in " << times[n][1] << " microseconds" << std::endl;
    std::cout << "llfio::algorithm::trivial_vector<udt> resizes " << printKb(times[n][0]) << " in " << times[n][2] << " microseconds" << std::endl;
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, trivial_vector, "Tests that llfio::algorithm::trivial_vector works as expected", TestTrivialVector())
KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, trivial_vector2, "Benchmarks llfio::algorithm::trivial_vector against std::vector with push_back()", BenchmarkTrivialVector1())
KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, trivial_vector3, "Benchmarks llfio::algorithm::trivial_vector against std::vector with resize()", BenchmarkTrivialVector2())
