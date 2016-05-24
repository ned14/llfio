/* Integration test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Apr 2016
*/

#include "../../integration_test_kernel.hpp"
#include "kernel_async_file_handle.hpp"
#include "kernel_file_handle.hpp"

// TODO FIXME: Make the below lambda based and rid ourselves of some of the macros

template <class U> inline void file_handle_create_close_creation(U &&f)
{
  using namespace BOOST_AFIO_V2_NAMESPACE;
  using BOOST_AFIO_V2_NAMESPACE::result;
  /* For every one of these parameter values listed, test with the value using
  the input workspace which should produce outcome workspace. Workspaces are:
  * non-existing: no files
  *    existing0: single zero length file
  *    existing1: single one byte length file
  */
  // clang-format off
  integration_test::mt_permute_parameters<  // This is a multithreaded parameter permutation test
    result<void>,                           // The output outcome/result/option type. Type void means we don't care about the return type.
    typename FileHandleType::creation       // The types of one or more input parameters to fuzz.
  >(
    {                                       // Initialiser list of output value expected for the input parameters
      { make_errored_result<void>(ENOENT), file_handle::creation::open_existing,     "non-existing", "non-existing" },
      {   make_ready_result<void>(),       file_handle::creation::open_existing,     "existing0",    "existing0"    },
      {   make_ready_result<void>(),       file_handle::creation::open_existing,     "existing1",    "existing1"    },
      {   make_ready_result<void>(),       file_handle::creation::only_if_not_exist, "non-existing", "existing0"    },
      { make_errored_result<void>(EEXIST), file_handle::creation::only_if_not_exist, "existing0",    "existing0"    },
      {   make_ready_result<void>(),       file_handle::creation::if_needed,         "non-existing", "existing0"    },
      {   make_ready_result<void>(),       file_handle::creation::if_needed,         "existing1",    "existing1"    },
      { make_errored_result<void>(ENOENT), file_handle::creation::truncate,          "non-existing", "non-existing" },
      {   make_ready_result<void>(),       file_handle::creation::truncate,          "existing0",    "existing0"    },
      {   make_ready_result<void>(),       file_handle::creation::truncate,          "existing1",    "existing0"    }
    },
    integration_test::pre_filesystem_setup("file_handle_create_close"),               // Configure this filesystem workspace before the test
    integration_test::post_filesystem_comparison_inexact("file_handle_create_close")  // Do an inexact comparison of the filesystem workspace after the test
    )
  // clang-format on
  (std::forward<U>(f));
}

BOOST_OUTCOME_INTEGRATION_TEST(afio, integration / file_handle_create_close / file_handle, "Tests that afio::file_handle's creation parameter works as expected", file_handle_create_close_creation(file_handle_create_close::test_kernel_file_handle))
BOOST_OUTCOME_INTEGRATION_TEST(afio, integration / file_handle_create_close / async_file_handle, "Tests that afio::async_file_handle's creation parameter works as expected", file_handle_create_close_creation(file_handle_create_close::test_kernel_async_file_handle))
