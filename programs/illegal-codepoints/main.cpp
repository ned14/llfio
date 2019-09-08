/* Test what codepoints are illegal in filenames
(C) 2019 Niall Douglas <http://www.nedproductions.biz/> (6 commits)
File Created: Sept 2019


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

/*
For Linux:

'/' is rejected. NUL causes early termination.

ls testdir
 a     aFz   aQz   'a>z'  'a\z'          'a'$'\250''z'  'a'$'\223''z'  'a'$'\371''z'  'a'$'\241''z'  'a'$'\271''z'  'a'$'\341''z'  'a'$'\027''z'
 a0z   agz   arz   'a|z'  'a&z'          'a'$'\215''z'  'a'$'\307''z'  'a'$'\337''z'  'a'$'\306''z'  'a'$'\240''z'  'a'$'\002''z'  'a'$'\030''z'
 a1z   aGz   aRz   'a z'   a#z           'a'$'\252''z'  'a'$'\224''z'  'a'$'\236''z'  'a'$'\373''z'  'a'$'\356''z'  'a'$'\003''z'  'a'$'\031''z'
 a2z   ahz   asz    a_z    a%z           'a'$'\275''z'  'a'$'\334''z'  'a'$'\270''z'  'a'$'\205''z'  'a'$'\213''z'  'a'$'\004''z'  'a'$'\032''z'
 a3z   aHz   aSz    a-z    a+z           'a'$'\246''z'  'a'$'\367''z'  'a'$'\202''z'  'a'$'\325''z'  'a'$'\247''z'  'a'$'\005''z'  'a'$'\033''z'
 a4z   aiz   atz    a,z   'a'$'\312''z'  'a'$'\317''z'  'a'$'\261''z'  'a'$'\343''z'  'a'$'\231''z'  'a'$'\255''z'  'a'$'\006''z'  'a'$'\034''z'
 a5z   aIz   aTz   'a;z'  'a'$'\360''z'  'a'$'\310''z'  'a'$'\277''z'  'a'$'\364''z'  'a'$'\245''z'  'a'$'\253''z'  'a'$'\a''z'    'a'$'\035''z'
 a6z   ajz   auz    a:z   'a'$'\353''z'  'a'$'\357''z'  'a'$'\233''z'  'a'$'\260''z'  'a'$'\001''z'  'a'$'\347''z'  'a'$'\b''z'    'a'$'\036''z'
 a7z   aJz   aUz   'a!z'  'a'$'\340''z'  'a'$'\220''z'  'a'$'\301''z'  'a'$'\321''z'  'a'$'\200''z'  'a'$'\331''z'  'a'$'\t''z'    'a'$'\037''z'
 a8z   akz   avz   'a?z'  'a'$'\227''z'  'a'$'\244''z'  'a'$'\323''z'  'a'$'\256''z'  'a'$'\201''z'  'a'$'\226''z'  'a'$'\n''z'    'a'$'\177''z'
 a9z   aKz   aVz    a.z   'a'$'\330''z'  'a'$'\333''z'  'a'$'\352''z'  'a'$'\350''z'  'a'$'\214''z'  'a'$'\327''z'  'a'$'\v''z'     azz
 aaz   alz   awz   "a'z"  'a'$'\216''z'  'a'$'\222''z'  'a'$'\254''z'  'a'$'\336''z'  'a'$'\314''z'  'a'$'\217''z'  'a'$'\f''z'     aZz
 aAz   aLz   aWz   'a"z'  'a'$'\272''z'  'a'$'\370''z'  'a'$'\234''z'  'a'$'\266''z'  'a'$'\211''z'  'a'$'\300''z'  'a'$'\r''z'
 abz   amz   axz   'a(z'  'a'$'\267''z'  'a'$'\313''z'  'a'$'\355''z'  'a'$'\363''z'  'a'$'\366''z'  'a'$'\251''z'  'a'$'\016''z'
 aBz   aMz   aXz   'a)z'  'a'$'\232''z'  'a'$'\324''z'  'a'$'\376''z'  'a'$'\257''z'  'a'$'\365''z'  'a'$'\242''z'  'a'$'\017''z'
 acz   anz   ayz   'a[z'  'a'$'\345''z'  'a'$'\303''z'  'a'$'\315''z'  'a'$'\304''z'  'a'$'\374''z'  'a'$'\302''z'  'a'$'\020''z'
 aCz   aNz   aYz    a]z   'a'$'\204''z'  'a'$'\361''z'  'a'$'\322''z'  'a'$'\273''z'  'a'$'\274''z'  'a'$'\316''z'  'a'$'\021''z'
 adz   aoz  'a`z'   a{z   'a'$'\237''z'  'a'$'\372''z'  'a'$'\311''z'  'a'$'\332''z'  'a'$'\320''z'  'a'$'\335''z'  'a'$'\022''z'
 aDz   aOz  'a^z'   a}z   'a'$'\276''z'  'a'$'\346''z'  'a'$'\210''z'  'a'$'\263''z'  'a'$'\265''z'  'a'$'\351''z'  'a'$'\023''z'
 aez   apz   a~z    a@z   'a'$'\230''z'  'a'$'\305''z'  'a'$'\342''z'  'a'$'\206''z'  'a'$'\203''z'  'a'$'\221''z'  'a'$'\024''z'
 aEz   aPz  'a<z'  'a$z'  'a'$'\375''z'  'a'$'\243''z'  'a'$'\362''z'  'a'$'\235''z'  'a'$'\354''z'  'a'$'\225''z'  'a'$'\025''z'
 afz   aqz  'a=z'  'a*z'  'a'$'\262''z'  'a'$'\212''z'  'a'$'\344''z'  'a'$'\207''z'  'a'$'\326''z'  'a'$'\264''z'  'a'$'\026''z'


*/

#include "../../include/llfio/llfio.hpp"

#include <iostream>

namespace llfio = LLFIO_V2_NAMESPACE;

int main()
{
  auto print_codepoint = [](auto v) -> std::string {
    if(v < 32 || v >= 127)
      return "non-ascii";
    char x = (char) v;
    return {&x, 1};
  };
  auto testdir = llfio::directory({}, "testdir", llfio::directory_handle::mode::write, llfio::directory_handle::creation::if_needed).value();
  const size_t max_codepoint = (sizeof(llfio::filesystem::path::value_type) > 1) ? 65535 : 255;
  std::cout << "Creating files with codepoints 0 - " << max_codepoint << " ...\n" << std::endl;
  for(size_t n = 0; n < max_codepoint; n++)
  {
    llfio::filesystem::path::value_type buffer[8];
    buffer[0] = 'a';
    buffer[1] = n;
    buffer[2] = 'z';
    buffer[3] = 0;
    llfio::path_view leaf(buffer, 3);
    auto h = llfio::file(testdir, leaf, llfio::file_handle::mode::write, llfio::file_handle::creation::if_needed);
    if(!h)
    {
      std::cout << "Failed to create file with codepoints " << n << " (" << print_codepoint(n) << ") due to: " << h.error() << std::endl;
    }
  }
  return 0;
}
