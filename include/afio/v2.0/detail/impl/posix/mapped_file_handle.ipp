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

AFIO_V2_NAMESPACE_BEGIN

result<mapped_file_handle::size_type> mapped_file_handle::reserve(size_type reservation) noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
  OUTCOME_TRY(length, underlying_file_length());
  if(length == 0)
  {
    // Not portable to map an empty file, so fail
    return std::errc::invalid_seek;
  }
  if(reservation == 0)
  {
    reservation = length;
  }
  reservation = utils::round_up_to_page_size(reservation);
  if(!_sh.is_valid())
  {
    section_handle::flag sectionflags = section_handle::flag::readwrite;
    OUTCOME_TRY(sh, section_handle::section(*this, length, sectionflags));
    _sh = std::move(sh);
  }
  if(_mh.is_valid() && reservation == _mh.length())
  {
    return reservation;
  }
  // Reserve the full reservation in address space
  section_handle::flag mapflags = section_handle::flag::nocommit | section_handle::flag::read;
  if(this->is_writable())
  {
    mapflags |= section_handle::flag::write;
  }
  OUTCOME_TRYV(_mh.close());
  OUTCOME_TRY(mh, map_handle::map(_sh, reservation, 0, mapflags));
  _mh = std::move(mh);
  _reservation = reservation;
  return reservation;
}

result<void> mapped_file_handle::close() noexcept
{
  AFIO_LOG_FUNCTION_CALL(this);
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
  AFIO_LOG_FUNCTION_CALL(this);
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
  AFIO_LOG_FUNCTION_CALL(this);
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
    // Reserve now we have resized, it'll create a new section for the new size
    OUTCOME_TRYV(reserve(_reservation));
    return ret;
  }
  // On POSIX the section's size is the file's size
  OUTCOME_TRY(size, _sh.length());
  if(size != newsize)
  {
    // If we are making this smaller, we must discard the pages about to get truncated
    // otherwise some kernels keep them around until last fd close, effectively leaking them
    if(newsize < size)
    {
      char *start = utils::round_up_to_page_size(_mh.address() + newsize);
      char *end = utils::round_up_to_page_size(_mh.address() + size);
      (void) _mh.do_not_store({start, static_cast<size_t>(end - start)});
    }
    // Resize the file, on unified page cache kernels it'll map any new pages into the reserved map
    OUTCOME_TRYV(file_handle::truncate(newsize));
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
  OUTCOME_TRY(length, underlying_file_length());
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
  // Adjust the map to reflect the new size of the section
  _mh._length = length;
  return length;
}

AFIO_V2_NAMESPACE_END
