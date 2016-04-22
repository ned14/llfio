/* Integration test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Apr 2016
*/

#include "../../integration_test_kernel.hpp"

BOOST_OUTCOME_INTEGRATION_TEST_KERNEL(file_handle_create_close_creation)
{
  file_handle::creation c;
  // clang-format off
  /* For every one of these parameter values listed, test with the value using
  the input workspace which should produce outcome workspace.
  */
  BOOST_OUTCOME_INTEGRATION_TEST_KERNEL_PARAMETER_TO_WORKSPACE(c, {
    {file_handle::creation::open_existing,     "non-existing",  make_errored_result<const char *>(ENOENT) },
    {file_handle::creation::open_existing,     "existing0",             make_result<const char *>("existing0") },
    {file_handle::creation::open_existing,     "existing1",             make_result<const char *>("existing1") },
    {file_handle::creation::only_if_not_exist, "non-existing",          make_result<const char *>("existing0") },
    {file_handle::creation::only_if_not_exist, "existing0",     make_errored_result<const char *>(EEXIST) },
    {file_handle::creation::if_needed,         "non-existing",          make_result<const char *>("existing0") },
    {file_handle::creation::if_needed,         "existing1",             make_result<const char *>("existing1") },
    {file_handle::creation::truncate,          "non-existing",  make_errored_result<const char *>(ENOENT) },
    {file_handle::creation::truncate,          "existing0",             make_result<const char *>("existing0") },
    {file_handle::creation::truncate,          "existing1",             make_result<const char *>("existing0") }
  })
  {
    file_handle::mode m=file_handle::mode::write;
    {
      // clang-format on
      auto h = file_handle::file("testfile.txt", m, c);
      BOOST_OUTCOME_INTEGRATION_TEST_KERNEL_RESULT(h);
    }
  }
}
