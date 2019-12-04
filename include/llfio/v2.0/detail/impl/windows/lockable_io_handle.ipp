/* A lockable i/o handle to something
(C) 2015-2019 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
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

#include "../../../lockable_io_handle.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

result<void> lockable_io_handle::lock_file() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(do_lock_file_range(_v, 0xffffffffffffffffULL, 1, true, {}));
  return success();
}
bool lockable_io_handle::try_lock_file() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto r = do_lock_file_range(_v, 0xffffffffffffffffULL, 1, true, std::chrono::seconds(0));
  return !!r;
}
void lockable_io_handle::unlock_file() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  do_unlock_file_range(_v, 0xffffffffffffffffULL, 1);
}

result<void> lockable_io_handle::lock_file_shared() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(do_lock_file_range(_v, 0xffffffffffffffffULL, 1, false, {}));
  return success();
}
bool lockable_io_handle::try_lock_file_shared() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  auto r = do_lock_file_range(_v, 0xffffffffffffffffULL, 1, false, std::chrono::seconds(0));
  return !!r;
}
void lockable_io_handle::unlock_file_shared() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  do_unlock_file_range(_v, 0xffffffffffffffffULL, 1);
}


result<lockable_io_handle::extent_guard> lockable_io_handle::lock_file_range(io_handle::extent_type offset, io_handle::extent_type bytes, lock_kind kind, deadline d) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(do_lock_file_range(_v, offset, bytes, kind != lock_kind::shared, d));
  return extent_guard(this, offset, bytes, kind);
}

void lockable_io_handle::unlock_file_range(io_handle::extent_type offset, io_handle::extent_type bytes) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  do_unlock_file_range(_v, offset, bytes);
}


LLFIO_V2_NAMESPACE_END
