/* Integration test kernel for whether current_process_memory_usage() works
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

#include "../test_kernel_decl.hpp"

static inline void TestCurrentProcessMemoryUsage()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  static const llfio::utils::process_memory_usage *last_pmu;
  auto print = [](llfio::utils::process_memory_usage &pmu) -> std::ostream &(*) (std::ostream &) {
    pmu = llfio::utils::current_process_memory_usage().value();
    last_pmu = &pmu;
    return [](std::ostream &s) -> std::ostream & {
      return s << "      " << (last_pmu->total_address_space_in_use / 1024.0 / 1024.0) << "," << (last_pmu->total_address_space_paged_in / 1024.0 / 1024.0) << "," << (last_pmu->private_committed / 1024.0 / 1024.0) << "," << (last_pmu->private_paged_in / 1024.0 / 1024.0) << std::endl;
    };
  };
  {
    llfio::utils::process_memory_usage pmu;
    std::cout << "   Really before anything:\n" << print(pmu) << std::endl;
  }
  {
    auto maph = llfio::map_handle::map(1024 * 1024 * 1024).value();
    auto mapfileh = llfio::mapped_file_handle::mapped_temp_inode(1024 * 1024 * 1024).value();
  }  // namespace llfio=LLFIO_V2_NAMESPACE;
  std::cout << "For page allocation:\n";
  {
    llfio::utils::process_memory_usage before_anything, after_reserve, after_commit, after_fault, after_decommit, after_zero, after_do_not_store;
    std::cout << "   Total address space, Total address space paged in, Private bytes committed, Private bytes paged in\n\n";
    std::cout << "   Before anything:\n" << print(before_anything) << std::endl;
    {
      // Should raise total_address_space_in_use by 1Gb
      auto maph = llfio::map_handle::map(1024 * 1024 * 1024, false, llfio::section_handle::flag::nocommit).value();
      std::cout << "   After reserving 1Gb:\n" << print(after_reserve) << std::endl;
    }
    {
      // Should raise total_address_space_in_use and private_committed by 1Gb
      auto maph = llfio::map_handle::map(1024 * 1024 * 1024).value();
      std::cout << "   After committing 1Gb:\n" << print(after_commit) << std::endl;
    }
    {
      // Should raise total_address_space_in_use, private_committed and private_paged_in by 1Gb
      auto maph = llfio::map_handle::map(1024 * 1024 * 1024, false, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      std::cout << "   After committing and faulting 1Gb:\n" << print(after_fault) << std::endl;
    }
    {
      // Should raise total_address_space_in_use by 1Gb
      auto maph = llfio::map_handle::map(1024 * 1024 * 1024, false, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      maph.decommit({maph.address(), maph.length()}).value();
      std::cout << "   After committing and faulting and decommitting 1Gb:\n" << print(after_decommit) << std::endl;
    }
    {
      // Should raise total_address_space_in_use, private_committed by 1Gb
      auto maph = llfio::map_handle::map(1024 * 1024 * 1024, false, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      maph.zero_memory({maph.address(), maph.length()}).value();
      std::cout << "   After committing and faulting and zeroing 1Gb:\n" << print(after_zero) << std::endl;
    }
    {
      // Should raise total_address_space_in_use, private_committed by 1Gb
      auto maph = llfio::map_handle::map(1024 * 1024 * 1024, false, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      maph.do_not_store({maph.address(), maph.length()}).value();
      std::cout << "   After committing and faulting and do not storing 1Gb:\n" << print(after_do_not_store) << std::endl;
    }
    auto within = [](const llfio::utils::process_memory_usage &a, const llfio::utils::process_memory_usage &b, int total_address_space_in_use, int total_address_space_paged_in, int private_committed, int private_paged_in) {
      auto check = [](size_t a, size_t b, int bound) {
        auto diff = abs((b / 1024.0 / 1024.0) - (a / 1024.0 / 1024.0));
        return diff > bound - 10 && diff < bound + 10;
      };
      return check(a.total_address_space_in_use, b.total_address_space_in_use, total_address_space_in_use)           //
             && check(a.total_address_space_paged_in, b.total_address_space_paged_in, total_address_space_paged_in)  //
             && check(a.private_committed, b.private_committed, private_committed)                                   //
             && check(a.private_paged_in, b.private_paged_in, private_paged_in);
    };
    BOOST_CHECK(within(before_anything, after_reserve, 1024, 0, 0, 0));
    BOOST_CHECK(within(before_anything, after_commit, 1024, 0, 1024, 0));
    BOOST_CHECK(within(before_anything, after_fault, 1024, 1024, 1024, 1024));
    BOOST_CHECK(within(before_anything, after_decommit, 1024, 0, 0, 0));
#ifdef _WIN32
    BOOST_CHECK(within(before_anything, after_zero, 1024, 0, 1024, 0));  // may not evict faulted set on POSIX
#else
    (void) after_zero;
#endif
    BOOST_CHECK(within(before_anything, after_do_not_store, 1024, 0, 1024, 0));
  }
  std::cout << "\nFor file mapping:\n";
  {
    auto sectionh = llfio::section_handle::section(1024 * 1024 * 1024).value();
    llfio::utils::process_memory_usage before_anything, after_reserve, after_commit, after_fault, after_decommit, after_zero, after_do_not_store;
    std::cout << "   Total address space, Total address space paged in, Private bytes committed, Private bytes paged in\n\n";
    std::cout << "   Before anything:\n" << print(before_anything) << std::endl;

    {
      // Should raise total_address_space_in_use by 1Gb
      auto mapfileh = llfio::map_handle::map(sectionh, 1024 * 1024 * 1024, 0, llfio::section_handle::flag::nocommit).value();
      std::cout << "   After reserving 1Gb:\n" << print(after_reserve) << std::endl;
    }
    {
      // Should raise total_address_space_in_use by 1Gb, but not private_committed
      auto mapfileh = llfio::map_handle::map(sectionh, 1024 * 1024 * 1024, 0).value();
      std::cout << "   After committing 1Gb:\n" << print(after_commit) << std::endl;
    }
    {
      // Should raise total_address_space_in_use and total_address_space_paged_in by 1Gb
      auto mapfileh = llfio::map_handle::map(sectionh, 1024 * 1024 * 1024, 0, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      std::cout << "   After committing and faulting 1Gb:\n" << print(after_fault) << std::endl;
    }
#ifndef _WIN32
    {
      // Should raise total_address_space_in_use by 1Gb, but not private_committed
      auto mapfileh = llfio::map_handle::map(sectionh, 1024 * 1024 * 1024, 0, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      mapfileh.decommit({mapfileh.address(), mapfileh.length()}).value();
      std::cout << "   After committing and faulting and decommitting 1Gb:\n" << print(after_decommit) << std::endl;
    }
    {
      // Should raise total_address_space_in_use by 1Gb, but not private_committed
      auto mapfileh = llfio::map_handle::map(sectionh, 1024 * 1024 * 1024, 0, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      mapfileh.zero_memory({mapfileh.address(), mapfileh.length()}).value();
      std::cout << "   After committing and faulting and zeroing 1Gb:\n" << print(after_zero) << std::endl;
    }
    {
      // Should raise total_address_space_in_use by 1Gb, but not private_committed
      auto mapfileh = llfio::map_handle::map(sectionh, 1024 * 1024 * 1024, 0, llfio::section_handle::flag::readwrite | llfio::section_handle::flag::prefault).value();
      mapfileh.do_not_store({mapfileh.address(), mapfileh.length()}).value();
      std::cout << "   After committing and faulting and do not storing 1Gb:\n" << print(after_do_not_store) << std::endl;
    }
#endif
    auto within = [](const llfio::utils::process_memory_usage &a, const llfio::utils::process_memory_usage &b, int total_address_space_in_use, int total_address_space_paged_in, int private_committed, int private_paged_in) {
      auto check = [](size_t a, size_t b, int bound) {
        auto diff = abs((b / 1024.0 / 1024.0) - (a / 1024.0 / 1024.0));
        return diff > bound - 10 && diff < bound + 10;
      };
      return check(a.total_address_space_in_use, b.total_address_space_in_use, total_address_space_in_use)           //
             && check(a.total_address_space_paged_in, b.total_address_space_paged_in, total_address_space_paged_in)  //
             && check(a.private_committed, b.private_committed, private_committed)                                   //
             && check(a.private_paged_in, b.private_paged_in, private_paged_in);
    };
    BOOST_CHECK(within(before_anything, after_reserve, 1024, 0, 0, 0));
    BOOST_CHECK(within(before_anything, after_commit, 1024, 0, 0, 0));
    BOOST_CHECK(within(before_anything, after_fault, 1024, 1024, 0, 0));
#ifndef _WIN32
    BOOST_CHECK(within(before_anything, after_decommit, 1024, 0, 0, 0));
    BOOST_CHECK(within(before_anything, after_zero, 1024, 0, 0, 0));
    BOOST_CHECK(within(before_anything, after_do_not_store, 1024, 0, 0, 0));
#else
    (void) after_decommit;
    (void) after_zero;
    (void) after_do_not_store;
#endif
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, utils, current_process_memory_usage, "Tests that llfio::utils::current_process_memory_usage() works as expected", TestCurrentProcessMemoryUsage())
