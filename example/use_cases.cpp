/* Examples of AFIO use
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

#include "../include/afio.hpp"

#include <future>

// clang-format off

void sparse_array()
{
  //! [sparse_array]
  namespace afio = AFIO_V2_NAMESPACE;

  // Make me a 1 trillion element sparsely allocated integer array!
  afio::mapped_file_handle mfh = afio::mapped_temp_inode().value();

  // On an extents based filing system, doesn't actually allocate any physical
  // storage but does map approximately 4Tb of all bits zero data into memory
  (void) mfh.truncate(1000000000000ULL * sizeof(int));

  // Create a typed view of the one trillion integers
  afio::algorithm::mapped_span<int> one_trillion_int_array(mfh);

  // Write and read as you see fit, if you exceed physical RAM it'll be paged out
  one_trillion_int_array[0] = 5;
  one_trillion_int_array[999999999999ULL] = 6;
  //! [sparse_array]
}

#ifdef __cpp_coroutines
std::future<void> coroutine_write()
{
  //! [coroutine_write]
  namespace afio = AFIO_V2_NAMESPACE;

  // Create an asynchronous file handle
  afio::io_service service;
  afio::async_file_handle fh =
    afio::async_file(service, {}, "testfile.txt",
      afio::async_file_handle::mode::write,
      afio::async_file_handle::creation::if_needed).value();

  // Resize it to 1024 bytes
  truncate(fh, 1024).value();

  // Begin to asynchronously write "hello world" into the file at offset 0,
  // suspending execution of this coroutine until completion and then resuming
  // execution. Requires the Coroutines TS.
  alignas(4096) char buffer[] = "hello world";
  co_await co_write(fh, 0, { { buffer, sizeof(buffer) } }).value();
  //! [coroutine_write]
}
#endif

int main()
{
  return 0;
}