/* Integration test kernel for traversal_summary accounting
(C) 2026 Niall Douglas <http://www.nedproductions.biz/> (1 commit)
File Created: Nov 2026


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

static inline void TestTraversalSummaryCounters()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  using llfio::algorithm::traversal_summary;
  traversal_summary a, b;
  a.stats_failed = 2;
  a.directory_opens_failed = 1;
  b.stats_failed = 3;
  b.directory_opens_failed = 4;
  // summarize() assembles its result by adding per-thread accumulators into one
  // summary, so a counter which operator+= does not carry is always reported as
  // zero. That is exactly what stats_failed used to do, whatever the traversal
  // actually saw.
  traversal_summary sum;
  sum += a;
  sum += b;
  BOOST_CHECK(sum.stats_failed == 5);
  BOOST_CHECK(sum.directory_opens_failed == 5);
  traversal_summary copied(a);
  BOOST_CHECK(copied.stats_failed == 2);
  traversal_summary moved(std::move(copied));
  BOOST_CHECK(moved.stats_failed == 2);
  traversal_summary assigned;
  assigned = b;
  BOOST_CHECK(assigned.stats_failed == 3);
  traversal_summary moveassigned;
  moveassigned = std::move(assigned);
  BOOST_CHECK(moveassigned.stats_failed == 3);
}

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, summarize, "Tests that llfio::algorithm::traversal_summary accumulates its failure counters",
                       TestTraversalSummaryCounters())
