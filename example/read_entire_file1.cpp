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

#include <future>
#include <iostream>
#include <vector>

// clang-format off
#ifdef _MSC_VER
#pragma warning(disable: 4706)  // assignment within conditional
#endif

void read_entire_file1()
{
  //! [file_entire_file1]
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

  // Make a vector sized the current maximum extent of the file
  std::vector<llfio::byte> buffer(fh.maximum_extent().value());

  // Synchronous scatter read from file
  llfio::file_handle::size_type bytesread = llfio::read(
    fh,                                 // handle to read from
    0,                                  // offset
    {{ buffer.data(), buffer.size() }}  // Single scatter buffer of the vector 
                                        // default deadline is infinite
  ).value();                            // If failed, throw a filesystem_error exception

  // In case of racy truncation of file by third party to new length, adjust buffer to
  // bytes actually read
  buffer.resize(bytesread);
  //! [file_entire_file1]
}

int main()
{
  return 0;
}
