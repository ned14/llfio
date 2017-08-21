/* A STL allocator using a section_handle for storage
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
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

#ifndef AFIO_SECTION_HANDLE_ALLOCATOR_HPP
#define AFIO_SECTION_HANDLE_ALLOCATOR_HPP

#include "../map_handle.hpp"
#include "../utils.hpp"

#include <memory>
#include <type_traits>

//! \file section_allocator.hpp Provides section based STL allocators.

AFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
/*! \todo `algorithm::monotonic_allocator<>`, borrow that from the unfortunately unusable `fixed_allocator` below
which is unusable because of checked iterators on Dinkumware STL (which causes two allocations per vector construction).
*/
#if 0
  namespace detail
  {
    struct state
    {
      section_handle &_backing;
      typename section_handle::extent_type _page_offset, _nonpage_offset;
      map_handle _allocation;
      size_t _use_count;
      state(section_handle &backing, typename section_handle::extent_type offset)
          : _backing(backing)
#ifdef _WIN32
          , _page_offset(offset & ~65535)
#else
          , _page_offset(utils::round_down_to_page_size(offset))
#endif
          , _nonpage_offset(offset - _page_offset)
          , _use_count(0)
      {
      }
    };
  }

  /*! \brief Adapts a `section_handle` for use as a STL container, usually `std::vector<T>`.
  \tparam T `T` must meet the TriviallyCopyable concept.

  \note You should be aware when using this allocator that `allocate()` always returns
  the same storage, even if at a different address. As a result, this allocator is useless
  for any STL container which allocates more than one thing, however, copy construction
  is correctly handled via a reference counter.
  */
  template <class T> AFIO_REQUIRES(std::is_trivially_copyable<T>::value) class fixed_allocator
  {
    static_assert(std::is_trivially_copyable<T>::value, "Type T must be trivially copyable to be used with this allocator.");
    template <class U> friend class fixed_allocator;
    template <class U> friend struct std::allocator_traits;
    template <class R, class S> friend inline bool operator==(const fixed_allocator<R> &a, const fixed_allocator<S> &b) noexcept;
    template <class R, class S> friend inline bool operator!=(const fixed_allocator<R> &a, const fixed_allocator<S> &b) noexcept;

    std::shared_ptr<detail::state> _state;

  public:
    //! The value type
    using value_type = T;
    using pointer = value_type *;
    using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
    using void_pointer = typename std::pointer_traits<pointer>::template rebind<void>;
    using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;
    using difference_type = typename std::pointer_traits<pointer>::difference_type;
    using size_type = typename std::make_unsigned<difference_type>::type;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::false_type;
    template <class U> struct rebind
    {
      using other = fixed_allocator<U>;
    };
    //! Constructs this allocator using `backing` as the source of allocations, with optional offset into the backing
    constexpr fixed_allocator(section_handle &backing, typename section_handle::extent_type offset = 0) noexcept : _state(std::make_shared<detail::state>(backing, offset)) {}
    //! Copy constructor
    fixed_allocator(const fixed_allocator &o) noexcept : _state(o._state) {}
    //! Rebinding copy constructor
    template <class U> fixed_allocator(const fixed_allocator<U> &o) noexcept : _state(o._state) {}

    pointer allocate(size_type n)
    {
      auto *p = _state.get();
      n = utils::round_up_to_page_size(n * sizeof(value_type));
      if(p->_allocation.length() != n)
      {
        p->_allocation = map_handle::map(p->_backing, n, p->_page_offset).value();
      }
      p->_use_count++;
      return reinterpret_cast<pointer>(p->_allocation.address() + p->_nonpage_offset);
    }
    pointer allocate(size_type n, const_void_pointer hint) { return allocate(a, n); }
    void deallocate(pointer /*unused*/, size_type /*unused*/)
    {
      auto *p = _state.get();
      if(!--p->_use_count)
      {
        p->_allocation = map_handle();
      }
    }
    template <class T, class... Args> static void construct(T *p, Args &&... args) { ::new(static_cast<void *>(p)) T(std::forward<Args>(args)...); }
    void construct(T *p, const T &o) { ::new(static_cast<void *>(p)) T(std::forward<T>(o)); }
    void construct(T *p, T &&o) { ::new(static_cast<void *>(p)) T(std::forward<T>(o)); }
    template <class T> void destroy(T *p) { p->~T(); }
    size_type max_size() const { return std::numeric_limits<size_type>::max() / sizeof(value_type); }
    fixed_allocator select_on_container_copy_construction() { return *this; }
  };
  template <class T, class U> inline bool operator==(const fixed_allocator<T> &a, const fixed_allocator<U> &b) noexcept { return a._state == b._state; }
  template <class T, class U> inline bool operator!=(const fixed_allocator<T> &a, const fixed_allocator<U> &b) noexcept { return a._state != b._state; }

  /*! \brief Returns allocations from a backing `section_handle`.

  This allocator works by incrementing a shared extent value by the allocated amount,
  mapping the newly allocated space into memory and returning it. Deallocation unmaps
  the allocation and punches a hole in the underlying storage if necessary. Allocations
  are always rounded up to the nearest page size.
  */
  template <class T, class A = std::allocator<T>> struct monotonic_allocator : public A
  {
    template <class U> struct rebind
    {
      typedef allocator<U> other;
    };
    allocator() {}
    allocator(const allocator &o)
      : A(o)
    {
    }
    template <class U>
    allocator(const allocator<U> &o)
      : A(o)
    {
    }
    typename A::pointer allocate(typename A::size_type n, typename std::allocator<void>::const_pointer hint = 0)
    {
      config &c = get_config();
      size_t count = ++c.count;
      if (count >= c.fail_from || count == c.fail_at)
        throw std::bad_alloc();
      return A::allocate(n, hint);
    }
  };
#endif

}  // namespace

AFIO_V2_NAMESPACE_END

#endif
