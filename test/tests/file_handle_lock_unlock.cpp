/* Integration test kernel for map_handle create and close
(C) 2016 Niall Douglas http://www.nedprod.com/
File Created: Aug 2016
*/

#include "../../include/boost/afio/afio.hpp"
#include "../kerneltest/include/boost/kerneltest.hpp"

static inline void TestFileHandleLockUnlock()
{
  namespace afio = BOOST_AFIO_V2_NAMESPACE;
  namespace stl11 = afio::stl11;
  afio::file_handle h1 = afio::file_handle::file("temp", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).get();
  afio::file_handle h2 = afio::file_handle::file("temp", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).get();
  // Two exclusive locks not possible
  {
    auto _1 = h1.lock(0, 0, true, stl11::chrono::seconds(0));
    BOOST_REQUIRE(!_1.has_error());
    if(h1.flags() & afio::file_handle::flag::byte_lock_insanity)
    {
      std::cout << "This platform has byte_lock_insanity so this test won't be useful, bailing out" << std::endl;
      return;
    }
    auto _2 = h2.lock(0, 0, true, stl11::chrono::seconds(0));
    BOOST_REQUIRE(_2.has_error());
    BOOST_CHECK(_2.error() == stl11::errc::timed_out);
  }
  // Two non-exclusive locks okay
  {
    auto _1 = h1.lock(0, 0, false, stl11::chrono::seconds(0));
    BOOST_REQUIRE(!_1.has_error());
    auto _2 = h2.lock(0, 0, false, stl11::chrono::seconds(0));
    BOOST_REQUIRE(!_2.has_error());
  }
  // Non-exclusive excludes exclusive
  {
    auto _1 = h1.lock(0, 0, false, stl11::chrono::seconds(0));
    BOOST_REQUIRE(!_1.has_error());
    auto _2 = h2.lock(0, 0, true, stl11::chrono::seconds(0));
    BOOST_REQUIRE(_2.has_error());
    BOOST_CHECK(_2.error() == stl11::errc::timed_out);
  }
}

BOOST_KERNELTEST_TEST_KERNEL(integration, afio, file_handle_lock_unlock, file_handle, "Tests that afio::file_handle's lock and unlock work as expected", TestFileHandleLockUnlock())
