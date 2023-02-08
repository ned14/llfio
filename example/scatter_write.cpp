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

void scatter_write()
{
  /* WARNING: This example cannot possibly work because files opened with caching::only_metadata
  are required by the operating system to be supplied with buffers aligned to, and be a multiple of,
  the device's native block size (often 4Kb). So the gather buffers below would need to be each 4Kb
  long, and aligned to 4Kb boundaries. If you would like this example to work as-is, change the
  caching::only_metadata to caching::all.
  */
  return;
  //! [scatter_write]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the file for write, creating if needed, don't cache reads nor writes
  llfio::file_handle fh = llfio::file(  //
    {},                                         // path_handle to base directory
    "hello",                                    // path_view to path fragment relative to base directory
    llfio::file_handle::mode::write,             // write access please
    llfio::file_handle::creation::if_needed,     // create new file if needed
    llfio::file_handle::caching::only_metadata   // cache neither reads nor writes of data on this handle
                                                // default flags is none
  ).value();                                    // If failed, throw a filesystem_error exception

  // Empty file
  fh.truncate(0).value();

  // Perform gather write
  const char a[] = "hel";
  const char b[] = "l";
  const char c[] = "lo w";
  const char d[] = "orld";

  fh.write(0,                // offset
    {                        // gather list, buffers use std::byte
      { reinterpret_cast<const llfio::byte *>(a), sizeof(a) - 1 },
      { reinterpret_cast<const llfio::byte *>(b), sizeof(b) - 1 },
      { reinterpret_cast<const llfio::byte *>(c), sizeof(c) - 1 },
      { reinterpret_cast<const llfio::byte *>(d), sizeof(d) - 1 },
    }
                            // default deadline is infinite
  ).value();                // If failed, throw a filesystem_error exception

  // Explicitly close the file rather than letting the destructor do it
  fh.close().value();
  //! [scatter_write]
}

int main()
{
  return 0;
}
