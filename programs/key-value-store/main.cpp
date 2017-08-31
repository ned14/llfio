/* Tests the prototype key-value store
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (3 commits)
File Created: Aug 2017


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

#include "include/key_value_store.hpp"

int main()
{
  try
  {
    key_value_store::basic_key_value_store store("teststore", 10);
    auto kvi = store.find(78);
    if(kvi)
    {
      std::cout << "Key 78 has value " << kvi.value << " and it was last updated at " << kvi.transaction_counter << std::endl;
    }
    else
    {
      std::cout << "Key 78 was not found!" << std::endl;
    }
    key_value_store::transaction tr(store);
    tr.fetch(78);
    tr.update(78, "niall");
    tr.commit();
  }
  catch(const std::exception &e)
  {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
