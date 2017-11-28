/* Integration test kernel for map_handle create and close
(C) 2016-2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2016


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
    (See accompanying file Licence.txt or copy at
          http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../test_kernel_decl.hpp"

static inline void TestFileHandleLockUnlock()
{
  namespace afio = AFIO_V2_NAMESPACE;
  afio::file_handle h1 = afio::file_handle::file({}, "temp", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).value();
  afio::file_handle h2 = afio::file_handle::file({}, "temp", afio::file_handle::mode::write, afio::file_handle::creation::if_needed, afio::file_handle::caching::temporary, afio::file_handle::flag::unlink_on_close).value();
  // Two exclusive locks not possible
  {
    auto _1 = h1.lock(0, 0, true, std::chrono::seconds(0));
    BOOST_REQUIRE(!_1.has_error());
    if(h1.flags() & afio::file_handle::flag::byte_lock_insanity)
    {
      std::cout << "This platform has byte_lock_insanity so this test won't be useful, bailing out" << std::endl;
      return;
    }
    auto _2 = h2.lock(0, 0, true, std::chrono::seconds(0));
    BOOST_REQUIRE(_2.has_error());
    BOOST_CHECK(_2.error() == std::errc::timed_out);
  }
  // Two non-exclusive locks okay
  {
    auto _1 = h1.lock(0, 0, false, std::chrono::seconds(0));
    BOOST_REQUIRE(!_1.has_error());
    auto _2 = h2.lock(0, 0, false, std::chrono::seconds(0));
    BOOST_REQUIRE(!_2.has_error());
  }
  // Non-exclusive excludes exclusive
  {
    auto _1 = h1.lock(0, 0, false, std::chrono::seconds(0));
    BOOST_REQUIRE(!_1.has_error());
    auto _2 = h2.lock(0, 0, true, std::chrono::seconds(0));
    BOOST_REQUIRE(_2.has_error());
    BOOST_CHECK(_2.error() == std::errc::timed_out);
  }
}

KERNELTEST_TEST_KERNEL(integration, afio, file_handle_lock_unlock, file_handle, "Tests that afio::file_handle's lock and unlock work as expected", TestFileHandleLockUnlock())
