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

#ifndef LLFIO_MAP_VIEW_HPP
#define LLFIO_MAP_VIEW_HPP

#include "mapped.hpp"

//! \file map_view.hpp Provides typed view of mapped section.

LLFIO_V2_NAMESPACE_BEGIN

/*! \brief Provides a lightweight typed view of a `map_handle`, a `mapped_file_handle`
or a `mapped<T>` suitable for feeding to STL algorithms or the Ranges TS.

This is the correct type to use when passing non-owning views of mapped data
around between functions. Where you wish the view to be (possibly) owning,
you may find the non-lightweight `mapped<T>` of more use.
*/
template <class T> class map_view : public span<T>
{
public:
  //! The extent type.
  using extent_type = typename section_handle::extent_type;
  //! The size type.
  using size_type = typename section_handle::size_type;

public:
  //! Default constructor
  constexpr map_view() {}  // NOLINT

  /*! Implicitly construct a mapped view of the given mapped data.

  \param map The mapped data to take a view upon.
  \param length The number of items to map, use -1 to mean the length of the input view.
  \param byteoffset The item offset into the mapped file handle.
  */
  map_view(mapped<T> &map, size_type length = (size_type) -1, size_type offset = 0)             // NOLINT
  : span<T>(map.begin() + offset, (length == (size_type) -1) ? (map.size() - offset) : length)  // NOLINT
  {
  }
  /*! Construct a mapped view of the given map handle.

  \param mh The map handle to use.
  \param length The number of items to map, use -1 to mean the length of the map handle divided by `sizeof(T)`.
  \param byteoffset The byte offset into the map handle, this does not need to be a multiple of the page size.
  */
  explicit map_view(map_handle &mh, size_type length = (size_type) -1, extent_type byteoffset = 0)                                             // NOLINT
  : span<T>(reinterpret_cast<T *>(mh.address() + byteoffset), (length == (size_type) -1) ? ((mh.length() - byteoffset) / sizeof(T)) : length)  // NOLINT
  {
  }
  /*! Construct a mapped view of the given mapped file handle.

  \param mfh The mapped file handle to take a view upon.
  \param length The number of items to map, use -1 to mean the length of the section handle divided by `sizeof(T)`.
  \param byteoffset The byte offset into the mapped file handle, this does not need to be a multiple of the page size.
  */
  explicit map_view(mapped_file_handle &mfh, size_type length = (size_type) -1, extent_type byteoffset = 0)                                                      // NOLINT
  : span<T>(reinterpret_cast<T *>(mfh.address() + byteoffset), (length == (size_type) -1) ? ((mfh.maximum_extent().value() - byteoffset) / sizeof(T)) : length)  // NOLINT
  {
  }
};

LLFIO_V2_NAMESPACE_END

#endif
