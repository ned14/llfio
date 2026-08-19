/* Test whether LLFIO collides with itself
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Sept 2018


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

#define ID2(a, b) a ## b
#define ID(a, b) ID2(a, b)

extern QUICKCPPLIB_SYMBOL_EXPORT LLFIO_V2_NAMESPACE::file_handle ID(make_file, LIBNO)()
{
  return {};
}
