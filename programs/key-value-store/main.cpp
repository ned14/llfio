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

namespace stackoverflow
{
  namespace filesystem = AFIO_V2_NAMESPACE::filesystem;
  using string_view = AFIO_V2_NAMESPACE::string_view;
  template <class T> using optional = AFIO_V2_NAMESPACE::optional<T>;

  // Try to read first line from file at path, returning no string if file does not exist,
  // throwing exception for any other error
  optional<std::string> read_first_line(filesystem::path path)
  {
    using namespace AFIO_V2_NAMESPACE;
    using AFIO_V2_NAMESPACE::file_handle;
    using AFIO_V2_NAMESPACE::byte;
    // The result<T> is from WG21 P0762, it looks quite like an `expected<T, std::error_code>` object
    // See Outcome v2 at https://ned14.github.io/outcome/ and https://lists.boost.org/boost-announce/2017/06/0510.php

    // Open for reading the file at path using a null handle as the base
    result<file_handle> _fh = file({}, path);
    // If fh represents failure ...
    if(!_fh)
    {
      // Fetch the error code
      std::error_code ec = make_error_code(_fh.error());
      // Did we fail due to file not found?
      // It is *very* important to note that ec contains the *original* error code which could
      // be POSIX, or Win32 or NT kernel error code domains. However we can always compare,
      // via 100% C++ 11 STL, any error code to a generic error *condition* for equivalence
      // So this comparison will work as expected irrespective of original error code.
      if(ec == errc::no_such_file_or_directory)
      {
        // Return empty optional
        return {};
      }
      std::cerr << "Opening file " << path << " failed with " << ec.message() << std::endl;
    }
    // If errored, result<T>.value() throws an error code failure as if `throw std::system_error(fh.error());`
    // Otherwise unpack the value containing the valid file_handle
    file_handle fh(std::move(_fh.value()));
    // Configure the scatter buffers for the read, ideally aligned to a page boundary for DMA
    alignas(4096) byte buffer[4096];
    // There is actually a faster to type shortcut for this, but I thought best to spell it out
    file_handle::buffer_type reqs[] = {{buffer, sizeof(buffer)}};
    // Do a blocking read from offset 0 possibly filling the scatter buffers passed in
    file_handle::io_result<file_handle::buffers_type> _buffers_read = read(fh, {reqs, 0});
    if(!_buffers_read)
    {
      std::error_code ec = make_error_code(_fh.error());
      std::cerr << "Reading the file " << path << " failed with " << ec.message() << std::endl;
    }
    // Same as before, either throw any error or unpack the value returned
    file_handle::buffers_type buffers_read(_buffers_read.value());
    // Note that buffers returned by AFIO read() may be completely different to buffers submitted
    // This lets us skip unnecessary memory copying

    // Make a string view of the first buffer returned
    string_view v((const char *) buffers_read[0].data, buffers_read[0].len);
    // Sub view that view with the first line
    string_view line(v.substr(0, v.find_first_of('\n')));
    // Return a string copying the first line from the file, or all 4096 bytes read if no newline found.
    return std::string(line);
  }
}

void benchmark(key_value_store::basic_key_value_store &store, const char *desc)
{
  std::cout << "\n" << desc << ":" << std::endl;
  // Write 1M values and see how long it takes
  static std::vector<std::pair<uint64_t, std::string>> values;
  if(values.empty())
  {
    std::cout << "  Generating 1M key-value pairs ..." << std::endl;
    for(size_t n = 0; n < 1000000; n++)
    {
      std::string randomvalue = AFIO_V2_NAMESPACE::utils::random_string(1024 / 2);
      values.push_back({100 + n, randomvalue});
    }
  }
  std::cout << "  Inserting 1M key-value pairs ..." << std::endl;
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
    std::cout << "  Inserted at " << (1000000000ULL / diff) << " items per sec" << std::endl;
  }
#if 0
  {
    auto begin = std::chrono::high_resolution_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - begin).count() < 5)
      ;
  }
#endif
  std::cout << "  Retrieving 1M key-value pairs ..." << std::endl;
  {
    auto begin = std::chrono::high_resolution_clock::now();
    for(auto &i : values)
    {
      if(!store.find(i.first))
        abort();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
    std::cout << "  Fetched at " << (1000000000ULL / diff) << " items per sec" << std::endl;
  }
}

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
    {
      key_value_store::basic_key_value_store store("teststore", 10);
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
    }
    // test read only
    {
      key_value_store::basic_key_value_store store("teststore");
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
      std::error_code ec;
      AFIO_V2_NAMESPACE::filesystem::remove_all("teststore", ec);
    }
    {
      key_value_store::basic_key_value_store store("teststore", 2000000);
      benchmark(store, "no integrity, no durability, read + append");
    }
    {
      std::error_code ec;
      AFIO_V2_NAMESPACE::filesystem::remove_all("teststore", ec);
    }
    {
      key_value_store::basic_key_value_store store("teststore", 2000000, true);
      benchmark(store, "integrity, no durability, read + append");
    }
    {
      std::error_code ec;
      AFIO_V2_NAMESPACE::filesystem::remove_all("teststore", ec);
    }
    {
      key_value_store::basic_key_value_store store("teststore", 2000000);
      store.use_mmaps();
      benchmark(store, "no integrity, no durability, mmaps");
    }
    {
      std::error_code ec;
      AFIO_V2_NAMESPACE::filesystem::remove_all("teststore", ec);
    }
    {
      key_value_store::basic_key_value_store store("teststore", 2000000, true);
      store.use_mmaps();
      benchmark(store, "integrity, no durability, mmaps");
    }
    {
      std::error_code ec;
      AFIO_V2_NAMESPACE::filesystem::remove_all("teststore", ec);
    }
    {
      key_value_store::basic_key_value_store store("teststore", 2000000, true, AFIO_V2_NAMESPACE::file_handle::mode::write, AFIO_V2_NAMESPACE::file_handle::caching::reads);
      store.use_mmaps();
      benchmark(store, "integrity, durability, mmaps");
    }
  }
  catch(const std::exception &e)
  {
    std::cerr << "EXCEPTION: " << e.what() << std::endl;
    return 1;
  }
  return 0;
}
