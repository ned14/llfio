/* Examples of LLFIO use
(C) 2018 - 2022 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
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

#include "../include/llfio.hpp"

#include <future>
#include <iostream>
#include <vector>

// clang-format off
#ifdef _MSC_VER
#pragma warning(disable: 4706)  // assignment within conditional
#endif

void malloc1()
{
  //! [malloc1]
  namespace llfio = LLFIO_V2_NAMESPACE;

  // Call whatever the equivalent to mmap() is on this platform to fetch
  // new private memory backed by the swap file. This will be the system
  // all bits zero page mapped into each page of the allocation. Only on
  // first write will a page fault allocate a real zeroed page for that
  // page.
  llfio::map_handle mh = llfio::map(4096).value();

  // Fill the newly allocated memory with 'a' C style. For each first write
  // to a page, it will be page faulted into a private page by the kernel.
  llfio::byte *p = mh.address();
  size_t len = mh.length();
  // map_handle::address() returns indeterminate bytes, so you need to bless
  // them into existence before use
  new(p) llfio::byte[len];
  memset(p, 'a', len);

  // Tell the kernel to throw away the contents of any whole pages
  // by resetting them to the system all zeros page. These pages
  // will be faulted into existence on first write.
  mh.zero_memory({ mh.address(), mh.length() }).value();

  // Do not write these pages to the swap file (flip dirty bit to false)
  mh.do_not_store({mh.address(), mh.length()}).value();

  // Fill the memory with 'b' C++ style, probably faulting new pages into existence
  llfio::attached<char> p2(mh);
  std::fill(p2.begin(), p2.end(), 'b');

  // Kick the contents of the memory out to the swap file so it is no longer cached in RAM
  // This also remaps the memory to reserved address space.
  mh.decommit({mh.address(), mh.length()}).value();

  // Map the swap file stored edition back into memory, it will fault on
  // first read to do the load back into the kernel page cache.
  mh.commit({ mh.address(), mh.length() }).value();

  // And rather than wait until first page fault read, tell the system we are going to
  // use this region soon. Most systems will begin an asynchronous population of the
  // kernel page cache immediately.
  llfio::map_handle::buffer_type pf[] = { { mh.address(), mh.length() } };
  mh.prefetch(pf).value();


  // You can actually save yourself some time and skip manually creating map handles.
  // Just construct a mapped_span directly, this creates an internal map_handle instance,
  // so memory is released when the span is destroyed
  llfio::mapped<float> f(1000);  // 1000 floats, allocated used mmap()
  std::fill(f.begin(), f.end(), 1.23f);
  //! [malloc1]
}

int main()
{
  return 0;
}
