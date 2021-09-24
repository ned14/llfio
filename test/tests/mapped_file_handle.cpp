/* Integration test kernel for whether the mapped file handle works
(C) 2021 Niall Douglas <http://www.nedproductions.biz/> (2 commits)
File Created: Sept 2021


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

#include "../test_kernel_decl.hpp"

static constexpr size_t DATA_SIZE = 1024 * 1024;

template <class T, class F> void runtest(T &v, F &&write)
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng rand;
  std::vector<llfio::file_handle::const_buffer_type> buffers;
  std::vector<char> bytes(4096);
  for(char n = ' '; n < 'z'; n++)
  {
    memset(bytes.data(), n, bytes.size());
    auto i = rand();
    auto offset = i % DATA_SIZE;
    auto bufferslen = (i >> 24);
    buffers.resize(bufferslen);
    size_t count = 0;
    uint32_t b = 0;
    for(; b < bufferslen && count < bytes.size() - 15; b++)
    {
      buffers[b] = llfio::file_handle::const_buffer_type((llfio::byte *) bytes.data() + count, rand() & 15);
      count += buffers[b].size();
    }
    buffers.resize(b);
    llfio::file_handle::io_request<llfio::file_handle::const_buffers_type> req(buffers, offset);
    write(v, req);
  }
};


static inline void TestMappedFileHandle()
{
  namespace llfio = LLFIO_V2_NAMESPACE;
  std::vector<llfio::byte> reference(DATA_SIZE);
  runtest(reference, [](std::vector<llfio::byte> &cont, llfio::file_handle::io_request<llfio::file_handle::const_buffers_type> req) {
    for(auto &b : req.buffers)
    {
      memcpy(cont.data() + req.offset, b.data(), b.size());
      req.offset += b.size();
    }
  });
  auto mf1 = llfio::mapped_file_handle::mapped_temp_inode(DATA_SIZE).value();
  mf1.truncate(DATA_SIZE).value();
  runtest(mf1, [](llfio::mapped_file_handle &cont, llfio::file_handle::io_request<llfio::file_handle::const_buffers_type> req) { cont.write(req).value(); });
  BOOST_CHECK(0 == memcmp(mf1.address(), reference.data(), DATA_SIZE));
  auto mf2 =
  llfio::mapped_file_handle::mapped_temp_inode(DATA_SIZE, llfio::path_discovery::storage_backed_temporary_files_directory(), llfio::file_handle::mode::write,
                                                   llfio::file_handle::flag::none, llfio::section_handle::flag::write_via_syscall)
             .value();
  mf2.truncate(DATA_SIZE).value();
  runtest(mf2, [](llfio::mapped_file_handle &cont, llfio::file_handle::io_request<llfio::file_handle::const_buffers_type> req) {
    cont.write(req).value(); 
    });
  BOOST_CHECK(0 == memcmp(mf2.address(), reference.data(), DATA_SIZE));
}

KERNELTEST_TEST_KERNEL(integration, llfio, mapped_file_handle, cache, "Tests that the mapped_file_handle works as expected", TestMappedFileHandle())
