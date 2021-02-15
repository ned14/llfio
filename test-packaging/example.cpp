/* Examples of LLFIO use
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

#include <llfio.hpp>

#include <iostream>

int main()
{
  namespace llfio = LLFIO_V2_NAMESPACE;

  auto r = []() -> llfio::result<int> {
    OUTCOME_TRY(auto &&fh, llfio::file_handle::temp_file());
    static const char *buffers[] = { "He", "llo", " world" };
    OUTCOME_TRY(fh.write(0, { { (const llfio::byte *) buffers[0], 2 }, { (const llfio::byte *) buffers[1], 3 }, { (const llfio::byte *) buffers[2], 6 } } ));
    llfio::byte buffer[64];
    OUTCOME_TRY(auto &&read, fh.read(0, { {buffer, sizeof(buffer)} }));
    if(read != 11)
    {
      std::cerr << "FAILURE: Did not read 11 bytes!" << std::endl;
      return 1;
    }
    if(0 != memcmp(buffer, "Hello world", 11))
    {
      std::cerr << "FAILURE: Did not read what was written!" << std::endl;
      return 1;
    }
    return 0;
  }();
  if(!r)
  {
    std::cerr << "ERROR: " << r.error().message().c_str() << std::endl;
    return 1;
  }
  return r.value();
}
