/* A handle which XORs two other handles
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: Oct 2018


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

#ifndef LLFIO_ALGORITHM_HANDLE_ADAPTER_XOR_H
#define LLFIO_ALGORITHM_HANDLE_ADAPTER_XOR_H

#include "combining.hpp"

//! \file handle_adapter/xor.hpp Provides `xor_handle_adapter`.

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace algorithm
{

  namespace detail
  {
    template <class Target, class Source> struct xor_handle_adapter_op
    {
      static_assert(!std::is_void<Source>::value, "Optional second input is not possible with xor_handle_adapter");

      using buffer_type = typename Target::buffer_type;
      using const_buffer_type = typename Target::const_buffer_type;
      using const_buffers_type = typename Target::const_buffers_type;

      static result<buffer_type> do_read(buffer_type out, buffer_type t, buffer_type s) noexcept
      {
        // Clamp out to smallest of inputs
        if(t.size() < out.size())
        {
          out = buffer_type(out.data(), t.size());
        }
        if(s.size() < out.size())
        {
          out = buffer_type(out.data(), s.size());
        }
        for(size_t idx = 0; idx < out.size();)
        {
          {
            // If addresses happen to align to SIMD, hint very strongly to use SIMD
            uintptr_t remain1 = ((uintptr_t) out.data() + idx) & 63;
            uintptr_t remain2 = ((uintptr_t) t.data() + idx) & 63;
            uintptr_t remain3 = ((uintptr_t) s.data() + idx) & 63;
            if(remain1 == 0 && remain2 == 0 && remain3 == 0)
            {
              while(out.size() - idx >= 64)
              {
                uint64_t *a = (uint64_t *) (out.data() + idx);
                const uint64_t *b = (const uint64_t *) (t.data() + idx);
                const uint64_t *c = (const uint64_t *) (s.data() + idx);
                a[0] = b[0] ^ c[0];
                a[1] = b[1] ^ c[1];
                a[2] = b[2] ^ c[2];
                a[3] = b[3] ^ c[3];
                a[4] = b[4] ^ c[4];
                a[5] = b[5] ^ c[5];
                a[6] = b[6] ^ c[6];
                a[7] = b[7] ^ c[7];
                idx += 64;
              }
            }
          }
          {
            // If addresses happen to align to registers, use that
            uintptr_t remain1 = ((uintptr_t) out.data() + idx) & (sizeof(uintptr_t) - 1);
            uintptr_t remain2 = ((uintptr_t) t.data() + idx) & (sizeof(uintptr_t) - 1);
            uintptr_t remain3 = ((uintptr_t) s.data() + idx) & (sizeof(uintptr_t) - 1);
            if(remain1 == 0 && remain2 == 0 && remain3 == 0)
            {
              while(out.size() - idx >= sizeof(uintptr_t))
              {
                uintptr_t *a = (uintptr_t *) (out.data() + idx);
                const uintptr_t *b = (const uintptr_t *) (t.data() + idx);
                const uintptr_t *c = (const uintptr_t *) (s.data() + idx);
                a[0] = b[0] ^ c[0];
                idx += sizeof(uintptr_t);
              }
            }
          }
          // Otherwise byte work
          while(out.size() - idx > 0)
          {
            uint8_t *a = (uint8_t *) (out.data() + idx);
            const uint8_t *b = (const uint8_t *) (t.data() + idx);
            const uint8_t *c = (const uint8_t *) (s.data() + idx);
            a[0] = b[0] ^ c[0];
            idx += 1;
          }
        }
        return out;
      }

      static result<const_buffer_type> do_write(buffer_type t, buffer_type s, const_buffer_type in) noexcept
      {
        // in is the constraint here
        for(size_t idx = 0; idx < in.size();)
        {
          {
            // If addresses happen to align to SIMD, hint very strongly to use SIMD
            uintptr_t remain1 = ((uintptr_t) t.data() + idx) & 63;
            uintptr_t remain2 = ((uintptr_t) s.data() + idx) & 63;
            uintptr_t remain3 = ((uintptr_t) in.data() + idx) & 63;
            if(remain1 == 0 && remain2 == 0 && remain3 == 0)
            {
              while(in.size() - idx >= 64)
              {
                uint64_t *a = (uint64_t *) (t.data() + idx);
                const uint64_t *b = (const uint64_t *) (s.data() + idx);
                const uint64_t *c = (const uint64_t *) (in.data() + idx);
                a[0] = b[0] ^ c[0];
                a[1] = b[1] ^ c[1];
                a[2] = b[2] ^ c[2];
                a[3] = b[3] ^ c[3];
                a[4] = b[4] ^ c[4];
                a[5] = b[5] ^ c[5];
                a[6] = b[6] ^ c[6];
                a[7] = b[7] ^ c[7];
                idx += 64;
              }
            }
          }
          {
            // If addresses happen to align to registers, use that
            uintptr_t remain1 = ((uintptr_t) t.data() + idx) & (sizeof(uintptr_t) - 1);
            uintptr_t remain2 = ((uintptr_t) s.data() + idx) & (sizeof(uintptr_t) - 1);
            uintptr_t remain3 = ((uintptr_t) in.data() + idx) & (sizeof(uintptr_t) - 1);
            if(remain1 == 0 && remain2 == 0 && remain3 == 0)
            {
              while(in.size() - idx >= sizeof(uintptr_t))
              {
                uintptr_t *a = (uintptr_t *) (t.data() + idx);
                const uintptr_t *b = (const uintptr_t *) (s.data() + idx);
                const uintptr_t *c = (const uintptr_t *) (in.data() + idx);
                a[0] = b[0] ^ c[0];
                idx += sizeof(uintptr_t);
              }
            }
          }
          // Otherwise byte work
          while(in.size() - idx > 0)
          {
            uint8_t *a = (uint8_t *) (t.data() + idx);
            const uint8_t *b = (const uint8_t *) (s.data() + idx);
            const uint8_t *c = (const uint8_t *) (in.data() + idx);
            a[0] = b[0] ^ c[0];
            idx += 1;
          }
        }
        // Adjust buffers returned to bytes read from in!
        t = {t.data(), in.size()};
        return t;
      }

      static result<const_buffers_type> adjust_written_buffers(const_buffers_type out, const_buffer_type twritten, const_buffer_type toriginal) noexcept
      {
        if(twritten.size() < toriginal.size())
        {
          auto byteswritten = twritten.size();
          for(auto &buffer : out)
          {
            if(buffer.size() >= byteswritten)
            {
              byteswritten -= buffer.size();
            }
            else if(byteswritten > 0)
            {
              buffer = {buffer.data(), byteswritten};
              byteswritten = 0;
            }
            else
            {
              buffer = {buffer.data(), 0};
            }
          }
        }
        return out;
      }

      template <class Base> struct override_ : public Base
      {
        override_() = default;
        using Base::Base;
      };
    };
  }

  /*! \brief A handle combining the data from two other handles using XOR.
  \tparam Target The type of the target handle. This is the one written to during any writes i.e. the input and
  second handle are XORed together and written to the first handle.
  \tparam Source The type of the second handle with which to XOR the target handle.

  \warning This class is still in development, do not use.
  */
  template <class Target, class Source> using xor_handle_adapter = combining_handle_adapter<detail::xor_handle_adapter_op, Target, Source>;

  // BEGIN make_free_functions.py

  // END make_free_functions.py

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#endif
