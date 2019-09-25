/* Examples from the P1031 Low level file i/o Technical Specification
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

#include "../include/llfio.hpp"

using namespace LLFIO_V2_NAMESPACE;
using namespace std;

#define throws(x)
#define VALUE .value()

inline io_handle::buffers_type read_all(io_handle &h, io_handle::io_request<io_handle::buffers_type> reqs, deadline d = deadline()) throws(file_io_error)
{
  // Record beginning if deadline is specified
  chrono::steady_clock::time_point began_steady;
  if(d && d.steady)
    began_steady = chrono::steady_clock::now();

  // Take copy of input buffers onto stack, and set output buffers to buffers supplied
  auto *input_buffers_mem = reinterpret_cast<io_handle::buffer_type *>(alloca(reqs.buffers.size() * sizeof(io_handle::buffer_type)));
  auto *input_buffers_sizes = reinterpret_cast<io_handle::size_type *>(alloca(reqs.buffers.size() * sizeof(io_handle::size_type)));
  io_handle::buffers_type output_buffers(reqs.buffers);
  io_handle::io_request<io_handle::buffers_type> creq({input_buffers_mem, reqs.buffers.size()}, 0);
  for(size_t n = 0; n < reqs.buffers.size(); n++)
  {
    // Copy input buffer to stack and retain original size
    creq.buffers[n] = reqs.buffers[n];
    input_buffers_sizes[n] = reqs.buffers[n].size();
    // Set output buffer length to zero
    output_buffers[n] = io_handle::buffer_type{output_buffers[n].data(), 0};
  }

  // Track which output buffer we are currently filling
  size_t idx = 0;
  do
  {
    // New deadline for this loop
    deadline nd;
    if(d)
    {
      if(d.steady)
      {
        auto ns = chrono::duration_cast<chrono::nanoseconds>((began_steady + chrono::nanoseconds(d.nsecs)) - chrono::steady_clock::now());
        if(ns.count() < 0)
          nd.nsecs = 0;
        else
          nd.nsecs = ns.count();
      }
      else
        nd = d;
    }
    // Partial fill buffers with current request
    io_handle::buffers_type filled = h.read(creq, nd) VALUE;
    (void) filled;

    // Adjust output buffers by what was filled, and prepare input
    // buffers for next round of partial fill
    for(size_t n = 0; n < creq.buffers.size(); n++)
    {
      // Add the amount of this buffer filled to next offset read and to output buffer
      auto &input_buffer = creq.buffers[n];
      auto &output_buffer = output_buffers[idx + n];
      creq.offset += input_buffer.size();
      output_buffer = io_handle::buffer_type{output_buffer.data(), output_buffer.size() + input_buffer.size()};
      // Adjust input buffer to amount remaining
      input_buffer = io_handle::buffer_type{input_buffer.data() + input_buffer.size(), input_buffers_sizes[idx + n] - output_buffer.size()};
    }

    // Remove completely filled input buffers
    while(!creq.buffers.empty() && creq.buffers[0].size() == 0)
    {
      creq.buffers = io_handle::buffers_type(creq.buffers.data() + 1, creq.buffers.size() - 1);
      ++idx;
    }
  } while(!creq.buffers.empty());
  return output_buffers;
}


int main()
{
  return 0;
}