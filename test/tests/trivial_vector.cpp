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
    uint64_t v;
    udt() = delete;
    explicit udt(int /*unused*/) { v = trivial_vector_udts_constructed++; };
  };
  using udt_vector = AFIO_V2_NAMESPACE::algorithm::trivial_vector<udt>;

  udt_vector v;
  BOOST_CHECK(v.empty());
  BOOST_CHECK(v.size() == 0);  // NOLINT
  std::cout << "Resizing to 4Kb ..." << std::endl;
  v.push_back(udt(5));         // first allocation of 4Kb
  BOOST_CHECK(v.size() == 1);
  BOOST_CHECK(v.capacity() == AFIO_V2_NAMESPACE::utils::page_size() / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  std::cout << "Resizing to capacity ..." << std::endl;
  v.resize(512, udt(6));       // ought to be precisely 4Kb
  BOOST_CHECK(v.size() == 512);
  BOOST_CHECK(v.capacity() == AFIO_V2_NAMESPACE::utils::round_up_to_page_size(512 * sizeof(udt)) / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  BOOST_REQUIRE(v[1].v == 79);
  std::cout << "Resizing to 16Kb ..." << std::endl;
  v.resize(2048, udt(7));     // 16Kb
  BOOST_CHECK(v.size() == 2048);
  BOOST_CHECK(v.capacity() == AFIO_V2_NAMESPACE::utils::round_up_to_page_size(2048 * sizeof(udt)) / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  BOOST_REQUIRE(v[1].v == 79);
  BOOST_REQUIRE(v[512].v == 80);
  std::cout << "Resizing to 64Kb ..." << std::endl;
  v.resize(8192, udt(8));     // 64Kb
  BOOST_CHECK(v.size() == 8192);
  BOOST_CHECK(v.capacity() == AFIO_V2_NAMESPACE::utils::round_up_to_page_size(8192 * sizeof(udt)) / sizeof(udt));
  BOOST_REQUIRE(v[0].v == 78);
  BOOST_REQUIRE(v[1].v == 79);
  BOOST_REQUIRE(v[512].v == 80);
  BOOST_REQUIRE(v[2048].v == 81);
  for(size_t n = 0; n < 8192; n++)
  {
    if(0 == n)
    {
      BOOST_CHECK(v[n].v == 78);
    }
    else if(n < 512)
    {
      BOOST_CHECK(v[n].v == 79);
    }
    else if(n < 2048)
    {
      BOOST_CHECK(v[n].v == 80);
    }
    else if(n < 8192)
    {
      BOOST_CHECK(v[n].v == 81);
    }
  }
  auto it = v.begin();
  for(size_t n = 0; n < 8192; n++, ++it)
  {
    if(0 == n)
    {
      BOOST_CHECK(it->v == 78);
    }
    else if(n < 512)
    {
      BOOST_CHECK(it->v == 79);
    }
    else if(n < 2048)
    {
      BOOST_CHECK(it->v == 80);
    }
    else if(n < 8192)
    {
      BOOST_CHECK(it->v == 81);
    }
  }
  BOOST_CHECK(it == v.end());
}

template <class T> unsigned long long BenchmarkVector(T &v, size_t no)
{
  auto begin = std::chrono::high_resolution_clock::now();
  for(size_t n = 0; n < no; n++)
  {
    v.push_back(5);
  }
  auto end = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
}

static inline void BenchmarkTrivialVector1()
{
  for(size_t items = 512; items < 16 * 1024 * 1024; items *= 2)
  {
    auto begin = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 1)
    {
    }
    std::vector<int> v1;
    v1.reserve(1);
    auto std_time = BenchmarkVector(v1, items);
    AFIO_V2_NAMESPACE::algorithm::trivial_vector<int> v2;
    v2.reserve(2);
    auto afio_time = BenchmarkVector(v2, items);
    std::cout << "                    std::vector<int> inserts " << items << " items in " << std_time << " microseconds" << std::endl;
    std::cout << "afio::algorithm::trivial_vector<int> inserts " << items << " items in " << afio_time << " microseconds" << std::endl;
  }
}

static inline void BenchmarkTrivialVector2()
{
  for(size_t items = 1024; items < 16 * 1024 * 1024; items *= 2)
  {
    std::chrono::high_resolution_clock::time_point begin = std::chrono::high_resolution_clock::now(), end;
    {
      std::vector<int> v1;
      v1.resize(items, 5);
      while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 1)
      {
      }
      begin = std::chrono::high_resolution_clock::now();
      v1.resize(items * 2, 6);
      end = std::chrono::high_resolution_clock::now();
    }
    auto std_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    {
      AFIO_V2_NAMESPACE::algorithm::trivial_vector<int> v1;
      v1.resize(items, 5);
      while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 1)
      {
      }
      begin = std::chrono::high_resolution_clock::now();
      v1.resize(items * 2, 6);
      end = std::chrono::high_resolution_clock::now();
    }
    auto afio_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();
    std::cout << "                    std::vector<int> resizes " << items << " items to " << (items * 2) << " items in " << std_time << " microseconds" << std::endl;
    std::cout << "afio::algorithm::trivial_vector<int> resizes " << items << " items to " << (items * 2) << " items in " << afio_time << " microseconds" << std::endl;
  }
}

KERNELTEST_TEST_KERNEL(integration, afio, algorithm, trivial_vector, "Tests that afio::algorithm::trivial_vector works as expected", TestTrivialVector())
KERNELTEST_TEST_KERNEL(integration, afio, algorithm, trivial_vector2, "Benchmarks afio::algorithm::trivial_vector against std::vector 1", BenchmarkTrivialVector1())
KERNELTEST_TEST_KERNEL(integration, afio, algorithm, trivial_vector3, "Benchmarks afio::algorithm::trivial_vector against std::vector 2", BenchmarkTrivialVector2())
