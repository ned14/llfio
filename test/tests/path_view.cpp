/* Integration test kernel for whether path views work
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2017


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

#include "../../include/afio/afio.hpp"
#include "kerneltest/include/kerneltest.hpp"

static inline void TestPathView()
{
  namespace afio = AFIO_V2_NAMESPACE;
  // path view has constexpr construction
  constexpr afio::path_view a, b("hello");
  BOOST_CHECK(a.empty());
  BOOST_CHECK(!b.empty());
  BOOST_CHECK(b == "hello");
  // Globs
  BOOST_CHECK(afio::path_view("niall*").contains_glob());
  // Splitting
  constexpr const char p[] = "/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0";
  afio::path_view e(p);
  afio::path_view f(e.filename());
  e.remove_filename();
  BOOST_CHECK(e == "/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir");
  BOOST_CHECK(f == "0");
#ifndef _WIN32
  // cstr
  afio::path_view::c_str g(e);
  BOOST_CHECK(g.buffer != p);
  afio::path_view::c_str h(f);
  BOOST_CHECK(h.buffer == p + 70);  // NOLINT
#endif

#ifdef _WIN32
  // On Windows, UTF-8 and UTF-16 paths are equivalent and backslash conversion happens
  afio::path_view c("path/to"), d(L"path\\to");
  BOOST_CHECK(c == d);
  // Globs
  BOOST_CHECK(afio::path_view(L"niall*").contains_glob());
  BOOST_CHECK(afio::path_view("0123456789012345678901234567890123456789012345678901234567890123.deleted").is_afio_deleted());
  BOOST_CHECK(afio::path_view(L"0123456789012345678901234567890123456789012345678901234567890123.deleted").is_afio_deleted());
  BOOST_CHECK(!afio::path_view("0123456789012345678901234567890123456789g12345678901234567890123.deleted").is_afio_deleted());
  // Splitting
  constexpr const wchar_t p2[] = L"\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0";
  afio::path_view g(p2);
  afio::path_view h(g.filename());
  g.remove_filename();
  BOOST_CHECK(g == "\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir");
  BOOST_CHECK(h == "0");
  // cstr
  afio::path_view::c_str i(g, false);
  BOOST_CHECK(i.buffer != p2);
  afio::path_view::c_str j(g, true);
  BOOST_CHECK(j.buffer == p2);
  afio::path_view::c_str k(h, false);
  BOOST_CHECK(k.buffer == p2 + 70);
#endif
}

KERNELTEST_TEST_KERNEL(integration, afio, path_view, path_view, "Tests that afio::path_view() works as expected", TestPathView())
