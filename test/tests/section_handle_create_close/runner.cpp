/* Integration test kernel for section_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (7 commits)
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

#include "../kerneltest/include/kerneltest.hpp"
#include "kernel_section_handle.cpp.hpp"

template <class U> inline void section_handle_create_close_(U &&f)
{
  using namespace KERNELTEST_V1_NAMESPACE;
  using namespace AFIO_V2_NAMESPACE;
  using AFIO_V2_NAMESPACE::file_handle;

  // Create a temporary file and put some text into it
  file_handle temph;
  auto boundf = [&](auto... pars) { return f(temph, pars...); };

  // clang-format off
  static const auto permuter(st_permute_parameters<
    result<void>,                               
    parameters<                              
      typename section_handle::extent_type,
      typename section_handle::flag
    >,
    precondition::filesystem_setup_parameters,
    postcondition::custom_parameters<bool>
  >(
    {
      { success(),{ 1, section_handle::flag::none },{ "_" },{ false } },
      { success(),{ 1, section_handle::flag::read },{ "_" },{ false } },
      { success(),{ 1, section_handle::flag::write },{ "_" },{ false } },
      { success(),{ 1, section_handle::flag::cow },{ "_" },{ false } },
      { success(),{ 1, section_handle::flag::execute },{ "_" },{ false } },
      { success(),{ 1, section_handle::flag::write|section_handle::flag::nocommit },{ "_" },{ false } },
      { success(),{ 1, section_handle::flag::write|section_handle::flag::prefault },{ "_" },{ false } },
      //{ success(),{ 1, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ false } },

      { success(),{ 1, section_handle::flag::none },{ "_" },{ true } },
      { success(),{ 1, section_handle::flag::read },{ "_" },{ true } },
      { success(),{ 1, section_handle::flag::write },{ "_" },{ true } },
      { success(),{ 1, section_handle::flag::cow },{ "_" },{ true } },
      //{ success(),{ 1, section_handle::flag::execute },{ "_" },{ true } },
      { success(),{ 1, section_handle::flag::write | section_handle::flag::nocommit },{ "_" },{ true } },
      { success(),{ 1, section_handle::flag::write | section_handle::flag::prefault },{ "_" },{ true } },
      //{ success(),{ 1, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ true } },
  },
    precondition::filesystem_setup(),
    postcondition::custom(
      [&](auto &, auto &testreturn, size_t, int use_file_backing) {
        if (use_file_backing)
        {
          temph = file_handle::file({}, "tempfile", file_handle::mode::write, file_handle::creation::if_needed).value();
          temph.write(0, "niall is not here", 17).value();
        }
        else
          temph = file_handle();
        return &testreturn;
      },
      [&](auto *testreturn) {
        // Need to close the section and any backing file as otherwise filesystem_setup won't be able to clear up the working dir
        if (*testreturn)
          testreturn->value().close();
        temph.close();
      },
    "check section")
  ));
  // clang-format on

  auto results = permuter(boundf);
  check_results_with_boost_test(permuter, results);
}

KERNELTEST_TEST_KERNEL(unit, afio, section_handle_create_close, section_handle, "Tests that afio::section_handle's creation parameters work as expected", section_handle_create_close_(section_handle_create_close::test_kernel_section_handle))
