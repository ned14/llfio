/* Integration test kernel for whether path views work
(C) 2017-2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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
  using LLFIO_V2_NAMESPACE::path_view;
  using LLFIO_V2_NAMESPACE::filesystem::path;
  auto r1 = c(p);
  auto r2 = c(path_view(p));
  if(r2.empty() && r1.native().size() == 1 && r1.native()[0] == '.')
  {
    // libstdc++ returns "." as the tail component for /a/b/ type paths
    BOOST_CHECK(true);
  }
  else
  {
    BOOST_CHECK(r1 == r2);
  }
  // if(r1 != r2)
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

static inline void CheckPathIteration(const LLFIO_V2_NAMESPACE::filesystem::path &path)
{
  LLFIO_V2_NAMESPACE::filesystem::path test1(path);
  LLFIO_V2_NAMESPACE::path_view test2(test1);
  std::cout << "\n" << test1 << std::endl;
  auto it1 = test1.begin();
  auto it2 = test2.begin();
  for(; it1 != test1.end() && it2 != test2.end(); ++it1, ++it2)
  {
    std::cout << "   " << *it1 << " == " << *it2 << "?" << std::endl;
    if(it2->empty() && it1->native().size() == 1 && it1->native()[0] == '.')
    {
      // libstdc++ returns "." as the tail component for /a/b/ type paths
      BOOST_CHECK(true);
    }
    else
    {
      BOOST_CHECK(*it1 == it2->path());
    }
  }
  BOOST_CHECK(it1 == test1.end());
  BOOST_CHECK(it2 == test2.end());
  for(--it1, --it2; it1 != test1.begin() && it2 != test2.begin(); --it1, --it2)
  {
    std::cout << "   " << *it1 << " == " << *it2 << "?" << std::endl;
    if(it2->empty() && it1->native().size() == 1 && it1->native()[0] == '.')
    {
      // libstdc++ returns "." as the tail component for /a/b/ type paths
      BOOST_CHECK(true);
    }
    else
    {
      BOOST_CHECK(*it1 == it2->path());
    }
  }
  BOOST_CHECK(it1 == test1.begin());
  BOOST_CHECK(it2 == test2.begin());
  std::cout << "   " << *it1 << " == " << *it2 << "?" << std::endl;
  BOOST_CHECK(*it1 == it2->path());
}

static inline void TestPathView()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  // path view has constexpr construction
  constexpr llfio::path_view a, b("hello");
  BOOST_CHECK(a.empty());
  BOOST_CHECK(!b.empty());
  BOOST_CHECK(0 == b.compare<>("hello"));
  // Globs
  BOOST_CHECK(llfio::path_view("niall*").contains_glob());
  // Splitting
  constexpr const char p[] = "/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0";
  llfio::path_view e(p);  // NOLINT
  llfio::path_view f(e.filename());
  e = e.remove_filename();
  BOOST_CHECK(0 == e.compare<>("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir"));
  BOOST_CHECK(0 == f.compare<>("0"));
#ifndef _WIN32
  // cstr
  llfio::path_view::c_str<> g(e, llfio::path_view::zero_terminated);
  BOOST_CHECK(g.buffer != p);  // NOLINT
  llfio::path_view::c_str<> h(f, llfio::path_view::zero_terminated);
  BOOST_CHECK(h.buffer == p + 70);  // NOLINT
#endif
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir");
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/");
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0");
  CheckPathView("/mnt/c/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0.txt");
  CheckPathView("boostish/afio/programs/build_posix/testdir");
  CheckPathView("boostish/afio/programs/build_posix/testdir/");
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
  BOOST_CHECK(0 == c.compare<>(d));
  // Globs
  BOOST_CHECK(llfio::path_view(L"niall*").contains_glob());
  BOOST_CHECK(llfio::path_view("0123456789012345678901234567890123456789012345678901234567890123.deleted").is_llfio_deleted());
  BOOST_CHECK(llfio::path_view(L"0123456789012345678901234567890123456789012345678901234567890123.deleted").is_llfio_deleted());
  BOOST_CHECK(!llfio::path_view("0123456789012345678901234567890123456789g12345678901234567890123.deleted").is_llfio_deleted());
  // Splitting
  constexpr const wchar_t p2[] = L"\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0";
  llfio::path_view g(p2);
  llfio::path_view h(g.filename());
  g = g.remove_filename();
  BOOST_CHECK(0 == g.compare<>("\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir"));
  BOOST_CHECK(0 == h.compare<>("0"));
  // cstr
  llfio::path_view::c_str<> i(g, llfio::path_view::zero_terminated);
  BOOST_CHECK(i.buffer != p2);
  llfio::path_view::c_str<> j(g, llfio::path_view::not_zero_terminated);
  BOOST_CHECK(j.buffer == p2);
  llfio::path_view::c_str<> k(h, llfio::path_view::not_zero_terminated);
  BOOST_CHECK(k.buffer == p2 + 70);

  CheckPathView(L"\\mnt\\c\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0");
  CheckPathView(L"C:\\Users\\ned\\Documents\\boostish\\afio\\programs\\build_posix\\testdir\\0");
  CheckPathView("C:/Users/ned/Documents/boostish/afio/programs/build_posix/testdir/0.txt");
  CheckPathView(L"\\\\niall\\douglas.txt");
  // CheckPathView(L"\\!!\\niall\\douglas.txt");
#ifndef _EXPERIMENTAL_FILESYSTEM_
  CheckPathView(L"\\??\\niall\\douglas.txt");
#endif
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

// This fails on VS2017 for no obvious reason
#if _MSC_VER >= 1920
  llfio::
  path_view_component longthing(LR"(Path=C:\Program Files\Docker\Docker\Resources\bin;C:\Program Files (x86)\Microsoft SDKs\Azure\CLI2\wbin;C:\Perl\site\bin;C:\Perl\bin;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Program Files\7-Zip;C:\Tools\GitVersion;C:\Tools\NuGet;C:\Program Files\Microsoft\Web Platform Installer\;C:\Tools\PsTools;C:\Program Files\Git LFS;C:\Program Files\Mercurial\;C:\Program Files (x86)\Subversion\bin;C:\Tools\WebDriver;C:\Tools\Coverity\bin;C:\Tools\MSpec;C:\Tools\NUnit\bin;C:\Tools\NUnit3;C:\Tools\xUnit;C:\Program Files\nodejs;C:\Program Files (x86)\iojs;C:\Program Files\iojs;C:\Program Files\Microsoft SQL Server\120\Tools\Binn\;C:\Program Files\Microsoft SQL Server\Client SDK\ODBC\110\Tools\Binn\;C:\Program Files (x86)\Microsoft SQL Server\120\Tools\Binn\;C:\Program Files\Microsoft SQL Server\120\DTS\Binn\;C:\Program Files (x86)\Microsoft SQL Server\120\Tools\Binn\ManagementStudio\;C:\Program Files (x86)\Microsoft SQL Server\120\DTS\Binn\;C:\Program Files (x86)\Microsoft SQL Server\130\Tools\Binn\;C:\Program Files\Microsoft SQL Server\130\Tools\Binn\;C:\Program Files (x86)\Microsoft SQL Server\130\DTS\Binn\;C:\Program Files\Microsoft SQL Server\130\DTS\Binn\;C:\Program Files\Microsoft SQL Server\Client SDK\ODBC\130\Tools\Binn\;C:\Ruby193\bin;C:\go\bin;C:\Program Files\Java\jdk1.8.0\bin;C:\Program Files (x86)\Apache\Maven\bin;C:\Python27;C:\Python27\Scripts;C:\Program Files (x86)\CMake\bin;C:\Tools\curl\bin;C:\Program Files\Microsoft DNX\Dnvm\;C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\MSBuild\15.0\Bin;C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\Extensions\Microsoft\SQLDB\DAC\130;C:\Program Files\dotnet\;C:\Tools\vcpkg;C:\Program Files (x86)\dotnet\;C:\Program Files (x86)\Microsoft SQL Server\140\Tools\Binn\;C:\Program Files\Microsoft SQL Server\140\Tools\Binn\;C:\Program Files\Microsoft SQL Server\140\DTS\Binn\;C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\Common7\IDE\Extensions\TestPlatform;C:\Program Files (x86)\Microsoft SQL Server\110\DTS\Binn\;C:\Program Files (x86)\Microsoft SQL Server\140\DTS\Binn\;C:\Program Files\erl9.2\bin;C:\Program Files (x86)\NSIS;C:\Tools\Octopus;C:\Program Files\Microsoft Service Fabric\bin\Fabric\Fabric.Code;C:\Program Files\Microsoft SDKs\Service Fabric\Tools\ServiceFabricLocalClusterManager;C:\Program Files\Docker\Docker\resources;C:\Program Files\LLVM\bin;C:\Users\appveyor\AppData\Roaming\npm;C:\Program Files (x86)\Windows Kits\10\Windows Performance Toolkit\;C:\Program Files\PowerShell\6\;C:\Program Files (x86)\nodejs\;C:\Program Files\Git\cmd;C:\Program Files\Git\usr\bin;C:\Program Files\Meson\;C:\ProgramData\chocolatey\bin;C:\Program Files\Amazon\AWSCLI\;C:\Program Files (x86)\Yarn\bin\;C:\Users\appveyor\AppData\Local\Microsoft\WindowsApps;C:\Users\appveyor\.dotnet\tools;C:\Users\appveyor\AppData\Roaming\npm;C:\Users\appveyor\AppData\Local\Yarn\bin;C:\Program Files\AppVeyor\BuildAgent\)");
  std::cout << "A very long path component is " << longthing << std::endl;
#endif
#else
  BOOST_CHECK(llfio::path_view("/niall").is_absolute());
#endif

  // Does iteration work right?
  CheckPathIteration("/mnt/testdir");
  CheckPathIteration("/mnt/testdir/");
  CheckPathIteration("boostish/testdir");
  CheckPathIteration("boostish/testdir/");
  CheckPathIteration("/a/c");
  CheckPathIteration("/a/c/");
  CheckPathIteration("a/c");
  CheckPathIteration("a/c/");

  // Does visitation work right?
  visit(llfio::path_view("hi"), [](auto sv) { BOOST_CHECK(0 == memcmp(sv.data(), "hi", 2)); });
  visit(*llfio::path_view(L"hi").begin(), [](auto sv) { BOOST_CHECK(0 == memcmp(sv.data(), L"hi", 4)); });

  // Custom allocator and deleter
  {
    struct custom_allocate
    {
      mutable int called{0};
      char *operator()(size_t /*unused*/) const
      {
        called++;
        static char buff[4];
        return buff;
      }
    } allocator;
    struct custom_delete
    {
      int sig{0};
      mutable int called{0};
      void operator()(char * /*unused*/) const { called++; }
    } deleter{5};
    llfio::path_view v("foo", 3, llfio::path_view::not_zero_terminated);
    llfio::path_view::c_str<char, custom_delete, 0> zbuff(v, llfio::path_view::zero_terminated, allocator, deleter);
    zbuff.reset();
    BOOST_CHECK(allocator.called == 1);  // copy must not be taken
    BOOST_CHECK(deleter.called == 0);    // copy must be taken
    BOOST_CHECK(zbuff.deleter().called == 1);
    BOOST_CHECK(zbuff.deleter().sig == 5);
  }
  // Custom memory_resource
  {
    struct custom_memory_resource final : llfio::pmr::memory_resource
    {
      int allocated{0}, deleted{0};
      void *do_allocate(size_t /*unused*/, size_t /*unused*/) override
      {
        allocated++;
        static char buff[4];
        return buff;
      }
      void do_deallocate(void * /*unused*/, size_t /*unused*/, size_t /*unused*/) override { deleted++; }
      bool do_is_equal(const llfio::pmr::memory_resource & /*unused*/) const noexcept override { return false; }
    } resource;
    llfio::path_view v("foo", 3, llfio::path_view::not_zero_terminated);
    llfio::path_view::c_str<char, std::default_delete<char[]>, 0> zbuff(v, llfio::path_view::zero_terminated, resource);
    zbuff.reset();
    BOOST_CHECK(resource.allocated == 1);
    BOOST_CHECK(resource.deleted == 1);
    BOOST_CHECK(zbuff.memory_resource() == &resource);
  }
  // Custom STL allocator
  {
    struct custom_allocator
    {
      int sig{0};
      using value_type = char;
      int allocated{0}, deleted{0};
      value_type *allocate(size_t /*unused*/)
      {
        allocated++;
        static char buff[4];
        return buff;
      }
      void deallocate(void * /*unused*/, size_t /*unused*/) { deleted++; }
    } allocator{5};
    llfio::path_view v("foo", 3, llfio::path_view::not_zero_terminated);
    llfio::path_view::c_str<char, custom_allocator, 0> zbuff(v, llfio::path_view::zero_terminated, allocator);
    zbuff.reset();
    BOOST_CHECK(allocator.allocated == 0);  // copy must be taken
    BOOST_CHECK(allocator.deleted == 0);    // copy must be taken
    BOOST_CHECK(zbuff.allocator().allocated == 1);
    BOOST_CHECK(zbuff.allocator().deleted == 1);
    BOOST_CHECK(zbuff.allocator().sig == 5);
  }
  // Default custom allocator
  {
    struct custom_allocator
    {
      int sig{0};
      using value_type = char;
      int allocated{0}, deleted{0};
      value_type *allocate(size_t /*unused*/)
      {
        allocated++;
        static char buff[4];
        return buff;
      }
      void deallocate(void * /*unused*/, size_t /*unused*/) { deleted++; }
    } allocator{5};
    llfio::path_view v("foo", 3, llfio::path_view::not_zero_terminated);
    llfio::path_view::c_str<char, custom_allocator, 0> zbuff(v, llfio::path_view::zero_terminated);
    zbuff.reset();
    BOOST_CHECK(allocator.allocated == 0);  // copy must be taken
    BOOST_CHECK(allocator.deleted == 0);    // copy must be taken
    BOOST_CHECK(zbuff.allocator().allocated == 1);
    BOOST_CHECK(zbuff.allocator().deleted == 1);
    BOOST_CHECK(zbuff.allocator().sig == 0);  // default initialised
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, path_view, path_view, "Tests that llfio::path_view() works as expected", TestPathView())
