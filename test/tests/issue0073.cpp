/* Integration test kernel for issue #73 Windows directory junctions cannot be opened with symlink_handle
(C) 2021 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Mar 2021


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

static inline void TestIssue0073()
{
#ifndef _WIN32
  return;
#endif
  namespace llfio = LLFIO_V2_NAMESPACE;

  auto tempdirh = llfio::temp_directory().value();
  auto untempdir = llfio::make_scope_exit([&]() noexcept { (void) llfio::algorithm::reduce(std::move(tempdirh)); });
  std::cout << "NOTE: Test directory can be found at " << tempdirh.current_path().value() << std::endl;

  // Test windows junctions
  {
    auto dirh =
    llfio::directory_handle::directory(tempdirh, "testjunction", llfio::symlink_handle::mode::write, llfio::symlink_handle::creation::if_needed).value();
    auto h = llfio::symlink_handle::symlink(dirh, {}, llfio::symlink_handle::mode::write).value();
    h.write({llfio::symlink_handle::const_buffers_type("linkcontent", llfio::symlink_handle::symlink_type::win_junction)}).value();
    auto rb = h.read().value();
    BOOST_CHECK(rb.type() == llfio::symlink_handle::symlink_type::win_junction);
    BOOST_CHECK(rb.path().path() == "linkcontent");
    h.unlink().value();
  }

  // Test normal symbolic links
  {
    auto h_ = llfio::symlink_handle::symlink(tempdirh, "testlink", llfio::symlink_handle::mode::write, llfio::symlink_handle::creation::if_needed);
    if(!h_ && h_.error() == llfio::errc::function_not_supported)
    {
      std::cout << "NOTE: Failed to create symbolic link, assuming lack of SeCreateSymbolicLinkPrivilege privileges." << std::endl;
      return;
    }
    auto h = std::move(h_).value();
    h.write("linkcontent").value();
    auto rb = h.read().value();
    BOOST_CHECK(rb.type() == llfio::symlink_handle::symlink_type::symbolic);
    BOOST_CHECK(rb.path().path() == "linkcontent");
    h.unlink().value();
  }
}

KERNELTEST_TEST_KERNEL(regression, llfio, issues, 0073, "Tests issue #0073 Windows directory junctions cannot be opened with symlink_handle", TestIssue0073())
