/* A handle to a pseudo random file
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: Sept 2018


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

#include "../../fast_random_file_handle.hpp"

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

fast_random_file_handle::io_result<fast_random_file_handle::buffers_type> fast_random_file_handle::_do_read(io_request<buffers_type> reqs, deadline /* unused */) noexcept
{
  if(reqs.offset >= _length)
  {
    // Return null read
    for(auto &buffer : reqs.buffers)
    {
      buffer = {buffer.data(), 0};
    }
    return std::move(reqs.buffers);
  }
  extent_type togo = _length - reqs.offset;
  // Fill the scatter buffers
  for(auto &buffer : reqs.buffers)
  {
    if(togo == 0)
    {
      buffer = {buffer.data(), 0};
    }
    else
    {
      for(size_type i = 0; i < buffer.size();)
      {
        // How much can we do at once?
        auto hashoffset = reqs.offset >> 2;                      // place this offset into the state
        auto thisblockoffset = reqs.offset - (hashoffset << 2);  // offset into our buffer due to request offset misalignment
        extent_type thisblocklen = 64;                           // maximum possible block this iteration
        if(thisblocklen > buffer.size() - i)
        {
          thisblocklen = buffer.size() - i;
        }
        if(thisblocklen > togo)
        {
          thisblocklen = togo;
        }

        struct _p
        {
          uint32_t hash[16];
        } blk, *p = &blk;
        // If not copying partial buffers
        if(thisblockoffset == 0)
        {
          // If destination is on 4 byte multiple, we can write direct
          if((((uintptr_t) buffer.data() + i) & 3) == 0)
            p = (_p *) (buffer.data() + i);
        }
        if(thisblocklen == 64)
        {
          auto &hash = p->hash;
          prng __prngs[16] = {_prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng, _prng};
          hash[0] = __prngs[0](hashoffset + 0);
          hash[1] = __prngs[1](hashoffset + 1);
          hash[2] = __prngs[2](hashoffset + 2);
          hash[3] = __prngs[3](hashoffset + 3);
          hash[4] = __prngs[4](hashoffset + 4);
          hash[5] = __prngs[5](hashoffset + 5);
          hash[6] = __prngs[6](hashoffset + 6);
          hash[7] = __prngs[7](hashoffset + 7);
          hash[8] = __prngs[8](hashoffset + 8);
          hash[9] = __prngs[9](hashoffset + 9);
          hash[10] = __prngs[10](hashoffset + 10);
          hash[11] = __prngs[11](hashoffset + 11);
          hash[12] = __prngs[12](hashoffset + 12);
          hash[13] = __prngs[13](hashoffset + 13);
          hash[14] = __prngs[14](hashoffset + 14);
          hash[15] = __prngs[15](hashoffset + 15);
          if(thisblocklen > 64 - thisblockoffset)
          {
            thisblocklen = 64 - thisblockoffset;
          }
        }
        else if(thisblocklen == 16)
        {
          auto &hash = p->hash;
          prng __prngs[4] = {_prng, _prng, _prng, _prng};
          hash[0] = __prngs[0](hashoffset + 0);
          hash[1] = __prngs[1](hashoffset + 1);
          hash[2] = __prngs[2](hashoffset + 2);
          hash[3] = __prngs[3](hashoffset + 3);
          if(thisblocklen > 16 - thisblockoffset)
          {
            thisblocklen = 16 - thisblockoffset;
          }
        }
        else
        {
          auto &hash = p->hash;
          auto __prng(_prng);
          hash[0] = __prng(hashoffset);
          if(thisblocklen > 4 - thisblockoffset)
          {
            thisblocklen = 4 - thisblockoffset;
          }
        }
        if(p == &blk)
        {
          memcpy(buffer.data() + i, ((const char *) p) + thisblockoffset, (size_type) thisblocklen);
        }
        reqs.offset += thisblocklen;
        i += (size_type) thisblocklen;
        togo -= thisblocklen;
        if(togo == 0)
        {
          buffer = {buffer.data(), i};
          break;
        }
      }
    }
  }
  return std::move(reqs.buffers);
}

LLFIO_V2_NAMESPACE_END
