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

#ifdef __has_include
#if __has_include("../../quickcpplib/include/algorithm/hash.hpp")
#include "../../quickcpplib/include/algorithm/hash.hpp"
#else
#include "quickcpplib/include/algorithm/hash.hpp"
#endif
#elif __PCPP_ALWAYS_TRUE__
#include "quickcpplib/include/algorithm/hash.hpp"
#else
#include "../../quickcpplib/include/algorithm/hash.hpp"
#endif


LLFIO_V2_NAMESPACE_EXPORT_BEGIN

fast_random_file_handle::io_result<fast_random_file_handle::buffers_type> fast_random_file_handle::read(io_request<buffers_type> reqs, deadline /* unused */) noexcept
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
  extent_type hashoffset, togo = _length - reqs.offset;
  // Fill the scatter buffers
  for(auto &buffer : reqs.buffers)
  {
    if(togo == 0)
    {
      buffer = {buffer.data(), 0};
    }
    else
    {
      for(extent_type i = 0; i < buffer.size();)
      {
        hashoffset = reqs.offset >> 4;  // add this offset to the state
        _iv[0] = hashoffset;
        auto hash = QUICKCPPLIB_NAMESPACE::algorithm::hash::fast_hash::hash((const char *) _iv, 16);
        extent_type thisblockoffset = reqs.offset - (hashoffset << 4), thisblocklen = 16 - thisblockoffset;
        if(thisblocklen > buffer.size() - i)
        {
          thisblocklen = buffer.size() - i;
        }
        if(thisblocklen > togo)
        {
          thisblocklen = togo;
        }
        memcpy(buffer.data() + i, hash.as_bytes + thisblockoffset, thisblocklen);
        reqs.offset += thisblocklen;
        i += thisblocklen;
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
