/* Integration testing for process handle
(C) 2016-2020 Niall Douglas <http://www.nedproductions.biz/> (13 commits)
File Created: Aug 2016


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

#define QUICKCPPLIB_BOOST_UNIT_TEST_CUSTOM_MAIN_DEFINED

#define _CRT_SECURE_NO_WARNINGS 1

#include "../test_kernel_decl.hpp"

static inline void TestProcessHandle(bool with_redirection) {
  namespace llfio = LLFIO_V2_NAMESPACE;
  std::vector<llfio::process_handle> children;
  auto &self = llfio::process_handle::current();
  auto myexepath = self.current_path().value();
  std::cout << "My process executable's path is " << myexepath << std::endl;
  auto myenv = self.environment();
  std::cout << "My process environment contains:";
  for(auto &i : *myenv)
  {
    std::cout << "\n  " << i;
  }
  std::cout << "\n" << std::endl;
  llfio::process_handle::flag flags = llfio::process_handle::flag::wait_on_close;
  if(!with_redirection)
  {
    flags |= llfio::process_handle::flag::no_redirect;
  }
  for(size_t n=0; n<4; n++)
  {
    char buffer[64];
    sprintf(buffer, "--testchild,%u", (unsigned) n);
    llfio::path_view_component arg(buffer);
    children.push_back(llfio::process_handle::launch_process(myexepath, {&arg, 1}, flags).value());
  }
  if(with_redirection)
  {
    for(size_t n = 0; n < 4; n++)
    {
      char _buffer[256];
      llfio::pipe_handle::buffer_type buffer((llfio::byte *) _buffer, sizeof(_buffer));
      children[n].in_pipe().read({{&buffer, 1}, 0}).value();
      _buffer[buffer.size()] = 0;
      std::cout << "Child process " << n << " wrote:\n   " << _buffer;
    }
  }
  for(size_t n = 0; n < 4; n++)
  {
    auto exitcode = children[n].wait().value();
    BOOST_CHECK(exitcode == (intptr_t) n + 1);
    std::cout << "The exit code of child process " << n << " was " << exitcode << std::endl;
  }
}

KERNELTEST_TEST_KERNEL(integration, llfio, process_handle, no_redirect, "Tests that llfio::process_handle without redirection works as expected", TestProcessHandle(false))
KERNELTEST_TEST_KERNEL(integration, llfio, process_handle, redirect, "Tests that llfio::process_handle with redirection works as expected", TestProcessHandle(true))

int main(int argc, char *argv[])
{
  using namespace KERNELTEST_V1_NAMESPACE;
  for(int n = 1; n < argc; n++)
  {
    if(0 == strncmp(argv[n], "--testchild,", 12))  // NOLINT
    {
      // Format is --testchild,no
      auto no = atoi(argv[n] + 12);
      std::cout << "I am child process " << no << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(3));
      return 1 + no;
    }
  }
  int result = QUICKCPPLIB_BOOST_UNIT_TEST_RUN_TESTS(argc, argv);
  return result;
}
