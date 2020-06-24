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

namespace detail
{
  template <class T, bool = std::is_trivially_copyable<T>::value> struct attach_or_reinterpret
  {
    static span<T> attach(span<byte> region) noexcept
    {
      return {reinterpret_cast<T *>(region.data()), region.size() / sizeof(T)};  // NOLINT
    }
    static span<byte> detach(span<T> region) noexcept
    {
      return {reinterpret_cast<byte *>(region.data()), region.size() * sizeof(T)};  // NOLINT
    }
  };
#if defined(__clang__)  //|| !defined(NDEBUG)
  // Clang doesn't memory copy for attach/detach of trivially copyable types
  template <class T> struct attach_or_reinterpret<T, true>
  {
    static span<T> attach(span<byte> region) noexcept
    {
      if(region.size() <= 4096)
      {
        return in_place_attach<T>(region);
      }
      return {reinterpret_cast<T *>(region.data()), region.size() / sizeof(T)};  // NOLINT
    }
    static span<byte> detach(span<T> region) noexcept
    {
      if(region.size() <= 4096)
      {
        return in_place_detach(region);
      }
      return {reinterpret_cast<byte *>(region.data()), region.size() * sizeof(T)};  // NOLINT
    }
  };
#endif
}  // namespace detail

/*! \brief Provides an owning, typed view of memory mapped from a `section_handle` or a `file_handle` suitable
for feeding to STL algorithms or the Ranges TS.

This opens a new `map_handle` (and if necessary a `section_handle`) onto the requested offset and length
of the supplied source, and thus is an *owning* view of mapped memory. It can be moved, but not copied.

The array of objects upon which the owning view is opened is attached via `in_place_detach<T>()` in
the `mapped` constructor. In the final destructor (i.e. not the destructor of any moved-from instances),
the array of objects is detached via `in_place_detach<T>()` just before the `map_handle` is destroyed.
As it is illegal to attach objects into more than one address or process at a time, you must not call
`mapped` on objects already mapped into any process anywhere else.

These owning semantics are convenient, but may be too heavy for your use case. You can gain more fine
grained control using `map_handle`/`mapped_file_handle` directly with P1631 `attached<T>`.

\note Only on the clang compiler, does `mapped` actually use P1631
in_place_attach/in_place_detach, as currently GCC and MSVC do two memcpy's of the mapped region. Also,
we restrict the use of in_place_attach/in_place_detach to regions less than 4Kb in size, as even clang
falls down on large regions.

Optionally can issue a blocking write barrier on destruction of the mapped view by setting the flag
`section_handle::flag::barrier_on_close`, thus forcing any changes to data referred to by this
to storage before the destructor returns.
*/
template <class T> class mapped : protected span<T>
{
public:
  //! The extent type.
  using extent_type = typename section_handle::extent_type;
  //! The size type.
  using size_type = typename section_handle::size_type;
  //! The index type
  //using index_type = typename span<T>::index_type;
  //! The element type
  using element_type = typename span<T>::element_type;
  //! The value type
  using value_type = typename span<T>::value_type;
  //! The reference type
  using reference = typename span<T>::reference;
  //! The pointer type
  using pointer = typename span<T>::pointer;
  //! The const reference type
  using const_reference = typename span<T>::const_reference;
  //! The const pointer type
  using const_pointer = typename span<T>::const_pointer;
  //! The iterator type
  using iterator = typename span<T>::iterator;
  //! The const iterator type
  //using const_iterator = typename span<T>::const_iterator;
  //! The reverse iterator type
  using reverse_iterator = typename span<T>::reverse_iterator;
  //! The const reverse iterator type
  //using const_reverse_iterator = typename span<T>::const_reverse_iterator;
  //! The difference type
  using difference_type = typename span<T>::difference_type;

private:
  section_handle _sectionh;
  map_handle _maph;
  mapped(file_handle *backing, size_type maximum_size, extent_type page_offset, extent_type offset, section_handle *sh, size_type bytes, section_handle::flag _flag)  // NOLINT
      : _sectionh((backing != nullptr) ? section_handle::section(*backing, maximum_size, _flag).value() : section_handle())
      ,  //
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
    if(_maph.is_writable())
    {
      static_cast<span<T> &>(*this) = detail::attach_or_reinterpret<T>::attach({addr, len});
    }
  }

public:
  //! Default constructor
  constexpr mapped() {}  // NOLINT

  mapped(const mapped &) = delete;
  mapped(mapped &&o) noexcept
      : span<T>(std::move(o))
      , _sectionh(std::move(o._sectionh))
      , _maph(std::move(o._maph))
  {
    static_cast<span<T> &>(o) = {};
  }
  mapped &operator=(const mapped &) = delete;
  mapped &operator=(mapped &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~mapped();
    new(this) mapped(std::move(o));
    return *this;
  }

  //! Detaches the array of `T`, before tearing down the map
  ~mapped()
  {
    if(!this->empty() && _maph.is_writable())
    {
      detail::attach_or_reinterpret<T>::detach(*this);
    }
  }

  //! Returns a reference to the internal section handle
  const section_handle &section() const noexcept { return _sectionh; }
  //! Returns a reference to the internal map handle
  const map_handle &map() const noexcept { return _maph; }
  //! Returns a span referring to this mapped region
  span<T> as_span() const noexcept { return *this; }

  using span<T>::first;
  using span<T>::last;
  using span<T>::subspan;
  using span<T>::size;
  //using span<T>::ssize;
  using span<T>::size_bytes;
  using span<T>::empty;
  using span<T>::operator[];
  //using span<T>::operator();
  //using span<T>::at;
  using span<T>::data;
  using span<T>::begin;
  using span<T>::end;
  //using span<T>::cbegin;
  //using span<T>::cend;
  using span<T>::rbegin;
  using span<T>::rend;
  //using span<T>::crbegin;
  //using span<T>::crend;
  //using span<T>::swap;

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
    static_cast<span<T> &>(*this) = detail::attach_or_reinterpret<T>::attach({addr, length * sizeof(T)});
  }
  /*! Construct a mapped view of the given section handle.

  \param sh The section handle to use as the data source for creating the map.
  \param length The number of items to map, use -1 to mean the length of the section handle divided by `sizeof(T)`.
  \param byteoffset The byte offset into the section handle, this does not need to be a multiple of the page size.
  \param _flag The flags to pass to `map_handle::map()`.
  */
  explicit mapped(section_handle &sh, size_type length = (size_type) -1, extent_type byteoffset = 0, section_handle::flag _flag = section_handle::flag::readwrite)  // NOLINT
      : mapped((length == 0) ? mapped() :
                               mapped(nullptr, 0,
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
      : mapped((length == 0) ? mapped() :
                               mapped(&backing, maximum_size,
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
