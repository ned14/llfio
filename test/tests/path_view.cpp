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

#include "../test_kernel_decl.hpp"

template <class U> inline void CheckPathView(const LLFIO_V2_NAMESPACE::filesystem::path &p, const char *desc, U &&c)
{
  using LLFIO_V2_NAMESPACE::filesystem::path;
  using LLFIO_V2_NAMESPACE::path_view;
  auto r1 = c(p);
  auto r2 = c(path_view(p));
  BOOST_CHECK(r1 == r2);
  if(r1 != r2)
  {
    std::cerr << "For " << desc << " with path " << p << "\n";
    std::cerr << "   filesystem::path returned " << r1 << "\n";
    std::cerr << "          path_view returned " << r2 << std::endl;
  }
}

static inline void CheckPathView(const LLFIO_V2_NAMESPACE::filesystem::path &path)
{
  CheckPathView(path, "root_directory()", [](const auto &p) { return p.root_directory(); });
  CheckPathView(path, "root_path()", [](const auto &p) { return p.root_path(); });
  CheckPathView(path, "relative_path()", [](const auto &p) { return p.relative_path(); });
  CheckPathView(path, "parent_path()", [](const auto &p) { return p.parent_path(); });
  CheckPathView(path, "filename()", [](const auto &p) { return p.filename(); });
  CheckPathView(path, "stem()", [](const auto &p) { return p.stem(); });
  CheckPathView(path, "extension()", [](const auto &p) { return p.extension(); });
}

static inline void TestPathView()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  // path view has constexpr construction
  constexpr llfio::path_view a, b("hello");
  BOOST_CHECK(a.empty());
  BOOST_CHECK(!b.empty());
  BOOST_CHECK(b == "hello");
  // Globs
  BOOST_CHECK(llfio::path_view("niall*").contains_glob());
  // Splitting
  constexpr const char p[] = "/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0";
  llfio::path_view e(p);  // NOLINT
  llfio::path_view f(e.filename());
  e.remove_filename();
  BOOST_CHECK(e == "/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir");
  BOOST_CHECK(f == "0");
#ifndef _WIN32
  // cstr
  llfio::path_view::c_str<> g(e);
  BOOST_CHECK(g.buffer != p);  // NOLINT
  llfio::path_view::c_str<> h(f);
  BOOST_CHECK(h.buffer == p + 70);  // NOLINT
#endif
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir");
#if 0
  // I think we are standards conforming here, Dinkumware and libstdc++ are not
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/");
#endif
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0");
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0.txt");
  CheckPathView("boostish/afio/programs/build_posix/testdir");
#if 0
  // I think we are standards conforming here, Dinkumware and libstdc++ are not
  CheckPathView("boostish/afio/programs/build_posix/testdir/");
#endif
  CheckPathView("boostish/afio/programs/build_posix/testdir/0");
  CheckPathView("boostish/afio/programs/build_posix/testdir/0.txt");
  CheckPathView("0");
  CheckPathView("0.txt");
  CheckPathView("0.foo.txt");
  CheckPathView(".0.foo.txt");
#if 0
  // I think we are standards conforming here, Dinkumware and libstdc++ are not
  CheckPathView(".txt");
  CheckPathView("/");
  CheckPathView("//");
#endif
  CheckPathView("");
  CheckPathView(".");
  CheckPathView("..");

#ifdef _WIN32
  // On Windows, UTF-8 and UTF-16 paths are equivalent and backslash conversion happens
  llfio::path_view c("path/to"), d(L"path\\to");
  BOOST_CHECK(c == d);
  // Globs
  BOOST_CHECK(llfio::path_view(L"niall*").contains_glob());
  BOOST_CHECK(llfio::path_view("0123456789012345678901234567890123456789012345678901234567890123.deleted").is_llfio_deleted());
  BOOST_CHECK(llfio::path_view(L"0123456789012345678901234567890123456789012345678901234567890123.deleted").is_llfio_deleted());
  BOOST_CHECK(!llfio::path_view("0123456789012345678901234567890123456789g12345678901234567890123.deleted").is_llfio_deleted());
  // Splitting
  constexpr const wchar_t p2[] = L"\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0";
  llfio::path_view g(p2);
  llfio::path_view h(g.filename());
  g.remove_filename();
  BOOST_CHECK(g == "\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir");
  BOOST_CHECK(h == "0");
  // cstr
  llfio::path_view::c_str<> i(g, false);
  BOOST_CHECK(i.buffer != p2);
  llfio::path_view::c_str<> j(g, true);
  BOOST_CHECK(j.buffer == p2);
  llfio::path_view::c_str<> k(h, false);
  BOOST_CHECK(k.buffer == p2 + 70);

  CheckPathView(L"\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0");
  CheckPathView(L"C:\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0");
  CheckPathView("C:/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0.txt");
  // CheckPathView(L"\\\\niall\\douglas.txt");
  CheckPathView(L"\\!!\\niall\\douglas.txt");
  CheckPathView(L"\\??\\niall\\douglas.txt");
  CheckPathView(L"\\\\?\\niall\\douglas.txt");
  CheckPathView(L"\\\\.\\niall\\douglas.txt");

  // Handle NT kernel paths correctly
  BOOST_CHECK(llfio::path_view(L"\\\\niall").is_absolute());
  BOOST_CHECK(llfio::path_view(L"\\!!\\niall").is_absolute());
  BOOST_CHECK(llfio::path_view(L"\\??\\niall").is_absolute());
  BOOST_CHECK(llfio::path_view(L"\\\\?\\niall").is_absolute());
  BOOST_CHECK(llfio::path_view(L"\\\\.\\niall").is_absolute());
  // On Windows this is relative, on POSIX it is absolute
  BOOST_CHECK(llfio::path_view("/niall").is_relative());
#else
  BOOST_CHECK(llfio::path_view("/niall").is_absolute());
#endif
}

KERNELTEST_TEST_KERNEL(integration, llfio, path_view, path_view, "Tests that llfio::path_view() works as expected", TestPathView())
