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

static size_t trivial_vector_udts_constructed;
static inline void TestTrivialVector()
{
  // Test udt without default constructor, but complex destructor and constructor
  struct udt
  {
    size_t v;
    udt() = delete;
    explicit udt(int) { v = trivial_vector_udts_constructed++; };
  };
  using udt_vector = AFIO_V2_NAMESPACE::algorithm::trivial_vector<udt>;

  udt_vector v;
  BOOST_CHECK(v.empty());
  BOOST_CHECK(v.size() == 0);
  v.push_back(udt(5));
  BOOST_CHECK(v.size() == 1);
  BOOST_CHECK(v.capacity() == AFIO_V2_NAMESPACE::utils::page_size() / sizeof(udt));
}

KERNELTEST_TEST_KERNEL(integration, afio, algorithm, trivial_vector, "Tests that afio::algorithm::trivial_vector works as expected", TestTrivialVector())
