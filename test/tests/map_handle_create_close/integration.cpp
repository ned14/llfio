/* Integration test kernel for map_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Aug 2016
*/

#include "../kerneltest/include/boost/kerneltest.hpp"

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, map_handle_nocommit_and_commit, map_handle, "Tests that afio::map_handle's nocommit and commit work as expected", [] {
  //! \todo TODO test map_handle::commit, decommit, zero, prefetch, do_not_store
})
