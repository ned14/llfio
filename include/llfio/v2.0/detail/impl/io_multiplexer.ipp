/* Multiplex file i/o
(C) 2019 Niall Douglas <http://www.nedproductions.biz/> (9 commits)
File Created: Nov 2019


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

#include "../../io_multiplexer.hpp"

#include <mutex>

LLFIO_V2_NAMESPACE_BEGIN

namespace this_thread
{
  static LLFIO_THREAD_LOCAL io_multiplexer *_thread_multiplexer;
  LLFIO_HEADERS_ONLY_FUNC_SPEC io_multiplexer *multiplexer() noexcept { return _thread_multiplexer; }
  LLFIO_HEADERS_ONLY_FUNC_SPEC void set_multiplexer(io_multiplexer *ctx) noexcept { _thread_multiplexer = ctx; }
}  // namespace this_thread

template <bool is_threadsafe> struct io_multiplexer_impl : io_multiplexer
{
  struct _lock_impl_type
  {
    void lock() {}
    void unlock() {}
  };
  _lock_impl_type _lock;
  using _lock_guard = std::unique_lock<_lock_impl_type>;
};
template <> struct io_multiplexer_impl<true> : io_multiplexer
{
  using _lock_impl_type = std::mutex;
  _lock_impl_type _lock;
  using _lock_guard = std::unique_lock<_lock_impl_type>;
};

LLFIO_V2_NAMESPACE_END
