/* Integration test kernel for statfs
(C) 2020 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Dec 2020


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

#include <future>
#include <cmath>

static inline void TestStatfsIosInProgress()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  llfio::file_handle h1 = llfio::file_handle::uniquely_named_file({}, llfio::file_handle::mode::write, llfio::file_handle::caching::all,
                                                                  llfio::file_handle::flag::unlink_on_first_close)
                          .value();
  llfio::file_handle h2 = llfio::file_handle::temp_file().value();
  // h1 is within our build directory's volume, h2 is within our temp volume.
  auto print_statfs = [](const llfio::file_handle &h, const llfio::statfs_t &statfs) {
    std::cout << "\nFor file " << h.current_path().value() << ":";
    std::cout << "\n fundamental filesystem block size = " << statfs.f_bsize;
    std::cout << "\n optimal transfer block size = " << statfs.f_iosize;
    std::cout << "\n total data blocks in filesystem = " << statfs.f_blocks;
    std::cout << "\n free blocks in filesystem = " << statfs.f_bfree;
    std::cout << "\n free blocks avail to non-superuser = " << statfs.f_bavail;
    std::cout << "\n total file nodes in filesystem = " << statfs.f_files;
    std::cout << "\n free nodes avail to non-superuser = " << statfs.f_ffree;
    std::cout << "\n maximum filename length = " << statfs.f_namemax;
    std::cout << "\n filesystem type name = " << statfs.f_fstypename;
    std::cout << "\n mounted filesystem = " << statfs.f_mntfromname;
    std::cout << "\n directory on which mounted = " << statfs.f_mntonname;
    std::cout << "\n i/o's currently in progress (i.e. queue depth) = " << statfs.f_iosinprogress;
    std::cout << "\n percentage of time spent doing i/o (1.0 = 100%) = " << statfs.f_ioswaittime;
    std::cout << std::endl;
  };
  llfio::statfs_t s1base, s2base;
  s1base.fill(h1).value();
  s2base.fill(h2).value();
  print_statfs(h1, s1base);
  print_statfs(h2, s2base);
  std::atomic<bool> done{false};
  auto do_io_to = [&done](llfio::file_handle &fh) {
    std::vector<llfio::byte> buffer;
    buffer.resize(4096);  // 1048576
    llfio::utils::random_fill((char *) buffer.data(), buffer.size());
    while(!done)
    {
      fh.write(0, {{buffer.data(), buffer.size()}}).value();
      fh.barrier().value();
    }
  };
  {
    auto f = std::async(std::launch::async, [&] { do_io_to(h1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    llfio::statfs_t s1load, s2load;
    s1load.fill(h1).value();
    s2load.fill(h2).value();
    done = true;
    print_statfs(h1, s1load);
    print_statfs(h2, s2load);
    // BOOST_CHECK(s1load.f_iosinprogress > s1base.f_iosinprogress);
    BOOST_CHECK(std::isnan(s1base.f_ioswaittime) || s1load.f_ioswaittime > s1base.f_ioswaittime);
    f.get();
    done = false;
  }
  {
    auto f = std::async(std::launch::async, [&] { do_io_to(h2); });
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    llfio::statfs_t s1load, s2load;
    s1load.fill(h1).value();
    s2load.fill(h2).value();
    done = true;
    print_statfs(h1, s1load);
    print_statfs(h2, s2load);
    // BOOST_CHECK(s2load.f_iosinprogress > s2base.f_iosinprogress);
    BOOST_CHECK(std::isnan(s2base.f_ioswaittime) || s2load.f_ioswaittime > s2base.f_ioswaittime);
    f.get();
    done = false;
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, statfs, iosinprogress, "Tests that llfio::statfs_t::f_iosinprogress works as expected", TestStatfsIosInProgress())
