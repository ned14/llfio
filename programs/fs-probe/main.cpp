/* Probes the OS and filing system for various characteristics
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (3 commits)
File Created: Nov 2015


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

#define _CRT_SECURE_NO_WARNINGS 1

#include "../../include/llfio/llfio.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef __linux__
#define file_handle LLFIO_V2_NAMESPACE::file_handle
#endif

constexpr unsigned permute_flags_max = 4;
static const std::regex sp_preamble{"(system|storage).*"};

static LLFIO_V2_NAMESPACE::storage_profile::storage_profile profile[permute_flags_max];

#define RETCHECK(expr)                                                                                                                                                                                                                                                                                                         \
  {                                                                                                                                                                                                                                                                                                                            \
    auto ret = (expr);                                                                                                                                                                                                                                                                                                         \
    if(ret.has_error())                                                                                                                                                                                                                                                                                                        \
    {                                                                                                                                                                                                                                                                                                                          \
      std::cerr << "WARNING: Operation " #expr " failed due to '" << ret.error().message() << "'" << std::endl;                                                                                                                                                                                                                \
      abort();                                                                                                                                                                                                                                                                                                                 \
    }                                                                                                                                                                                                                                                                                                                          \
  }

int main(int argc, char *argv[])
{
  using namespace LLFIO_V2_NAMESPACE;
  using LLFIO_V2_NAMESPACE::byte;
  std::regex torun(".*");
  bool regexvalid = false;
  unsigned torunflags = (1 << permute_flags_max) - 1;
  if(argc > 1)
  {
    try
    {
      torun.assign(argv[1]);
      regexvalid = true;
    }
    catch(...)
    {
    }
    if(argc > 2)
      torunflags = atoi(argv[2]);
    if(!regexvalid)
    {
      std::cerr << "Usage: " << argv[0] << " <regex for tests to run> [<flags>]" << std::endl;
      return 1;
    }
  }

  // Force extent allocation before test begins
  auto make_testfile = [](std::string name) {
    // Create file with O_SYNC
    auto _testfile(file_handle::file({}, name, handle::mode::write, handle::creation::if_needed, handle::caching::reads));
    if(!_testfile)
    {
      std::cerr << "WARNING: Failed to create test file due to '" << _testfile.error().message() << "', failing" << std::endl;
      abort();
    }
    file_handle testfile(std::move(_testfile.value()));
    std::vector<byte> buffer(1024 * 1024 * 1024);
    RETCHECK(testfile.truncate(buffer.size()));
    file_handle::const_buffer_type _reqs[1] = {{buffer.data(), buffer.size()}};
    file_handle::io_request<file_handle::const_buffers_type> reqs(_reqs, 0);
    RETCHECK(testfile.write(reqs));
  };
  if(std::regex_match("latency:read:qd16", torun) || std::regex_match("latency:write:qd16", torun) || std::regex_match("latency:readwrite:qd4", torun))
  {
    std::cout << "Writing 17Gb of temporary test files, this will take a while ..." << std::endl;
    make_testfile("test");
    for(size_t n = 0; n < 16; n++)
      make_testfile(std::to_string(n));
  }
  else
  {
    std::cout << "Writing 1Gb of temporary test files, this will take a while ..." << std::endl;
    make_testfile("test");
  }
  // File closes, as it was opened with O_SYNC it forces extent allocation
  // Drop filesystem caches
  {
    std::cout << "Attempting to flush all modified data in system and drop all filesystem caches ..." << std::endl;
    result<void> o = utils::drop_filesystem_cache();
    if(o)
    {
      std::cout << "   Success!" << std::endl;
    }
    else
    {
      std::cout << "   WARNING: Failed due to " << o.error().message() << std::endl;
    }
  }
  // Pause as Windows still takes a while
  std::cout << "Waiting for hard drive to quieten after temp files written ..." << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(10));
  std::ofstream results("fs_probe_results.yaml", std::ios::app);
  {
    auto put_time = [](const std::tm *tmb, const char *fmt) {
      std::string buffer(256, 0);
      buffer.resize(std::strftime((char *) buffer.data(), buffer.size(), fmt, tmb));
      return buffer;
    };
    std::time_t t = std::time(nullptr);
    results << "---\ntimestamp: " << put_time(std::gmtime(&t), "%F %T %z") << "\n";
  }
  bool first = true;
  for(unsigned flags = 0; flags < permute_flags_max; flags++)
  {
    if((1 << flags) & torunflags)
    {
      handle::caching strategy = handle::caching::all;
      switch(flags)
      {
      case 1:
        strategy = handle::caching::only_metadata;  // O_DIRECT
        break;
      case 2:
        strategy = handle::caching::reads;  // O_SYNC
        break;
      case 3:
        strategy = handle::caching::none;  // O_DIRECT|O_SYNC
        break;
      }
      std::cout << "\ndirect=" << !!(flags & 1) << " sync=" << !!(flags & 2) << ":\n";
      auto _testfile(file_handle::file({}, "test", handle::mode::write, handle::creation::open_existing, strategy));
      if(!_testfile)
      {
        std::cerr << "WARNING: Failed to create test file due to '" << _testfile.error().message() << "', skipping" << std::endl;
        continue;
      }
      file_handle testfile(std::move(_testfile.value()));
      for(auto begin = std::chrono::steady_clock::now(); std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - begin).count() < 3;)
        ;
      for(auto &test : profile[flags])
      {
        if(std::regex_match(test.name, torun))
        {
          std::cout << "Running test " << test.name << " ..." << std::endl;
          auto result = test(profile[flags], testfile);
          if(result)
          {
            test.invoke([](auto &i) { std::cout << "   " << i.name << " = " << i.value << std::endl; });
          }
          else
            std::cerr << "   ERROR running test '" << test.name << "': " << print(result) << std::endl;
        }
      }
      // Write out results for this combination of flags
      if(first)
      {
        profile[flags].write(results, sp_preamble);
        first = false;
      }
      results << "direct=" << !!(flags & 1) << " sync=" << !!(flags & 2) << ":\n";
      profile[flags].write(results, sp_preamble, 4, true);
      results.flush();
    }
  }
  // Delete the test file
  auto delete_testfile = [](std::string name) {
    auto _testfile(file_handle::file({}, name, handle::mode::write));
    if(!_testfile)
    {
      std::cerr << "WARNING: Failed to open test file due to '" << _testfile.error().message() << std::endl;
      abort();
    }
    auto out = _testfile.value().unlink();
    if(!out)
    {
      std::cerr << "WARNING: Failed to unlink test file due to '" << out.error().message() << std::endl;
      abort();
    }
  };
  delete_testfile("test");
  if(std::regex_match("latency:read:qd16", torun) || std::regex_match("latency:write:qd16", torun) || std::regex_match("latency:readwrite:qd4", torun))
  {
    for(size_t n = 0; n < 16; n++)
      delete_testfile(std::to_string(n));
  }
  return 0;
}
