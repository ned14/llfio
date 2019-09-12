/* Integration test kernel for directory_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (22 commits)
File Created: Apr 2016


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

#include "kernel_directory_handle.cpp.hpp"

inline LLFIO_V2_NAMESPACE::directory_handle::buffers_type static_buffers()
{
  using directory_handle = LLFIO_V2_NAMESPACE::directory_handle;
  static typename directory_handle::buffer_type _entries[5];
  // Reset enumeration entries
  for(auto &i : _entries)
  {
    i = typename directory_handle::buffer_type();
  }
  return {_entries};
}

inline LLFIO_V2_NAMESPACE::result<typename LLFIO_V2_NAMESPACE::directory_handle::buffers_type> &static_info()
{
  using LLFIO_V2_NAMESPACE::result;
  using directory_handle = LLFIO_V2_NAMESPACE::directory_handle;
  static result<typename directory_handle::buffers_type> info(typename result<typename directory_handle::buffers_type>::error_type{});
  info = {typename result<typename directory_handle::buffers_type>::error_type{}};
  return info;
}


template <class U> inline void directory_handle_create_close_creation(U &&f)
{
  using namespace KERNELTEST_V1_NAMESPACE;
  using LLFIO_V2_NAMESPACE::result;
  using directory_handle = LLFIO_V2_NAMESPACE::directory_handle;
  static const il_result<void> no_such_file_or_directory = LLFIO_V2_NAMESPACE::errc::no_such_file_or_directory;
  static const il_result<void> file_exists = LLFIO_V2_NAMESPACE::errc::file_exists;
  static const il_result<void> is_a_directory = LLFIO_V2_NAMESPACE::errc::is_a_directory;
  static const il_result<void> permission_denied = LLFIO_V2_NAMESPACE::errc::permission_denied;

  // clang-format off
  auto entries = static_buffers();
  auto &info = static_info();
  static const auto permuter(mt_permute_parameters< 
    il_result<void>,                              
    parameters<                                
      typename directory_handle::mode,
      typename directory_handle::creation,
      typename directory_handle::flag,
      typename directory_handle::buffers_type *,
      result<typename directory_handle::buffers_type> *
    >,
    precondition::filesystem_setup_parameters,
    postcondition::filesystem_comparison_structure_parameters,
    postcondition::custom_parameters<il_result<void>>
  >(
    { 

      // Does the mode parameter have the expected side effects?
      {                 success(), { directory_handle::mode::none,       directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ permission_denied } },
      {                 success(), { directory_handle::mode::attr_read,  directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } },
      {                 success(), { directory_handle::mode::attr_write, directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } },
      {                 success(), { directory_handle::mode::write,      directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ success() } },
      {                 success(), { directory_handle::mode::write,      directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } },
      {                 success(), { directory_handle::mode::append,     directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ success() } },
      {                 success(), { directory_handle::mode::append,     directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } },
      {                 success(), { directory_handle::mode::none,       directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ permission_denied } },
      {                 success(), { directory_handle::mode::attr_read,  directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ success() } },
      {                 success(), { directory_handle::mode::attr_write, directory_handle::creation::if_needed, directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ success() } },

      // Does the creation parameter have the expected side effects?
      { no_such_file_or_directory, { directory_handle::mode::write, directory_handle::creation::open_existing    , directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "non-existing" },{ success() } },
      {                 success(), { directory_handle::mode::write, directory_handle::creation::open_existing    , directory_handle::flag::none, &entries, &info }, { "existing0"    }, { "existing0"    },{ success() } },
      {                 success(), { directory_handle::mode::write, directory_handle::creation::open_existing    , directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } },
      {                 success(), { directory_handle::mode::write, directory_handle::creation::only_if_not_exist, directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ success() } },
      {               file_exists, { directory_handle::mode::write, directory_handle::creation::only_if_not_exist, directory_handle::flag::none, &entries, &info }, { "existing0"    }, { "existing0"    },{ success() } },
      {                 success(), { directory_handle::mode::write, directory_handle::creation::if_needed        , directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "existing0"    },{ success() } },
      {                 success(), { directory_handle::mode::write, directory_handle::creation::if_needed        , directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } },
      {            is_a_directory, { directory_handle::mode::write, directory_handle::creation::truncate         , directory_handle::flag::none, &entries, &info }, { "non-existing" }, { "non-existing" },{ success() } },
      {            is_a_directory, { directory_handle::mode::write, directory_handle::creation::truncate         , directory_handle::flag::none, &entries, &info }, { "existing0"    }, { "existing0"    },{ success() } },
      {            is_a_directory, { directory_handle::mode::write, directory_handle::creation::truncate         , directory_handle::flag::none, &entries, &info }, { "existing1"    }, { "existing1"    },{ success() } }
    },
    precondition::filesystem_setup(),                
    postcondition::filesystem_comparison_structure(),
    postcondition::custom(
      [&](auto &permuter, auto &testreturn, size_t idx, auto &enumeration_should_be) {
        // reset
        static_buffers();
        static_info();
        return std::make_tuple(std::ref(permuter), std::ref(testreturn), idx, std::ref(enumeration_should_be));
      },
      [&](auto /*tuplestate*/) {
#if 0
        auto &permuter = std::get<0>(tuplestate);
        auto &testreturn = std::get<1>(tuplestate);
        size_t idx = std::get<2>(tuplestate);
        auto &enumeration_should_be = std::get<3>(tuplestate);

        // Check enumerate worked
        if (testreturn && *testreturn)
        {
          if (info)
          {
            bool foundfile = false, foundpin=false, foundother = false;
            for (auto &i : info.value().filled)
            {
              if (i.leafname == "testfile.txt")
              {
                foundfile = true;
              }
              else if (i.leafname == "pin.txt")
              {
                foundpin = true;
              }
              else
              {
                foundother = true;
              }
            }
            const char *fs_after = std::get<0>(std::get<3>(permuter[idx]));
            if (!strcmp(fs_after, "existing0"))
            {
              KERNELTEST_CHECK(testreturn, foundfile == false);
              KERNELTEST_CHECK(testreturn, foundpin == true);
              KERNELTEST_CHECK(testreturn, foundother == false);
            }
            else if (!strcmp(fs_after, "existing1"))
            {
              KERNELTEST_CHECK(testreturn, foundfile == true);
              KERNELTEST_CHECK(testreturn, foundpin == true);
              KERNELTEST_CHECK(testreturn, foundother == false);
            }
          }
          else
          {
            if (!enumeration_should_be)
            {
              KERNELTEST_CHECK(testreturn, info.error().default_error_condition() == enumeration_should_be.error());
            }
            else
            {
              KERNELTEST_CHECK(testreturn, info.error() == std::error_code());
            }
          }
        }
#endif
      },
    "check enumeration")
  ));
  // clang-format on

  auto results = permuter(std::forward<U>(f));
  check_results_with_boost_test(permuter, results);
}

KERNELTEST_TEST_KERNEL(unit, llfio, directory_handle_create_close, directory_handle, "Tests that llfio::directory_handle::directory()'s parameters with absolute paths work as expected", directory_handle_create_close_creation(directory_handle_create_close::test_kernel_directory_handle_absolute))
KERNELTEST_TEST_KERNEL(unit, llfio, directory_handle_create_close, directory_handle, "Tests that llfio::directory_handle::directory()'s parameters with relative paths work as expected", directory_handle_create_close_creation(directory_handle_create_close::test_kernel_directory_handle_relative))
