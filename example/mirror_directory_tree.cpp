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

#include <iostream>
#include <vector>

// clang-format off

void read_directory()
{
  //! [read_directory]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open the directory for read
  llfio::directory_handle dh = llfio::directory(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // Read up to ten directory_entry
  std::vector<llfio::directory_handle::buffer_type> buffer(10);

  // Very similar to reading from a file handle, we need
  // to achieve a single snapshot read to be race free.
  llfio::directory_handle::buffers_type entries(buffer);
  for(;;)
  {
    entries = dh.read(
      {std::move(entries)}   // buffers to fill
    ).value();               // If failed, throw a filesystem_error exception

    // If there were fewer entries in the directory than buffers
    // passed in, we are done.
    if(entries.done())
    {
      break;
    }
    // Otherwise double the size of the buffer
    buffer.resize(buffer.size() << 1);
    // Set the next read attempt to use the newly enlarged buffer.
    // buffers_type may cache internally reusable state depending
    // on platform, to efficiently reuse that state pass in the
    // old entries by rvalue ref.
    entries = { buffer, std::move(entries) };
  }

  std::cout << "The directory " << dh.current_path().value() << " has entries:";
  for(auto &entry : entries)
  {
    std::cout << "\n   " << entry.leafname;
    // Different platforms fill in different fields of the stat_t
    // in directory_entry as some fields are fillable free of cost
    // during directory enumeration. What the platform provides for
    // free is given by metadata().
    if(!(entries.metadata() & llfio::stat_t::want::size))
    {
      llfio::file_handle fh = llfio::file(
        dh,              // base directory for open
        entry.leafname,  // leaf name within that base directory
        // don't need privs above minimum to read attributes
        // (this is significantly faster than opening a file
        // for read on some platforms)
        llfio::file_handle::mode::attr_read
      ).value();
      // Do the minimum work to fill in only the st_size member
      // (other fields may get also filled if it is free of cost)
      entry.stat.fill(fh, llfio::stat_t::want::size).value();
    }
    std::cout << " maximum extent: " << entry.stat.st_size;
  }
  std::cout << std::endl;
  //! [read_directory]
}

int main()
{
  return 0;
}
