/* Integration test kernel for the algorithm::contents() tree enumeration
(C) 2026 Niall Douglas <http://www.nedproductions.biz/> (1 commit)
File Created: Nov 2026


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

#include <algorithm>
#include <future>

namespace
{
  // A visitor which corrupts the cached length of the root's current path just
  // before every enumeration, i.e. it pretends the root was renamed to a longer path
  // after its length was cached. Stripping a too short length leaves a tail of the
  // root's own path glued to the front of the relative path, which does not resolve to
  // the same inode, so the algorithm has to refresh the cached length and retry. Nothing
  // guarantees that a refresh converges (a filesystem root's current path can never gain
  // the separator the old arithmetic added), so the retry must be bounded; the watchdog
  // in TestContents() turns a regression to an unbounded loop into a failure, not a hang.
  struct stale_root_cache_visitor : public LLFIO_V2_NAMESPACE::algorithm::contents_visitor
  {
    using LLFIO_V2_NAMESPACE::algorithm::contents_visitor::contents_visitor;

    LLFIO_V2_NAMESPACE::result<void> post_enumeration(void *data, const LLFIO_V2_NAMESPACE::directory_handle &dirh,
                                                      LLFIO_V2_NAMESPACE::directory_handle::buffers_type &contents,
                                                      size_t depth) noexcept override
    {
      auto *state = reinterpret_cast<_state_type *>(data);
      const size_t cached = state->rootdirpathlen.load(std::memory_order_relaxed);
      state->rootdirpathlen.store(cached > 3 ? cached - 3 : 0, std::memory_order_relaxed);
      return contents_visitor::post_enumeration(data, dirh, contents, depth);
    }
  };
}  // namespace

static inline void TestContents()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  auto dirh = llfio::directory_handle::temp_directory().value();
  {
    auto subdirh = llfio::directory_handle::directory(dirh, "a", llfio::directory_handle::mode::write,
                                                      llfio::directory_handle::creation::if_needed)
                   .value();
    (void) llfio::file_handle::file(subdirh, "b.txt", llfio::file_handle::mode::write,
                                    llfio::file_handle::creation::if_needed)
    .value();
  }
  (void) llfio::file_handle::file(dirh, "c.txt", llfio::file_handle::mode::write,
                                  llfio::file_handle::creation::if_needed)
  .value();

  auto enumerate = [&](llfio::algorithm::contents_visitor *visitor)
  {
    auto r = llfio::algorithm::contents(dirh, visitor);
    BOOST_REQUIRE(r);
    std::vector<std::string> paths;
    for(auto &entry : r.value())
    {
      // Every returned path is relative to the root handle, never absolute.
      BOOST_CHECK(!entry.first.is_absolute());
      paths.push_back(entry.first.generic_string());
    }
    std::sort(paths.begin(), paths.end());
    for(auto &path : paths)
    {
      std::cout << "   " << path << std::endl;
    }
    return paths;
  };

  const std::vector<std::string> expected{"a", "a/b.txt", "c.txt"};
  std::cout << "\ncontents() of a small tree:" << std::endl;
  BOOST_CHECK(enumerate(nullptr) == expected);
  BOOST_CHECK(enumerate(nullptr) == expected);  // the cached root path length is now warm

  std::cout << "\ncontents() with a stale cached root path length must terminate:" << std::endl;
  stale_root_cache_visitor visitor;
  auto f = std::async(std::launch::async, [&] { return enumerate(&visitor); });
  if(f.wait_for(std::chrono::seconds(60)) != std::future_status::ready)
  {
    std::cerr
    << "\nFAIL: algorithm::contents() did not terminate within 60 seconds, so its retry loop is unbounded again\n"
    << std::endl;
    std::_Exit(EXIT_FAILURE);
  }
  BOOST_CHECK(f.get() == expected);
}

KERNELTEST_TEST_KERNEL(integration, llfio, algorithm, contents,
                       "Tests that llfio::algorithm::contents() returns every entry relative to the root, and "
                       "terminates when the cached root path length is stale",
                       TestContents())
