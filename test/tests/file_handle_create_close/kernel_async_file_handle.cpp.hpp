/* Test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: May 2016
*/

#include "../../test_kernel_decl.hpp"

namespace file_handle_create_close
{
  BOOST_AFIO_TEST_KERNEL_DECL boost::outcome::result<boost::afio::async_file_handle> test_kernel_async_file_handle(boost::afio::async_file_handle::mode m, boost::afio::async_file_handle::creation c, boost::afio::async_file_handle::flag f)
  {
    boost::afio::io_service service;
    auto h = boost::afio::async_file_handle::async_file(service, "testfile.txt", m, c, boost::afio::async_file_handle::caching::all, f);
    if(h)
      h.get().close();
    return h;
  }
}
