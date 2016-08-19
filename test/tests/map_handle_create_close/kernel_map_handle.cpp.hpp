/* Test kernel for map_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: August 2016
*/

#include "../../test_kernel_decl.hpp"

namespace map_handle_create_close
{
  BOOST_AFIO_TEST_KERNEL_DECL boost::outcome::result<boost::afio::map_handle> test_kernel_map_handle(boost::afio::file_handle &backing, boost::afio::map_handle::size_type bytes, boost::afio::section_handle::flag m)
  {
    auto sectionh = boost::afio::section_handle::section(backing, bytes, boost::afio::section_handle::flag::readwrite);
    auto h = boost::afio::map_handle::map(sectionh.get(), bytes, 0, m);
    return h;
  }
}
