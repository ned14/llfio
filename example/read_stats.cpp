/* Examples of LLFIO use
(C) 2018 - 2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Aug 2018


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

#include "../include/llfio.hpp"

#include <ctime>
#include <iostream>
#include <string>

auto print = [](std::chrono::system_clock::time_point ts)
{
  static time_t t;
  t = std::chrono::system_clock::to_time_t(ts);
  return [](std::ostream &os) -> std::ostream &
  {
    return os << std::ctime(&t);
  };
};

// clang-format off

void read_stats()
{
  //! [file_read_stats]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the file for read
  llfio::file_handle fh = llfio::file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // warning: default constructor does nothing. If you want all bits
  // zero, construct with `nullptr`.
  llfio::stat_t fhstat;
  fhstat.fill(
    fh        // file handle from which to fill stat_t
              // default stat_t::want is all
  ).value();
  std::cout << "The file was last modified on "
            << print(fhstat.st_mtim) << std::endl;
  
  // Note that default constructor fills with all bits one. This
  // lets you do partial fills and usually detect what wasn't filled.
  llfio::statfs_t fhfsstat;
  fhfsstat.fill(
    fh        // file handle from which to fill statfs_t
              // default statfs_t::want is all
  ).value();
  std::cout << "The file's storage has "
            << (fhfsstat.f_bavail * fhfsstat.f_bsize)
            << "bytes free." << std::endl;
  //! [file_read_stats]
}

int main()
{
  return 0;
}
