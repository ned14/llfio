/* Integration test kernel for whether current path works
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

template <class FileHandleType, class DirectoryHandleType> static inline void TestHandleCurrentPath()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  {
    std::error_code ec;
    llfio::filesystem::current_path(llfio::filesystem::temp_directory_path());
    llfio::filesystem::remove_all("tempfile", ec);
    llfio::filesystem::remove_all("tempfile2", ec);
    llfio::filesystem::remove_all("tempfile3", ec);
    llfio::filesystem::remove_all("tempfile4", ec);
    llfio::filesystem::remove_all("tempdir", ec);
    llfio::filesystem::remove_all("tempdir2", ec);
    llfio::filesystem::remove_all("tempdir3", ec);
  }  // namespace ;
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
  llfio::path_handle null_path_handle;
  FileHandleType h1 = llfio::construct<FileHandleType>{null_path_handle, "tempfile", llfio::file_handle::mode::write, llfio::file_handle::creation::if_needed, llfio::file_handle::caching::temporary, llfio::file_handle::flag::none}().value();     // NOLINT
  DirectoryHandleType h2 = llfio::construct<DirectoryHandleType>{null_path_handle, "tempdir", llfio::file_handle::mode::write, llfio::file_handle::creation::if_needed, llfio::file_handle::caching::all, llfio::file_handle::flag::none}().value();  // NOLINT
#ifdef __clang__
#pragma clang diagnostic pop
#endif

  {
    llfio::stat_t s(nullptr);
    auto print = [](const llfio::stat_t &s) {
      auto print_dt = [](const std::chrono::system_clock::time_point &tp) {
        time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::ctime(&tt);
        ss.seekp(-1, ss.cur);
        ss << "." << std::chrono::duration_cast<std::chrono::nanoseconds>(tp - std::chrono::system_clock::from_time_t(tt)).count();
        return ss.str();
      };
      std::cout << "Handle has stat_t:";
      std::cout << "\n           dev : " << s.st_dev;
      std::cout << "\n           ino : " << s.st_ino;
      std::cout << "\n          type : " << (int) s.st_type;
      std::cout << "\n         nlink : " << s.st_nlink;
      std::cout << "\n          atim : " << print_dt(s.st_atim);
      std::cout << "\n          mtim : " << print_dt(s.st_mtim);
      std::cout << "\n          ctim : " << print_dt(s.st_ctim);
      std::cout << "\n          size : " << s.st_size;
      std::cout << "\n     allocated : " << s.st_allocated;
      std::cout << "\n        blocks : " << s.st_blocks;
      std::cout << "\n       blksize : " << s.st_blksize;
      std::cout << "\n         flags : " << s.st_flags;
      std::cout << "\n           gen : " << s.st_gen;
      std::cout << "\n      birthtim : " << print_dt(s.st_birthtim);
      std::cout << "\n        sparse : " << s.st_sparse;
      std::cout << "\n    compressed : " << s.st_compressed;
      std::cout << "\n reparse_point : " << s.st_reparse_point;
      std::cout << "\n" << std::endl;
    };
    s.fill(h1).value();
    print(s);
  }
  std::cout << "\ncurrent_path() works at all:\n";
  {
    auto h1path = h1.current_path();
    BOOST_CHECK(h1path);
    if(!h1path)
    {
      std::cerr << "   Getting the current path of a file FAILED due to " << h1path.error().message().c_str() << std::endl;
    }
    else if(h1path.value().empty())
    {
      BOOST_CHECK(!h1path.value().empty());
      std::cerr << "   Getting the current path of a file FAILED due to the returned path being empty" << std::endl;
    }
    else
    {
      std::cout << "   The path of the file is " << h1path.value() << std::endl;
    }

    auto h2path = h2.current_path();
    BOOST_CHECK(h2path);
    if(!h2path)
    {
      std::cerr << "   Getting the current path of a directory FAILED due to " << h2path.error().message().c_str() << std::endl;
    }
    else if(h2path.value().empty())
    {
      BOOST_CHECK(!h2path.value().empty());
      std::cerr << "   Getting the current path of a directory FAILED due to the returned path being empty" << std::endl;
    }
    else
    {
      std::cout << "   The path of the directory is " << h2path.value() << std::endl;
    }
  }

  // Atomic relink
  std::cout << "\ncurrent_path() after atomic relink:\n";
  h1.relink({}, "tempfile2", true).value();
  h2.relink({}, "tempdir2", true).value();

  {
    auto h1path = h1.current_path();
    BOOST_CHECK(h1path);
    if(!h1path)
    {
      std::cerr << "   Getting the current path of a file FAILED due to " << h1path.error().message().c_str() << std::endl;
    }
    else if(h1path.value().empty())
    {
      BOOST_CHECK(!h1path.value().empty());
      std::cerr << "   Getting the current path of a file FAILED due to the returned path being empty" << std::endl;
    }
    else if(h1path.value().filename() != "tempfile2")
    {
      BOOST_CHECK(h1path.value().filename() == "tempfile2");
      std::cerr << "   Getting the current path of a file FAILED due to the wrong path being returned: " << h1path.value() << std::endl;
    }
    else
    {
      std::cout << "   The path of the file is " << h1path.value() << std::endl;
    }

    auto h2path = h2.current_path();
    BOOST_CHECK(h2path);
    if(!h2path)
    {
      std::cerr << "   Getting the current path of a directory FAILED due to " << h2path.error().message().c_str() << std::endl;
    }
    else if(h2path.value().empty())
    {
      BOOST_CHECK(!h2path.value().empty());
      std::cerr << "   Getting the current path of a directory FAILED due to the returned path being empty" << std::endl;
    }
    else if(h2path.value().filename() != "tempdir2")
    {
      BOOST_CHECK(h2path.value().filename() == "tempdir2");
      std::cerr << "   Getting the current path of a directory FAILED due to the wrong path being returned: " << h2path.value() << std::endl;
    }
    else
    {
      std::cout << "   The path of the directory is " << h2path.value() << std::endl;
    }
  }

  // Non-atomic relink
  std::cout << "\ncurrent_path() after non-atomic relink:\n";
  h1.relink({}, "tempfile3", false).value();

  {
    auto h1path = h1.current_path();
    BOOST_CHECK(h1path);
    if(!h1path)
    {
      std::cerr << "   Getting the current path of a file FAILED due to " << h1path.error().message().c_str() << std::endl;
    }
    else if(h1path.value().empty())
    {
      BOOST_CHECK(!h1path.value().empty());
      std::cerr << "   Getting the current path of a file FAILED due to the returned path being empty" << std::endl;
    }
    else if(h1path.value().filename() != "tempfile3")
    {
      BOOST_CHECK(h1path.value().filename() == "tempfile3");
      std::cerr << "   Getting the current path of a file FAILED due to the wrong path being returned: " << h1path.value() << std::endl;
    }
    else
    {
      std::cout << "   The path of the file is " << h1path.value() << std::endl;
    }
  }
  {
    llfio::stat_t s(nullptr);
    auto print = [](const llfio::stat_t &s) {
      auto print_dt = [](const std::chrono::system_clock::time_point &tp) {
        time_t tt = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::ctime(&tt);
        ss.seekp(-1, ss.cur);
        ss << "." << std::chrono::duration_cast<std::chrono::nanoseconds>(tp - std::chrono::system_clock::from_time_t(tt)).count();
        return ss.str();
      };
      std::cout << "Handle has stat_t:";
      std::cout << "\n           dev : " << s.st_dev;
      std::cout << "\n           ino : " << s.st_ino;
      std::cout << "\n          type : " << (int) s.st_type;
      std::cout << "\n         nlink : " << s.st_nlink;
      std::cout << "\n          atim : " << print_dt(s.st_atim);
      std::cout << "\n          mtim : " << print_dt(s.st_mtim);
      std::cout << "\n          ctim : " << print_dt(s.st_ctim);
      std::cout << "\n          size : " << s.st_size;
      std::cout << "\n     allocated : " << s.st_allocated;
      std::cout << "\n        blocks : " << s.st_blocks;
      std::cout << "\n       blksize : " << s.st_blksize;
      std::cout << "\n         flags : " << s.st_flags;
      std::cout << "\n           gen : " << s.st_gen;
      std::cout << "\n      birthtim : " << print_dt(s.st_birthtim);
      std::cout << "\n        sparse : " << s.st_sparse;
      std::cout << "\n    compressed : " << s.st_compressed;
      std::cout << "\n reparse_point : " << s.st_reparse_point;
      std::cout << "\n" << std::endl;
    };
    s.fill(h1).value();
    print(s);
    BOOST_CHECK(s.st_nlink == 1);
  }

  // Non-atomic relink file into dir
  std::cout << "\ncurrent_path() after relink into directory:\n";
  h1.relink(h2, "tempfile4", false).value();

  {
    auto h1path = h1.current_path();
    BOOST_CHECK(h1path);
    if(!h1path)
    {
      std::cerr << "   Getting the current path of a file FAILED due to " << h1path.error().message().c_str() << std::endl;
    }
    else if(h1path.value().empty())
    {
      BOOST_CHECK(!h1path.value().empty());
      std::cerr << "   Getting the current path of a file FAILED due to the returned path being empty" << std::endl;
    }
    else if(h1path.value().filename() != "tempfile4")
    {
      BOOST_CHECK(h1path.value().filename() == "tempfile4");
      std::cerr << "   Getting the current path of a file FAILED due to the wrong path being returned: " << h1path.value() << std::endl;
    }
    else
    {
      std::cout << "   The path of the file is " << h1path.value() << std::endl;
    }
  }

  h1.unlink().value();
  h1.close().value();
  h2.unlink().value();
}

KERNELTEST_TEST_KERNEL(integration, llfio, current_path, handle, "Tests that llfio::handle::current_path() works as expected", TestHandleCurrentPath<LLFIO_V2_NAMESPACE::file_handle, LLFIO_V2_NAMESPACE::directory_handle>())
KERNELTEST_TEST_KERNEL(integration, llfio, current_path, cached_parent_handle_adapter, "Tests that llfio::cached_parent_handle_adapter::current_path() works as expected",
                       TestHandleCurrentPath<LLFIO_V2_NAMESPACE::algorithm::cached_parent_handle_adapter<LLFIO_V2_NAMESPACE::file_handle>, LLFIO_V2_NAMESPACE::algorithm::cached_parent_handle_adapter<LLFIO_V2_NAMESPACE::directory_handle>>())
