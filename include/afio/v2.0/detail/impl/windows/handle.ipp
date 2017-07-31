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
#include "import.hpp"

AFIO_V2_NAMESPACE_BEGIN

handle::~handle()
{
  if(_v)
  {
    // Call close() below
    auto ret = handle::close();
    if(ret.has_error())
    {
      AFIO_LOG_FATAL(_v.h, "handle::~handle() close failed");
      abort();
    }
  }
}

result<handle::path_type> handle::current_path() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // Most efficient, least memory copying method is direct fill of a wstring which is moved into filesystem::path
  filesystem::path::string_type buffer;
  buffer.resize(32769);
  wchar_t *_buffer = const_cast<wchar_t *>(buffer.data());
  memcpy(_buffer, L"\\\\.", 6);
  DWORD len = GetFinalPathNameByHandle(_v.h, _buffer + 3, buffer.size() - 4 * sizeof(wchar_t), VOLUME_NAME_NT);  // NOLINT
  if(len == 0)
  {
    return {GetLastError(), std::system_category()};
  }
  buffer.resize(3 + len);
  return path_type(std::move(buffer));
}

result<void> handle::close() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  if(_v)
  {
    if(are_safety_fsyncs_issued())
    {
      if(!FlushFileBuffers(_v.h))
        return {GetLastError(), std::system_category()};
    }
    if(!CloseHandle(_v.h))
      return {GetLastError(), std::system_category()};
    _v = native_handle_type();
  }
  return success();
}

result<handle> handle::clone() const noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  result<handle> ret(handle(native_handle_type(), _caching, _flags));
  ret.value()._v.behaviour = _v.behaviour;
  if(!DuplicateHandle(GetCurrentProcess(), _v.h, GetCurrentProcess(), &ret.value()._v.h, 0, false, DUPLICATE_SAME_ACCESS))
    return {GetLastError(), std::system_category()};
  return ret;
}

result<void> handle::set_append_only(bool enable) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  // This works only due to special handling in OVERLAPPED later
  if(enable)
  {
    // Set append_only
    _v.behaviour |= native_handle_type::disposition::append_only;
  }
  else
  {
    // Remove append_only
    _v.behaviour &= ~native_handle_type::disposition::append_only;
  }
  return success();
}

result<void> handle::set_kernel_caching(caching caching) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  native_handle_type nativeh;
  handle::mode _mode = mode::none;
  if(is_append_only())
    _mode = mode::append;
  else if(is_writable())
    _mode = mode::write;
  else if(is_readable())
    _mode = mode::read;
  OUTCOME_TRY(access, access_mask_from_handle_mode(nativeh, _mode, _flags));
  OUTCOME_TRY(attribs, attributes_from_handle_caching_and_flags(nativeh, caching, _flags));
  nativeh.behaviour |= native_handle_type::disposition::file;
  if(INVALID_HANDLE_VALUE == (nativeh.h = ReOpenFile(_v.h, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, attribs)))
    return {GetLastError(), std::system_category()};
  _v.swap(nativeh);
  if(!CloseHandle(nativeh.h))
    return {GetLastError(), std::system_category()};
  return success();
}

AFIO_V2_NAMESPACE_END
