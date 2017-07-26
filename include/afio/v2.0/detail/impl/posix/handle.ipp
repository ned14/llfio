/* A handle to something
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: Dec 2015


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

#include "../../../handle.hpp"

#include <fcntl.h>
#include <unistd.h>

AFIO_V2_NAMESPACE_BEGIN

handle::~handle()
{
  if(_v)
  {
    // Call close() below
    auto ret = handle::close();
    if(ret.has_error())
    {
      AFIO_LOG_FATAL(_v.fd, "handle::~handle() close failed");
      abort();
    }
  }
}

result<void> handle::close() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(_v)
  {
    if(are_safety_fsyncs_issued())
    {
      if(-1 == fsync(_v.fd))
        return {errno, std::system_category()};
    }
    if(-1 == ::close(_v.fd))
      return {errno, std::system_category()};
    _v = native_handle_type();
  }
  return success();
}

result<handle> handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  result<handle> ret(handle(native_handle_type(), _caching, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  ret.value()._v.fd = ::dup(_v.fd);
  if(-1 == ret.value()._v.fd)
    return {errno, std::system_category()};
  return ret;
}

result<void> handle::set_append_only(bool enable) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  int attribs = fcntl(_v.fd, F_GETFL);
  if(-1 == attribs)
    return {errno, std::system_category()};
  if(enable)
  {
    // Set append_only
    attribs |= O_APPEND;
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour |= native_handle_type::disposition::append_only;
  }
  else
  {
    // Remove append_only
    attribs &= ~O_APPEND;
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour &= ~native_handle_type::disposition::append_only;
  }
  return success();
}

result<void> handle::set_kernel_caching(caching caching) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  int attribs = fcntl(_v.fd, F_GETFL);
  if(-1 == attribs)
    return {errno, std::system_category()};
  attribs &= ~(O_SYNC | O_DIRECT
#ifdef O_DSYNC
               | O_DSYNC
#endif
               );
  switch(_caching)
  {
  case handle::caching::unchanged:
    break;
  case handle::caching::none:
    attribs |= O_SYNC | O_DIRECT;
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::only_metadata:
    attribs |= O_DIRECT;
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::reads:
    attribs |= O_SYNC;
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour &= ~native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::reads_and_metadata:
#ifdef O_DSYNC
    attribs |= O_DSYNC;
#else
    attribs |= O_SYNC;
#endif
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour &= ~native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::all:
  case handle::caching::safety_fsyncs:
  case handle::caching::temporary:
    if(-1 == fcntl(_v.fd, F_SETFL, attribs))
      return {errno, std::system_category()};
    _v.behaviour &= ~native_handle_type::disposition::aligned_io;
    break;
  }
  _caching = caching;
  return success();
}

AFIO_V2_NAMESPACE_END
