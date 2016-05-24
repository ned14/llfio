/* Test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: May 2016
*/

#include "../../test_kernel.hpp"

namespace file_handle_create_close
{
  extern BOOST_OUTCOME_TEST_KERNEL_DECL boost::outcome::result<boost::afio::async_file_handle> test_kernel_async_file_handle(boost::afio::async_file_handle::creation c)
  {
    auto h = boost::afio::async_file_handle::file("testfile.txt", boost::afio::async_file_handle::mode::write, c);
    BOOST_OUTCOME_TEST_KERNEL_RETURN(h);
  }
}
