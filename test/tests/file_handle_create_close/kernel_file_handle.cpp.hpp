/* Test kernel for file_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: May 2016
*/

#include "../../test_kernel_decl.hpp"

namespace file_handle_create_close
{
  BOOST_AFIO_TEST_KERNEL_DECL boost::outcome::result<boost::afio::file_handle> test_kernel_file_handle(boost::afio::file_handle::mode m, boost::afio::file_handle::creation c, boost::afio::file_handle::flag f)
  {
    auto h = boost::afio::file_handle::file("testfile.txt", m, c, boost::afio::file_handle::caching::all, f);
    if(h)
      h.get().close();
    return h;
  }
}
