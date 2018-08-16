/* Sanity check on the single file edition
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Jun 2018


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

#if __has_include("../single-header/llfio-win.hpp") && (_HAS_CXX17 || __cplusplus >= 201700)
#ifdef _WIN32
#include "../single-header/llfio-win.hpp"
#else
#include "../single-header/llfio-posix.hpp"
#endif

int main()
{
  LLFIO_V2_NAMESPACE::mapped_file_handle v;
  (void) v;
  return 0;
}
#else
int main()
{
  return 0;
}
#endif
