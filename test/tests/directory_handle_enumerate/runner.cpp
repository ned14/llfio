/* Integration test kernel for map_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
File Created: Aug 2016


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

#include "kernel_directory_handle_enumerate.cpp.hpp"

inline LLFIO_V2_NAMESPACE::span<LLFIO_V2_NAMESPACE::directory_entry> static_directory_entries()
{
  using LLFIO_V2_NAMESPACE::directory_entry;
  static directory_entry _entries[5];
  for(auto &i : _entries)
  {
    i = directory_entry();
  }
  return {_entries};
}

template <class U> inline void directory_handle_enumerate_(U &&f)
{
  using namespace KERNELTEST_V1_NAMESPACE;
  using LLFIO_V2_NAMESPACE::directory_entry;
  using LLFIO_V2_NAMESPACE::directory_handle;
  using LLFIO_V2_NAMESPACE::path_view;
  using LLFIO_V2_NAMESPACE::result;
  using filter = LLFIO_V2_NAMESPACE::directory_handle::filter;

  auto entries = static_directory_entries();

  // clang-format off
  static const auto permuter(mt_permute_parameters<
    il_result<void>,
    parameters<                              
      LLFIO_V2_NAMESPACE::span<directory_entry> *,
      path_view,
      filter
    >,
    precondition::filesystem_setup_parameters,
    postcondition::custom_parameters<>
  >(
    {
      { success(),{ &entries, {}, filter::none },{ "existing" },{ } },
#ifdef _WIN32
      { success(),{ &entries, {}, filter::fastdeleted },{ "existing" },{ } },
#endif
      { success(),{ &entries,"foo*", filter::fastdeleted },{ "existing" },{ } }
  },
    precondition::filesystem_setup(),
    postcondition::custom(
      [&](auto &permuter, auto &testreturn, size_t idx) {
        // reset
        static_directory_entries();
        return std::make_tuple(std::ref(permuter), std::ref(testreturn), idx);
      },
      [&](auto tuplestate) {
        auto &permuter = std::get<0>(tuplestate);
        auto &testreturn = *std::get<1>(tuplestate);
        size_t idx = std::get<2>(tuplestate);

        if (testreturn)
        {
          path_view glob = std::get<1>(std::get<1>(permuter[idx]));
          filter filtered = std::get<2>(std::get<1>(permuter[idx]));
          directory_handle::buffers_type info(std::move(testreturn.value()));
          KERNELTEST_CHECK(testreturn, info.done() == true);
          if (!glob.empty())
          {
            KERNELTEST_CHECK(testreturn, info.size() == 1);
          }
          else if (filtered == filter::fastdeleted)
          {
            KERNELTEST_CHECK(testreturn, info.size() == 3);
          }
          else
          {
            KERNELTEST_CHECK(testreturn, info.size() == 4);
          }
          bool havedeleted1 = !glob.empty();
          bool havedeleted2 = (filtered == filter::fastdeleted);
          bool havefoo = false;
          bool havedir = !glob.empty();
          for (directory_entry &i : info)
          {
            if (i.leafname == "012345678901234567890123456789012345678901234567890123456789012z.deleted")
            {
              havedeleted1 = true;
            }
            if (i.leafname == "0123456789012345678901234567890123456789012345678901234567890123.deleted")
            {
              havedeleted2 = true;
            }
            if (i.leafname == "foo.txt")
            {
              havefoo = true;
            }
            if (i.leafname == "dir")
            {
              havedir = true;
            }
          }
          KERNELTEST_CHECK(testreturn, havedeleted1 == true);
          KERNELTEST_CHECK(testreturn, havedeleted2 == true);
          KERNELTEST_CHECK(testreturn, havefoo == true);
          KERNELTEST_CHECK(testreturn, havedir == true);
        }
      },
    "check enumeration")
  ));
  // clang-format on

  auto results = permuter(std::forward<U>(f));
  check_results_with_boost_test(permuter, results);
}

KERNELTEST_TEST_KERNEL(unit, llfio, directory_handle_enumerate, directory_handle, "Tests that llfio::directory_handle::enumerate's parameters work as expected", directory_handle_enumerate_(directory_handle_enumerate::test_kernel_directory_handle_enumerate))
