/* Test whether two independent header only inclusions of LLFIO collide
when linked together into the same process.
(C) 2024 Niall Douglas <http://www.nedproductions.biz/>
File Created: 2024


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

#include "../../include/llfio/llfio.hpp"

// These are implemented in lib1.cpp and lib2.cpp, each of which
// independently includes the LLFIO headers in header only mode and
// builds into its own shared object. If the header only edition leaks
// any non-inline symbols, the dynamic linker will either fail to load
// both shared objects together (duplicate symbol), or silently bind
// both libraries to whichever definition loads first, which can result
// in ODR violations that are very hard to diagnose. This program links
// both shared objects into a single process and exercises both, so any
// collision shows up as a link or load time failure in CI.
extern LLFIO_V2_NAMESPACE::file_handle make_file1();
extern LLFIO_V2_NAMESPACE::file_handle make_file2();

int main()
{
  auto h1 = make_file1();
  auto h2 = make_file2();
  (void) h1;
  (void) h2;
  return 0;
}
