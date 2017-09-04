/* Declarations for POSIX system APIs
(C) 2015-2017 Niall Douglas <http://www.nedproductions.biz/> (8 commits)
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

#ifndef AFIO_POSIX_HPP
#define AFIO_POSIX_HPP

#include "../../../handle.hpp"

#ifdef _WIN32
#error You should not include posix/import.hpp on Windows platforms
#endif

#include <fcntl.h>
#include <unistd.h>

AFIO_V2_NAMESPACE_BEGIN

inline result<int> attribs_from_handle_mode_caching_and_flags(native_handle_type &nativeh, handle::mode _mode, handle::creation _creation, handle::caching _caching, handle::flag) noexcept
{
  int attribs = O_CLOEXEC;
  switch(_mode)
  {
  case handle::mode::unchanged:
    return std::errc::invalid_argument;
  case handle::mode::none:
    break;
  case handle::mode::attr_read:
  case handle::mode::read:
    attribs = O_RDONLY;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
    break;
  case handle::mode::attr_write:
  case handle::mode::write:
    attribs = O_RDWR;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable | native_handle_type::disposition::writable;
    break;
  case handle::mode::append:
    attribs = O_WRONLY | O_APPEND;
    nativeh.behaviour |= native_handle_type::disposition::writable | native_handle_type::disposition::append_only;
    break;
  }
  switch(_creation)
  {
  case handle::creation::open_existing:
    break;
  case handle::creation::only_if_not_exist:
    attribs |= O_CREAT | O_EXCL;
    break;
  case handle::creation::if_needed:
    attribs |= O_CREAT;
    break;
  case handle::creation::truncate:
    attribs |= O_TRUNC;
    break;
  }
  switch(_caching)
  {
  case handle::caching::unchanged:
    return std::errc::invalid_argument;
  case handle::caching::none:
    attribs |= O_SYNC | O_DIRECT;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::only_metadata:
    attribs |= O_DIRECT;
    nativeh.behaviour |= native_handle_type::disposition::aligned_io;
    break;
  case handle::caching::reads:
    attribs |= O_SYNC;
    break;
  case handle::caching::reads_and_metadata:
#ifdef O_DSYNC
    attribs |= O_DSYNC;
#else
    attribs |= O_SYNC;
#endif
    break;
  case handle::caching::all:
  case handle::caching::safety_fsyncs:
  case handle::caching::temporary:
    break;
  }
  return attribs;
}

AFIO_V2_NAMESPACE_END

#endif
