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
#include <vector>

// clang-format off

void read_entire_file1()
{
  //! [file_entire_file1]
  namespace afio = AFIO_V2_NAMESPACE;

  // Open the file for read
  afio::file_handle fh = afio::file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // Make a vector sized the current length of the file
  std::vector<char> buffer(fh.length().value());

  // Synchronous scatter read from file
  afio::file_handle::buffers_type filled = afio::read(
    fh,                                 // handle to read from
    0,                                  // offset
    {{ buffer.data(), buffer.size() }}  // Single scatter buffer of the vector 
                                        // default deadline is infinite
  ).value();                            // If failed, throw a filesystem_error exception

  // In case of racy truncation of file by third party to new length, adjust buffer to
  // bytes actually read
  buffer.resize(filled[0].len);
  //! [file_entire_file1]
}

void scatter_write()
{
  //! [scatter_write]
  namespace afio = AFIO_V2_NAMESPACE;

  // Open the file for write, creating if needed, don't cache reads nor writes
  afio::file_handle fh = afio::file(  //
    {},                                         // path_handle to base directory
    "hello",                                    // path_view to path fragment relative to base directory
    afio::file_handle::mode::write,             // write access please
    afio::file_handle::creation::if_needed,     // create new file if needed
    afio::file_handle::caching::only_metadata   // cache neither reads nor writes of data on this handle
                                                // default flags is none
  ).value();                                    // If failed, throw a filesystem_error exception

  // Empty file
  fh.truncate(0).value();

  // Perform gather write
  const char a[] = "hel";
  const char b[] = "l";
  const char c[] = "lo w";
  const char d[] = "orld";

  fh.write(0,                // offset
    {                        // gather list
      { a, sizeof(a) - 1 },
      { b, sizeof(b) - 1 },
      { c, sizeof(c) - 1 },
      { d, sizeof(d) - 1 },
    }
                            // default deadline is infinite
  ).value();                // If failed, throw a filesystem_error exception

  // Explicitly close the file rather than letting the destructor do it
  fh.close().value();
  //! [scatter_write]
}

void map_file()
{
  //! [map_file]
  namespace afio = AFIO_V2_NAMESPACE;

  // Open the file for read
  afio::file_handle rfh = afio::file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  // Open the same file for atomic append
  afio::file_handle afh = afio::file(  //
    {},                               // path_handle to base directory
    "foo",                            // path_view to path fragment relative to base directory
    afio::file_handle::mode::append   // open for atomic append
                                      // default creation is open existing
                                      // default caching is all
                                      // default flags is none
  ).value();                          // If failed, throw a filesystem_error exception

  // Create a section for the file of exactly the current length of the file
  afio::section_handle sh = afio::section(rfh).value();

  // Map the end of the file into memory with a 1Mb address reservation
  afio::map_handle mh = afio::map(sh, 1024 * 1024, sh.length().value() & ~4095).value();

  // Append stuff to append only handle
  afio::write(afh,
    0,             // offset is ignored for atomic append only handles
    {{ "hello" }}  // single gather buffer
                   // default deadline is infinite
  ).value();

  // Poke map to update itself into its reservation if necessary to match its backing
  // file, bringing the just appended text into the map. A no-op on many platforms.
  size_t length = mh.update_map().value();

  // Find my appended text
  for (char *p = mh.address(); (p = (char *) memchr(p, 'h', mh.address() + length - p)); p++)
  {
    if (strcmp(p, "hello"))
    {
      std::cout << "Happy days!" << std::endl;
    }
  }
  //! [map_file]
}

void mapped_file()
{
  //! [mapped_file]
  namespace afio = AFIO_V2_NAMESPACE;

  // Open the mapped file for read
  afio::mapped_file_handle mh = afio::mapped_file(  //
    {},       // path_handle to base directory
    "foo"     // path_view to path fragment relative to base directory
              // default mode is read only
              // default creation is open existing
              // default caching is all
              // default flags is none
  ).value();  // If failed, throw a filesystem_error exception

  auto length = mh.length().value();

  // Find my text
  for (char *p = mh.address(); (p = (char *)memchr(p, 'h', mh.address() + length - p)); p++)
  {
    if (strcmp(p, "hello"))
    {
      std::cout << "Happy days!" << std::endl;
    }
  }
  //! [mapped_file]
}

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