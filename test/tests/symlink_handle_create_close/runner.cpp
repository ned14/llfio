/* Integration test kernel for symlink_handle create and close
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (22 commits)
File Created: Jul 2018


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

#include "kernel_symlink_handle.cpp.hpp"

template <class U> inline void symlink_handle_create_close_creation(U &&f)
{
  using namespace KERNELTEST_V1_NAMESPACE;
  using LLFIO_V2_NAMESPACE::result;
  using symlink_handle = LLFIO_V2_NAMESPACE::symlink_handle;
  static const result<void> no_such_file_or_directory = LLFIO_V2_NAMESPACE::errc::no_such_file_or_directory;
  static const result<void> file_exists = LLFIO_V2_NAMESPACE::errc::file_exists;
  static const result<void> function_not_supported = LLFIO_V2_NAMESPACE::errc::function_not_supported;
  static const result<void> permission_denied = LLFIO_V2_NAMESPACE::errc::permission_denied;

  // clang-format off
  static const auto permuter(mt_permute_parameters<
    result<void>,                                  
    parameters<                                    
      typename symlink_handle::mode,
      typename symlink_handle::creation,
      typename symlink_handle::flag
    >,
    precondition::filesystem_setup_parameters,
    postcondition::filesystem_comparison_structure_parameters
  >(
    { 

      // Does the mode parameter have the expected side effects?
      {                 success(), { symlink_handle::mode::none,       symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::attr_read,  symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::attr_write, symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::write,      symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing0" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::write,      symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {    function_not_supported, { symlink_handle::mode::append,     symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::none,       symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing0" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::attr_read,  symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing0" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::attr_write, symlink_handle::creation::if_needed,     symlink_handle::flag::none }, { "existing0" }, { "existing1" }},

      // Does the creation parameter have the expected side effects?
      { no_such_file_or_directory, { symlink_handle::mode::write, symlink_handle::creation::open_existing    , symlink_handle::flag::none }, { "existing0" }, { "existing0" }},
      {                 success(), { symlink_handle::mode::write, symlink_handle::creation::open_existing    , symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::write, symlink_handle::creation::only_if_not_exist, symlink_handle::flag::none }, { "existing0" }, { "existing1" }},
      {               file_exists, { symlink_handle::mode::write, symlink_handle::creation::only_if_not_exist, symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::write, symlink_handle::creation::if_needed        , symlink_handle::flag::none }, { "existing0" }, { "existing1" }},
      {                 success(), { symlink_handle::mode::write, symlink_handle::creation::if_needed        , symlink_handle::flag::none }, { "existing1" }, { "existing1" }},
      {    function_not_supported, { symlink_handle::mode::write, symlink_handle::creation::truncate         , symlink_handle::flag::none }, { "existing1" }, { "existing1" }},

      // Does the flag parameter have the expected side effects?
      {                 success(), { symlink_handle::mode::write, symlink_handle::creation::open_existing, symlink_handle::flag::unlink_on_first_close }, { "existing1" }, { "existing0" }}
    },
    precondition::filesystem_setup(),              
    postcondition::filesystem_comparison_structure() 
  ));
  // clang-format on

  auto results = permuter(std::forward<U>(f));
  check_results_with_boost_test(permuter, results);
}

KERNELTEST_TEST_KERNEL(unit, llfio, symlink_handle_create_close, symlink_handle, "Tests that llfio::symlink_handle::symlink()'s parameters with absolute paths work as expected", symlink_handle_create_close_creation(symlink_handle_create_close::test_kernel_symlink_handle_absolute))
KERNELTEST_TEST_KERNEL(unit, llfio, symlink_handle_create_close, symlink_handle, "Tests that llfio::symlink_handle::symlink()'s parameters with relative paths work as expected", symlink_handle_create_close_creation(symlink_handle_create_close::test_kernel_symlink_handle_relative))
