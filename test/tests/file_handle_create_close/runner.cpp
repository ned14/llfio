/* Integration test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Apr 2016
*/

#include "../kerneltest/include/boost/kerneltest.hpp"
#include "kernel_async_file_handle.cpp.hpp"
#include "kernel_file_handle.cpp.hpp"

template <class U> inline void file_handle_create_close_creation(U &&f)
{
  using namespace BOOST_KERNELTEST_V1_NAMESPACE;
  using file_handle = BOOST_AFIO_V2_NAMESPACE::file_handle;

  /* Set up a permuter which for every one of these parameter values listed,
  tests with the value using the input workspace which should produce outcome
  workspace. Workspaces are:
    * non-existing: no files
    *    existing0: single zero length file
    *    existing1: single one byte length file
  */
  // clang-format off
  static const auto permuter(mt_permute_parameters<  // This is a multithreaded parameter permutation test
    result<void>,                                    // The output outcome/result/option type. Type void means we don't care about the return type.
    parameters<                                      // The types of one or more input parameters to permute/fuzz the kernel with.
      typename file_handle::mode,
      typename file_handle::creation,
      typename file_handle::flag
    >,
    // Any additional per-permute parameters not used to invoke the kernel
    precondition::filesystem_setup_parameters,
    postcondition::filesystem_comparison_structure_parameters
  >(
    { // Initialiser list of output value expected for the input parameters, plus any precondition/postcondition parameters

      // Does the mode parameter have the expected side effects?
      {   make_ready_result<void>(),       { file_handle::mode::none,       file_handle::creation::if_needed, file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
      {   make_ready_result<void>(),       { file_handle::mode::attr_read,  file_handle::creation::if_needed, file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
      {   make_ready_result<void>(),       { file_handle::mode::attr_write, file_handle::creation::if_needed, file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write,      file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write,      file_handle::creation::if_needed, file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
      {   make_ready_result<void>(),       { file_handle::mode::append,     file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::append,     file_handle::creation::if_needed, file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
#ifdef _WIN32
      // Interestingly Windows lets you create a new file which has no access at all. Go figure.
      {   make_ready_result<void>(),       { file_handle::mode::none,       file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::attr_read,  file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::attr_write, file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
#else
      { make_errored_result<void>(EACCES), { file_handle::mode::none,       file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "non-existing" }},
      { make_errored_result<void>(EACCES), { file_handle::mode::attr_read,  file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "non-existing" }},
      { make_errored_result<void>(EACCES), { file_handle::mode::attr_write, file_handle::creation::if_needed, file_handle::flag::none }, { "non-existing" }, { "non-existing" }},
#endif

      // Does the creation parameter have the expected side effects?
      { make_errored_result<void>(ENOENT), { file_handle::mode::write, file_handle::creation::open_existing    , file_handle::flag::none }, { "non-existing" }, { "non-existing" }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::open_existing    , file_handle::flag::none }, { "existing0"    }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::open_existing    , file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::only_if_not_exist, file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
      { make_errored_result<void>(EEXIST), { file_handle::mode::write, file_handle::creation::only_if_not_exist, file_handle::flag::none }, { "existing0"    }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::if_needed        , file_handle::flag::none }, { "non-existing" }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::if_needed        , file_handle::flag::none }, { "existing1"    }, { "existing1"    }},
      { make_errored_result<void>(ENOENT), { file_handle::mode::write, file_handle::creation::truncate         , file_handle::flag::none }, { "non-existing" }, { "non-existing" }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::truncate         , file_handle::flag::none }, { "existing0"    }, { "existing0"    }},
      {   make_ready_result<void>(),       { file_handle::mode::write, file_handle::creation::truncate         , file_handle::flag::none }, { "existing1"    }, { "existing0"    }},

      // Does the flag parameter have the expected side effects?
      { make_ready_result<void>(), { file_handle::mode::write, file_handle::creation::open_existing, file_handle::flag::win_delete_on_last_close|file_handle::flag::posix_unlink_on_first_close }, { "existing1" }, { "non-existing" }}
    },
    // Any parameters from now on are called before each permutation and the object returned is
    // destroyed after each permutation. The callspec is (parameter_permuter<...> *parent, outcome<T> &testret, size_t, pars)
    precondition::filesystem_setup(),                 // Configure this filesystem workspace before the test
    postcondition::filesystem_comparison_structure()  // Do a structural comparison of the filesystem workspace after the test
  ));
  // clang-format on

  // Have the permuter permute callable f with all the permutations, returning outcomes
  auto results = permuter(std::forward<U>(f));
  static_assert(permuter.permutation_results_type_is_constant_sized, "Results type returned should be a std::array due to the constant sized input");
  static_assert(results.size() > 0, "Results type returned should be a std::array due to the constant sized input");
  std::vector<std::function<void()>> checks;

  // Check each of the results, calling the failure/pass callables supplied. We don't do
  // this inside the kernel or permuter as it messes with fuzzing/sanitising.
  //
  // Note that we accumulate failures into the checks vector for later processing
  bool all_passed = permuter.check(results, pretty_print_failure(permuter, [&checks](const auto &result, const auto &shouldbe) { checks.push_back([&] { BOOST_CHECK(result == shouldbe); }); }), pretty_print_success(permuter));
  BOOST_CHECK(all_passed);

  // The pretty printing gets messed up by the unit test output, so defer telling it
  // about failures until now
  for(auto &i : checks)
    i();
}

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, file_handle_create_close, file_handle, "Tests that afio::file_handle's creation parameter works as expected", file_handle_create_close_creation(file_handle_create_close::test_kernel_file_handle))
BOOST_KERNELTEST_TEST_KERNEL(integration, afio, file_handle_create_close, async_file_handle, "Tests that afio::async_file_handle's creation parameter works as expected", file_handle_create_close_creation(file_handle_create_close::test_kernel_async_file_handle))
