/* An mapped handle to a file
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (11 commits)
File Created: Sept 2017


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

#include "../../../mapped_file_handle.hpp"
#include "import.hpp"

LLFIO_V2_NAMESPACE_BEGIN

result<mapped_file_handle::size_type> mapped_file_handle::reserve(size_type reservation) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(length, underlying_file_maximum_extent());
  if(length == 0)
  {
    // Cannot create a section nor a map for a zero lengthed file
    _reservation = reservation;
    return success();
  }
  if(reservation == 0)
  {
    reservation = length;
  }
  if(!_sh.is_valid())
  {
    // Section must match read/write of file, as otherwise map reservation doesn't work on Windows
    section_handle::flag sectionflags = _sh.section_flags() | section_handle::flag::singleton;
    if(this->is_readable())
      sectionflags |= section_handle::flag::read;
    if(this->is_writable())
      sectionflags |= section_handle::flag::write;
    OUTCOME_TRY(sh, section_handle::section(*this, 0, sectionflags));
    _sh = std::move(sh);
  }
  if(_mh.is_valid() && reservation == _mh.length())
  {
    _reservation = reservation;
    return _reservation;
  }
  section_handle::flag mapflags = section_handle::flag::read | section_handle::flag::nocommit;
  // Reserve the full reservation in address space
  if(this->is_writable())
  {
    mapflags |= section_handle::flag::write;
  }
  OUTCOME_TRYV(_mh.close());
  OUTCOME_TRY(mh, map_handle::map(_sh, reservation, 0, mapflags));
  _mh = std::move(mh);
  _reservation = reservation;
  return _reservation;
}

result<void> mapped_file_handle::close() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_mh.is_valid())
  {
    OUTCOME_TRYV(_mh.close());
  }
  if(_sh.is_valid())
  {
    OUTCOME_TRYV(_sh.close());
  }
  return file_handle::close();
}
native_handle_type mapped_file_handle::release() noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  if(_mh.is_valid())
  {
    (void) _mh.close();
  }
  if(_sh.is_valid())
  {
    (void) _sh.close();
  }
  return file_handle::release();
}

result<mapped_file_handle::extent_type> mapped_file_handle::truncate(extent_type newsize) noexcept
{
  LLFIO_LOG_FUNCTION_CALL(this);
  // Release all maps and sections and truncate the backing file to zero
  if(newsize == 0)
  {
    OUTCOME_TRYV(_mh.close());
    OUTCOME_TRYV(_sh.close());
    return file_handle::truncate(newsize);
  }
  if(!_sh.is_valid())
  {
    OUTCOME_TRY(ret, file_handle::truncate(newsize));
    if(newsize > _reservation)
    {
      _reservation = newsize;
    }
    // Reserve now we have resized, it'll create a new section for the new size
    OUTCOME_TRYV(reserve(_reservation));
    return ret;
  }
  // Ask the section what size it is. If multiple LLFIO processes are using the same file,
  // because the section is a singleton based on the canonical path of the file, another
  // LLFIO in another process may have already resized the section for us in which case
  // we can skip doing work now.
  OUTCOME_TRY(size, _sh.length());
  if(size != newsize)
  {
    // If we are making this smaller, we must destroy the map and section first
    if(newsize < size)
    {
      OUTCOME_TRYV(_mh.close());
      OUTCOME_TRYV(_sh.close());
      // This will fail on Windows if any other processes are holding a section on this file
      OUTCOME_TRY(ret, file_handle::truncate(newsize));
      // Put the reservation and map back
      OUTCOME_TRYV(reserve(_reservation));
      return ret;
    }
    // Otherwise resize the file upwards, then the section.
    OUTCOME_TRYV(file_handle::truncate(newsize));
    // On Windows, resizing the section upwards maps the added extents into memory in all
    // processes using this singleton section
    OUTCOME_TRYV(_sh.truncate(newsize));
    // Have we exceeded the reservation? If so, reserve a new reservation which will recreate the map.
    if(newsize > _reservation)
    {
      OUTCOME_TRY(ret, reserve(newsize));
      return ret;
    }
    size = newsize;
  }
  // Adjust the map to reflect the new size of the section
  _mh._length = size;
  return newsize;
}

result<mapped_file_handle::extent_type> mapped_file_handle::update_map() noexcept
{
  OUTCOME_TRY(length, underlying_file_maximum_extent());
  if(length > _reservation)
  {
    // This API never exceeds the reservation
    length = _reservation;
  }
  if(length == 0)
  {
    OUTCOME_TRYV(_mh.close());
    OUTCOME_TRYV(_sh.close());
    return length;
  }
  if(!_sh.is_valid())
  {
    OUTCOME_TRYV(reserve(_reservation));
    return length;
  }
  OUTCOME_TRY(size, _sh.length());
  // Section may have become bigger than our reservation ...
  if(size >= length)
  {
    // Section is already the same size as the file, or is as big as it can go
    _mh._length = length;
    return length;
  }
  // Nobody appears to have extended the section to match the file yet
  OUTCOME_TRYV(_sh.truncate(length));
  // Adjust the map to reflect the new size of the section
  _mh._length = length;
  return length;
}

LLFIO_V2_NAMESPACE_END
