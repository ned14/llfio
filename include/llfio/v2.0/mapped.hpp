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

/*! \brief Provides an owning, typed view of memory mapped from a `section_handle` or a `file_handle` suitable
for feeding to STL algorithms or the Ranges TS.

This opens a new `map_handle` (and if necessary a `section_handle`) onto the requested offset and length
of the supplied source, and thus is an *owning* view of mapped memory. It can be moved, but not copied.
If you wish to pass around a non-owning view, see `map_view<T>`.

Optionally can issue a blocking write barrier on destruction of the mapped view by setting the flag
`section_handle::flag::barrier_on_close`, thus forcing any changes to data referred to by this
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
  section_handle _sectionh;
  map_handle _maph;
  mapped(file_handle *backing, size_type maximum_size, extent_type page_offset, extent_type offset, section_handle *sh, size_type bytes, section_handle::flag _flag)  // NOLINT
  : _sectionh((backing != nullptr) ? section_handle::section(*backing, maximum_size, _flag).value() : section_handle()),                                              //
    _maph(map_handle::map((sh != nullptr) ? *sh : _sectionh, (bytes == 0) ? 0 : bytes + (offset - page_offset), page_offset, _flag).value())
  {
    if(sh == nullptr)
    {
      sh = &_sectionh;
    }
    offset -= page_offset;
    byte *addr = _maph.address() + offset;
    size_t len = sh->length().value() - offset;  // use section length, not mapped length as mapped length is rounded up to page size
    if(bytes != 0 && bytes < len)
    {
      len = bytes;
    }
    static_cast<span<T> &>(*this) = span<T>(reinterpret_cast<T *>(addr), len / sizeof(T));  // NOLINT
  }

public:
  //! Default constructor
  constexpr mapped() {}  // NOLINT

  //! Returns a reference to the internal section handle
  const section_handle &section() const noexcept { return _sectionh; }
  //! Returns a reference to the internal map handle
  const map_handle &map() const noexcept { return _maph; }

  /*! Create a view of newly allocated unused memory, creating new memory if insufficient unused memory is available.
  Note that the memory mapped by this call may contain non-zero bits (recycled memory) unless `zeroed` is true.

  \param length The number of items to map.
  \param zeroed Whether to ensure that the viewed memory returned is all bits zero or not.
  \param _flag The flags to pass to `map_handle::map()`.
  */
  explicit mapped(size_type length, bool zeroed = false, section_handle::flag _flag = section_handle::flag::readwrite)
      : _maph(map_handle::map(length * sizeof(T), zeroed, _flag).value())
  {
    byte *addr = _maph.address();
    static_cast<span<T> &>(*this) = span<T>(reinterpret_cast<T *>(addr), length);  // NOLINT
  }
  /*! Construct a mapped view of the given section handle.

  \param sh The section handle to use as the data source for creating the map.
  \param length The number of items to map, use -1 to mean the length of the section handle divided by `sizeof(T)`.
  \param byteoffset The byte offset into the section handle, this does not need to be a multiple of the page size.
  \param _flag The flags to pass to `map_handle::map()`.
  */
  explicit mapped(section_handle &sh, size_type length = (size_type) -1, extent_type byteoffset = 0, section_handle::flag _flag = section_handle::flag::readwrite)  // NOLINT
  : mapped((length == 0) ? mapped() : mapped(nullptr, 0,
#ifdef _WIN32
                                             byteoffset & ~65535,
#else
                                             utils::round_down_to_page_size(byteoffset, utils::page_size()),
#endif
                                             byteoffset, &sh, (length == (size_type) -1) ? 0 : length * sizeof(T), _flag))  // NOLINT
  {
  }
  /*! Construct a mapped view of the given file.

  \param backing The handle to use as backing storage.
  \param length The number of items to map, use -1 to mean the length of the section handle divided by `sizeof(T)`.
  \param maximum_size The initial size of this section *in bytes*, which cannot be larger than any backing file. Zero means to use `backing.maximum_extent()`.
  \param byteoffset The byte offset into the section handle, this does not need to be a multiple of the page size.
  \param _flag The flags to pass to `map_handle::map()`.
  */
  explicit mapped(file_handle &backing, size_type length = (size_type) -1, extent_type maximum_size = 0, extent_type byteoffset = 0, section_handle::flag _flag = section_handle::flag::readwrite)  // NOLINT
  : mapped((length == 0) ? mapped() : mapped(&backing, maximum_size,
#ifdef _WIN32
                                             byteoffset & ~65535,
#else
                                             utils::round_down_to_page_size(byteoffset, utils::page_size()),
#endif
                                             byteoffset, nullptr, (length == (size_type) -1) ? 0 : length * sizeof(T), _flag))  // NOLINT
  {
  }
};

LLFIO_V2_NAMESPACE_END

#endif
