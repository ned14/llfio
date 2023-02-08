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

void malloc2()
{
  //! [malloc2]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Create 4Kb of anonymous shared memory. This will persist
  // until the last handle to it in the system is destructed.
  // You can fetch a path to it to give to other processes using
  // sh.current_path()
  llfio::section_handle sh = llfio::section(4096).value();

  {
    // Map it into memory, and fill it with 'a'
    llfio::mapped<char> ms1(sh);
    std::fill(ms1.begin(), ms1.end(), 'a');

    // Destructor unmaps it from memory
  }

  // Map it into memory again, verify it contains 'a'
  llfio::mapped<char> ms1(sh);
  assert(ms1[0] == 'a');

  // Map a *second view* of the same memory
  llfio::mapped<char> ms2(sh);
  assert(ms2[0] == 'a');

  // The addresses of the two maps are unique
  assert(ms1.data() != ms2.data());

  // Yet writes to one map appear in the other map
  ms2[0] = 'b';
  assert(ms1[0] == 'b');
  //! [malloc2]
}

int main()
{
  return 0;
}
