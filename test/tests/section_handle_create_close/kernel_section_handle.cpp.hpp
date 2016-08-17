/* Test kernel for section_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: August 2016
*/

#include "../../test_kernel_decl.hpp"

namespace section_handle_create_close
{
  BOOST_AFIO_TEST_KERNEL_DECL boost::outcome::result<boost::afio::section_handle> test_kernel_section_handle(boost::afio::file_handle &backing, boost::afio::section_handle::extent_type maximum_size, boost::afio::section_handle::flag m)
  {
    auto h = boost::afio::section_handle::section(backing, maximum_size, m);
    if(h)
      h.get().close();
    return h;
  }
}
