/* A typed view of a mapped section
(C) 2017-2018 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: Aug 2017


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

#ifndef LLFIO_MAPPED_HPP
#define LLFIO_MAPPED_HPP

#include "mapped_file_handle.hpp"
#include "utils.hpp"

//! \file mapped.hpp Provides typed view of mapped section.

LLFIO_V2_NAMESPACE_BEGIN

/*! \brief Provides a map owning typed view of a `section_handle` suitable for feeding to STL algorithms
or the Ranges TS by wrapping a `map_handle` into a `span<T>`.

This opens a new `map_handle` onto the requested offset and length of the supplied source, and thus
is an *owning* view of mapped memory. It can be moved, but not copied. If you wish to pass a non-owning
view, see `map_view<T>`.

Optionally can issue a blocking write barrier on destruction of the mapped view by setting the flag
`section_handle::flag::barrier_on_close`, thus forcing any changes to data referred to by the view
to storage before the destructor returns.
*/
template <class T> class mapped : public span<T>
{
public:
  //! The extent type.
  using extent_type = typename section_handle::extent_type;
  //! The size type.
  using size_type = typename section_handle::size_type;

private:
  map_handle _mapping;
  mapped(extent_type page_offset, extent_type offset, section_handle &sh, size_type bytes, section_handle::flag _flag)  // NOLINT
  : _mapping(map_handle::map(sh, (bytes == 0) ? 0 : bytes + (offset - page_offset), page_offset, _flag).value())
  {
    offset -= page_offset;
    byte *addr = _mapping.address() + offset;
    size_t len = sh.length().value() - offset;  // use section length, not mapped length as mapped length is rounded up to page size
    if(bytes != 0 && bytes < len)
    {
      len = bytes;
    }
    static_cast<span<T> &>(*this) = span<T>(reinterpret_cast<T *>(addr), len / sizeof(T));  // NOLINT
  }

public:
  //! Default constructor
  constexpr mapped() {}  // NOLINT

  //! Returns a reference to the internal map handle
  const map_handle &map_handle() const noexcept { return _mapping; }

  /*! Create a view of new memory.

  \param length The number of items to map.
  \param _flag The flags to pass to `map_handle::map()`.
  */
  explicit mapped(size_type length, section_handle::flag _flag = section_handle::flag::readwrite)
      : _mapping(map_handle::map(length * sizeof(T), _flag).value())
  {
    byte *addr = _mapping.address();
    static_cast<span<T> &>(*this) = span<T>(reinterpret_cast<T *>(addr), length);  // NOLINT
  }
  /*! Construct a mapped view of the given section handle.

  \param sh The section handle to use as the data source for creating the map.
  \param length The number of items to map, use -1 to mean the length of the section handle divided by `sizeof(T)`.
  \param byteoffset The byte offset into the section handle, this does not need to be a multiple of the page size.
  \param _flag The flags to pass to `map_handle::map()`.
  */
  explicit mapped(section_handle &sh, size_type length = (size_type) -1, extent_type byteoffset = 0, section_handle::flag _flag = section_handle::flag::readwrite)  // NOLINT
  : mapped((length == 0) ? mapped() : mapped(
#ifdef _WIN32
                                      byteoffset & ~65535,
#else
                                      utils::round_down_to_page_size(byteoffset),
#endif
                                      byteoffset, sh, (length == (size_type) -1) ? 0 : length * sizeof(T), _flag))  // NOLINT
  {
  }
};

LLFIO_V2_NAMESPACE_END

#endif
