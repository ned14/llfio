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

void map_file()
{
  //! [map_file]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the file for read
  llfio::file_handle rfh = llfio::file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // Open the same file for atomic append
  llfio::file_handle afh = llfio::file(  //
    {},                               // path_handle to base directory
    "foo",                            // path_view to path fragment relative to base directory
    llfio::file_handle::mode::append   // open for atomic append
                                      // default creation is open existing
                                      // default caching is all
                                      // default flags is none
  ).value();                          // If failed, throw a filesystem_error exception

  // Create a section for the file of exactly the current length of the file
  llfio::section_handle sh = llfio::section(rfh).value();

  // Map the end of the file into memory with a 1Mb address reservation
  llfio::map_handle mh = llfio::map(sh, 1024 * 1024, sh.length().value() & ~4095).value();

  // Append stuff to append only handle
  llfio::write(afh,
    0,                                                      // offset is ignored for atomic append only handles
    {{ reinterpret_cast<const llfio::byte *>("hello"), 6 }}  // single gather buffer
                                                            // default deadline is infinite
  ).value();

  // Poke map to update itself into its reservation if necessary to match its backing
  // file, bringing the just appended text into the map. A no-op on many platforms.
  size_t length = mh.update_map().value();

  // Find my appended text
  for (char *p = reinterpret_cast<char *>(mh.address()); (p = (char *) memchr(p, 'h', reinterpret_cast<char *>(mh.address()) + length - p)); p++)
  {
    if (strcmp(p, "hello"))
    {
      std::cout << "Happy days!" << std::endl;
    }
  }
  //! [map_file]
}

int main()
{
  return 0;
}
