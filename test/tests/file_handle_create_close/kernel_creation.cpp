/* Integration test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Apr 2016
*/

#include "../../integration_test_kernel.hpp"

// TODO FIXME: Make the below lambda based and rid ourselves of some of the macros

template <class FileHandleType> inline void file_handle_create_close_creation()
{
  using namespace BOOST_AFIO_V2_NAMESPACE;
  using BOOST_AFIO_V2_NAMESPACE::result;
  typename FileHandleType::creation c;
  /* For every one of these parameter values listed, test with the value using
  the input workspace which should produce outcome workspace. Workspaces are:
  * non-existing: no files
  *    existing0: single zero length file
  *    existing1: single one byte length file
  */
  // clang-format off
  BOOST_OUTCOME_INTEGRATION_TEST_MT_KERNEL_PARAMETER_TO_FILESYSTEM((result<void>), c, "file_handle_create_close", ({
    { file_handle::creation::open_existing,     "non-existing", make_errored_result<void>(ENOENT), "non-existing" },
    { file_handle::creation::open_existing,     "existing0",            make_result<void>(),       "existing0" },
    { file_handle::creation::open_existing,     "existing1",            make_result<void>(),       "existing1" },
    { file_handle::creation::only_if_not_exist, "non-existing",         make_result<void>(),       "existing0" },
    { file_handle::creation::only_if_not_exist, "existing0",    make_errored_result<void>(EEXIST), "existing0" },
    { file_handle::creation::if_needed,         "non-existing",         make_result<void>(),       "existing0" },
    { file_handle::creation::if_needed,         "existing1",            make_result<void>(),       "existing1" },
    { file_handle::creation::truncate,          "non-existing", make_errored_result<void>(ENOENT), "non-existing" },
    { file_handle::creation::truncate,          "existing0",            make_result<void>(),       "existing0" },
    { file_handle::creation::truncate,          "existing1",            make_result<void>(),       "existing0" }
  }),
                                                                   // clang-format on
                                                                   {
                                                                     typename FileHandleType::mode m(FileHandleType::mode::write);
                                                                     {
                                                                       auto h = FileHandleType::file("testfile.txt", m, c);
                                                                       BOOST_OUTCOME_INTEGRATION_TEST_KERNEL_RESULT(h);
                                                                     }
                                                                   })
}

BOOST_OUTCOME_INTEGRATION_TEST_KERNEL(afio, integration / file_handle_create_close / creation1, "Tests that afio::file_handle's creation parameter works as expected", file_handle_create_close_creation<BOOST_AFIO_V2_NAMESPACE::file_handle>())
BOOST_OUTCOME_INTEGRATION_TEST_KERNEL(afio, integration / file_handle_create_close / creation2, "Tests that afio::async_file_handle's creation parameter works as expected", file_handle_create_close_creation<BOOST_AFIO_V2_NAMESPACE::async_file_handle>())
