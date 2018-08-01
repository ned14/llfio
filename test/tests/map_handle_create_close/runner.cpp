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

#include "kernel_map_handle.cpp.hpp"

template <class U> inline void map_handle_create_close_(U &&f)
{
  using namespace KERNELTEST_V1_NAMESPACE;
  using LLFIO_V2_NAMESPACE::result;
  using LLFIO_V2_NAMESPACE::file_handle;
  using LLFIO_V2_NAMESPACE::section_handle;
  using LLFIO_V2_NAMESPACE::map_handle;

  // Create a temporary file and put some text into it
  file_handle temph;
  auto boundf = [&](auto... pars) { return f(temph, pars...); };

  // clang-format off
  static const auto permuter(st_permute_parameters<
    result<void>,                               
    parameters<                              
      typename map_handle::size_type,
      typename section_handle::flag
    >,
    precondition::filesystem_setup_parameters,
    postcondition::custom_parameters<bool>
  >(
    {
      { success(),{ 4096, section_handle::flag::none },{ "_" },{ false } },
      { success(),{ 4096, section_handle::flag::read },{ "_" },{ false } },
      { success(),{ 4096, section_handle::flag::write },{ "_" },{ false } },
      { success(),{ 4096, section_handle::flag::cow },{ "_" },{ false } },
      //{ success(),{ 4096, section_handle::flag::execute },{ "_" },{ false } },
      { success(),{ 4096, section_handle::flag::write|section_handle::flag::nocommit },{ "_" },{ false } },
      { success(),{ 4096, section_handle::flag::write|section_handle::flag::prefault },{ "_" },{ false } },
      //{ success(),{ 4096, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ false } },

      { success(),{ 0, section_handle::flag::none },{ "_" },{ true } },
      { success(),{ 0, section_handle::flag::read },{ "_" },{ true } },
      { success(),{ 0, section_handle::flag::write },{ "_" },{ true } },
      { success(),{ 0, section_handle::flag::cow },{ "_" },{ true } },
      //{ success(),{ 0, section_handle::flag::execute },{ "_" },{ true } },
      { success(),{ 0, section_handle::flag::write | section_handle::flag::nocommit },{ "_" },{ true } },
      { success(),{ 0, section_handle::flag::write | section_handle::flag::prefault },{ "_" },{ true } },
      //{ success(),{ 0, section_handle::flag::write|section_handle::flag::executable },{ "_" },{ true } },
  },
    precondition::filesystem_setup(),
    postcondition::custom(
      [&](auto &permuter, auto &testreturn, size_t idx, int use_file_backing) {
        if (use_file_backing)
        {
          // Create a temporary backing file
          temph = file_handle::file({}, "tempfile", file_handle::mode::write, file_handle::creation::if_needed).value();
          temph.write(0, { {reinterpret_cast<const LLFIO_V2_NAMESPACE::byte *>("I am some file data"), 19} }).value();
        }
        else
        {
          // Use the page file
          temph = file_handle();
	      }
        return std::make_tuple(std::ref(permuter), std::ref(testreturn), idx, use_file_backing);
      },
      [&](auto tuplestate) {
        auto &permuter = std::get<0>(tuplestate);
        auto &testreturn = *std::get<1>(tuplestate);
        size_t idx = std::get<2>(tuplestate);
        int use_file_backing = std::get<3>(tuplestate);
        map_handle maph;
        if (testreturn)
        {
          maph = std::move(testreturn.value());
        }
        // Need to close the map and any backing file as otherwise filesystem_setup won't be able to clear up the working dir on Windows
        auto onexit = LLFIO_V2_NAMESPACE::undoer([&]{
          (void) maph.close();
          (void) temph.close();
        });
//#undef KERNELTEST_CHECK
//#define KERNELTEST_CHECK(a, ...) BOOST_CHECK(__VA_ARGS__)
        if (testreturn)
        {
          LLFIO_V2_NAMESPACE::byte *addr = maph.address();
          section_handle::flag flags = std::get<1>(std::get<1>(permuter[idx]));
          KERNELTEST_CHECK(testreturn, maph.length() > 0);
          KERNELTEST_CHECK(testreturn, addr != nullptr);
          if (!(flags & section_handle::flag::nocommit) && addr != nullptr)
          {
            LLFIO_V2_NAMESPACE::byte buffer[64];
            // Make sure the backing file is appearing in the map
            if (use_file_backing && maph.is_readable())
            {
              KERNELTEST_CHECK(testreturn, !memcmp(addr, "I am some file data", 19));
            }
            // Make sure maph's read() does what it is supposed to
            if (use_file_backing)
            {
              auto b = maph.read(0, { {nullptr, 20} }).value();
              KERNELTEST_CHECK(testreturn, b[0].data() == addr);
              KERNELTEST_CHECK(testreturn, b[0].size() == 19);  // reads do not read more than the backing length
            }
            else
            {
              auto b = maph.read(5, { {nullptr, 5000} }).value();
              KERNELTEST_CHECK(testreturn, b[0].data() == addr+5); // NOLINT
              KERNELTEST_CHECK(testreturn, b[0].size() == 4091);
            }
            // If we are writable, write straight into the map
            if (maph.is_writable() && addr)
            {
              memcpy(addr, "NIALL was here", 14);
              // Make sure maph's write() works
              maph.write(1, { {reinterpret_cast<const LLFIO_V2_NAMESPACE::byte *>("iall"), 4} });
              // Make sure data written to the map turns up in the file
              if (use_file_backing)
              {
                temph.read(0, { {buffer, 64} });  // NOLINT
                if (flags & section_handle::flag::cow)
                {
                  KERNELTEST_CHECK(testreturn, !memcmp(buffer, "I am some file data", 19));  // NOLINT
                }
                else
                {
                  KERNELTEST_CHECK(testreturn, !memcmp(buffer, "Niall was here data", 19));  // NOLINT
                }
              }
            }
            // The OS should not auto expand storage to 4Kb
            if (use_file_backing)
            {
              KERNELTEST_CHECK(testreturn, temph.maximum_extent().value() == 19);
            }
          }
        }
      },
    "check mmap")
  ));
  // clang-format on

  auto results = permuter(boundf);
  check_results_with_boost_test(permuter, results);
}

KERNELTEST_TEST_KERNEL(unit, llfio, map_handle_create_close, map_handle, "Tests that llfio::map_handle's creation parameters work as expected", map_handle_create_close_(map_handle_create_close::test_kernel_map_handle))
