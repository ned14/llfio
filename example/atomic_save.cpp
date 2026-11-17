/* Examples of LLFIO use
(C) 2026 d3fault <d3fault@d3fault.net> (1 commit)
File Created: Oct 2026


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

// clang-format off
#ifdef _MSC_VER
#pragma warning(disable: 4706)  // assignment within conditional
#endif

void atomic_save()
{
  //! [atomic_save]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Open a transaction which will eventually replace "config.json". Nothing is written to
  // "config.json" yet, and if we never commit, it is left exactly as it was.
  llfio::atomic_file f = llfio::atomic_file::open({},  // path_handle to base directory (here, the CWD)
                                                  "config.json")
                           .value();  // If failed, throw a filesystem_error exception

  // Write the new content. Multiple sequential writes are fine; the offset is tracked for you.
  // Writes are buffered by the OS as normal, and are NOT fsync'd one by one.
  f.write({reinterpret_cast<const llfio::byte *>("{\n"), 2}).value();
  f.write({reinterpret_cast<const llfio::byte *>("  \"setting\": true\n"), 18}).value();
  f.write({reinterpret_cast<const llfio::byte *>("}\n"), 2}).value();

  // Publish. This issues exactly one durability barrier, then atomically renames the new content
  // over "config.json", then flushes the containing directory so the rename itself is durable.
  // A concurrent reader of "config.json" sees either the old content or the new content, never
  // a mixture, and a power loss after this returns will not revert the change.
  f.commit().value();  // If failed, throw a filesystem_error exception
  //! [atomic_save]
}

int main()
{
  return 0;
}