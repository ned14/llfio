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
#ifdef _WIN32
  SetThreadAffinityMask(GetCurrentThread(), 1);
#endif
  try
  {
    {
      std::error_code ec;
      AFIO_V2_NAMESPACE::filesystem::remove_all("teststore", ec);
    }
    key_value_store::basic_key_value_store store("teststore", 2000000);
    {
      key_value_store::transaction tr(store);
      tr.fetch(78);
      tr.update(78, "niall");
      tr.commit();
      auto kvi = store.find(78);
      if(kvi)
      {
        std::cout << "Key 78 has value " << kvi.value << " and it was last updated at " << kvi.transaction_counter << std::endl;
      }
      else
      {
        std::cerr << "FAILURE: Key 78 was not found!" << std::endl;
      }
    }
    {
      key_value_store::transaction tr(store);
      tr.fetch(79);
      tr.update(79, "douglas");
      tr.commit();
      auto kvi = store.find(79);
      if(kvi)
      {
        std::cout << "Key 79 has value " << kvi.value << " and it was last updated at " << kvi.transaction_counter << std::endl;
      }
      else
      {
        std::cerr << "FAILURE: Key 79 was not found!" << std::endl;
      }
    }
    {
      key_value_store::transaction tr(store);
      tr.fetch(78);
      tr.remove(78);
      tr.commit();
      auto kvi = store.find(78, 0);
      if(kvi)
      {
        std::cerr << "FAILURE: Revision 0 of Key 78 has value " << kvi.value << " and it was last updated at " << kvi.transaction_counter << std::endl;
      }
      else
      {
        std::cout << "Revision 0 of key 78 was not found!" << std::endl;
      }
      kvi = store.find(78, 1);
      if(kvi)
      {
        std::cout << "Revision 1 of Key 78 has value " << kvi.value << " and it was last updated at " << kvi.transaction_counter << std::endl;
      }
      else
      {
        std::cerr << "FAILURE: Revision 1Key 78 was not found!" << std::endl;
      }
    }

    // Write 1M values and see how long it takes
    std::vector<std::pair<uint64_t, std::string>> values;
    std::cout << "\nGenerating 1M key-value pairs ..." << std::endl;
    for(size_t n = 0; n < 1000000; n++)
    {
      std::string randomvalue = AFIO_V2_NAMESPACE::utils::random_string(1024 / 2);
      values.push_back({100 + n, randomvalue});
    }
    std::cout << "Inserting 1M key-value pairs ..." << std::endl;
    {
      auto begin = std::chrono::high_resolution_clock::now();
      for(size_t n = 0; n < values.size(); n += 1024)
      {
        key_value_store::transaction tr(store);
        for(size_t m = 0; m < 1024; m++)
        {
          if(n + m >= values.size())
            break;
          auto &i = values[n + m];
          tr.update_unsafe(i.first, i.second);
        }
        tr.commit();
      }
      auto end = std::chrono::high_resolution_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
      std::cout << "Inserted at " << (1000000000ULL / diff) << " items per sec" << std::endl;
    }
    std::cout << "Retrieving 1M key-value pairs ..." << std::endl;
    {
      auto begin = std::chrono::high_resolution_clock::now();
      for(auto &i : values)
      {
        if(!store.find(i.first))
          abort();
      }
      auto end = std::chrono::high_resolution_clock::now();
      auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
      std::cout << "Fetched at " << (1000000000ULL / diff) << " items per sec" << std::endl;
    }
  }
  catch(const std::exception &e)
  {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
