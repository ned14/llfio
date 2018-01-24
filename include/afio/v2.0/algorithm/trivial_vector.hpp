/* A STL vector using a section_handle for storage
(C) 2017 Niall Douglas <http://www.nedproductions.biz/> (12 commits)
File Created: Nov 2017


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

#ifndef AFIO_ALGORITHM_VECTOR_HPP
#define AFIO_ALGORITHM_VECTOR_HPP

#include "../map_handle.hpp"
#include "../utils.hpp"


//! \file trivial_vector.hpp Provides constant time reallocating STL vector.

AFIO_V2_NAMESPACE_BEGIN

namespace algorithm
{
#ifndef DOXYGEN_IS_IN_THE_HOUSE
  namespace detail
#else
  //! Does not exist in the actual source code, purely here to workaround doxygen limitations
  namespace impl
#endif
  {
    template <bool has_default_construction, class T> struct trivial_vector_impl;
    template <class T> class trivial_vector_iterator
    {
      template <bool has_default_construction, class _T> friend struct trivial_vector_impl;
      T *_v;
      explicit trivial_vector_iterator(T *v)
          : _v(v)
      {
      }

    public:
      //! Value type
      using value_type = T;
      //! Pointer type
      using pointer = value_type *;
      //! Const pointer type
      using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
      //! Difference type
      using difference_type = typename std::pointer_traits<pointer>::difference_type;
      //! Size type
      using size_type = typename std::make_unsigned<difference_type>::type;
      //! Reference type
      using reference = value_type &;
      //! Const reference type
      using const_reference = const value_type &;

      //! Increment the iterator
      trivial_vector_iterator &operator+=(size_type n)
      {
        _v += n;
        return *this;
      }
      //! Decrement the iterator
      trivial_vector_iterator &operator-=(size_type n)
      {
        _v -= n;
        return *this;
      }
      //! Difference between iterators
      difference_type operator-(const trivial_vector_iterator &o) const { return _v - o._v; }
      //! Index iterator
      reference operator[](size_type n) { return _v[n]; }
      //! Index iterator
      const_reference operator[](size_type n) const { return _v[n]; }
      //! Compare
      bool operator<(const trivial_vector_iterator &o) const { return _v < o._v; }
      //! Compare
      bool operator<=(const trivial_vector_iterator &o) const { return _v <= o._v; }
      //! Compare
      bool operator>(const trivial_vector_iterator &o) const { return _v > o._v; }
      //! Compare
      bool operator>=(const trivial_vector_iterator &o) const { return _v >= o._v; }

      //! Decrement
      trivial_vector_iterator &operator--()
      {
        _v--;
        return *this;
      }
      //! Decrement
      const trivial_vector_iterator operator--(int /*unused*/)
      {
        trivial_vector_iterator ret(*this);
        _v--;
        return ret;
      }

      //! Increment
      trivial_vector_iterator &operator++()
      {
        _v++;
        return *this;
      }
      //! Increment
      const trivial_vector_iterator operator++(int /*unused*/)
      {
        trivial_vector_iterator ret(*this);
        _v++;
        return ret;
      }
      //! Compare
      bool operator==(const trivial_vector_iterator &o) const { return _v == o._v; }
      //! Compare
      bool operator!=(const trivial_vector_iterator &o) const { return _v != o._v; }

      //! Dereference
      reference operator*() { return *_v; }
      //! Dereference
      const_reference operator*() const { return *_v; }
      //! Dereference
      pointer operator->() { return _v; }
      //! Dereference
      const_pointer operator->() const { return _v; }
    };
    //! Adds to the iterator
    template <class T> inline trivial_vector_iterator<T> operator+(trivial_vector_iterator<T> a, size_t n)
    {
      a += n;
      return a;
    }
    //! Adds to the iterator
    template <class T> inline trivial_vector_iterator<T> operator+(size_t n, trivial_vector_iterator<T> a)
    {
      a += n;
      return a;
    }
    //! Subtracts from the iterator
    template <class T> inline trivial_vector_iterator<T> operator-(trivial_vector_iterator<T> a, size_t n)
    {
      a -= n;
      return a;
    }
    template <bool has_default_construction, class T> struct trivial_vector_impl
    {
      static_assert(std::is_trivially_copyable<T>::value, "trivial_vector: Type T is not trivially copyable!");

      //! Value type
      using value_type = T;
      //! Pointer type
      using pointer = value_type *;
      //! Const pointer type
      using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
      //! Difference type
      using difference_type = typename std::pointer_traits<pointer>::difference_type;
      //! Size type
      using size_type = typename std::make_unsigned<difference_type>::type;
      //! Reference type
      using reference = value_type &;
      //! Const reference type
      using const_reference = const value_type &;
      //! Iterator type
      using iterator = pointer;
      //! Const iterator type
      using const_iterator = const_pointer;
      //! Reverse iterator type
      using reverse_iterator = std::reverse_iterator<iterator>;
      //! Const reverse iterator type
      using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    private:
      section_handle _sh;
      map_handle _mh;
      pointer _begin{nullptr}, _end{nullptr}, _capacity{nullptr};

      static size_type _scale_capacity(size_type cap)
      {
        if(cap == 0)
        {
          cap = utils::page_size() / sizeof(value_type);
          return cap;
        }
        return cap * 2;
      }

    public:
      //! Default constructor
      trivial_vector_impl() = default;
      //! Filling constructor of `value_type`
      trivial_vector_impl(size_type count, const value_type &v) { insert(begin(), count, v); }
      //! Range constructor
      template <class InputIt> trivial_vector_impl(InputIt first, InputIt last) { insert(begin(), first, last); }
      //! Copy constructor disabled, use range constructor if you really want this
      trivial_vector_impl(const trivial_vector_impl &) = delete;
      //! Copy assigned disabled, use range constructor if you really want this
      trivial_vector_impl &operator=(const trivial_vector_impl &) = delete;
      //! Move constructor
      trivial_vector_impl(trivial_vector_impl &&o) noexcept : _sh(std::move(o._sh)), _mh(std::move(o._mh)), _begin(o._begin), _end(o._end), _capacity(o._capacity)
      {
        _mh.set_section(&_sh);
        o._begin = o._end = o._capacity = nullptr;
      }
      //! Move assignment
      trivial_vector_impl &operator=(trivial_vector_impl &&o) noexcept
      {
        this->~trivial_vector_impl();
        new(this) trivial_vector_impl(std::move(o));
        return *this;
      }
      //! Initialiser list constructor
      trivial_vector_impl(std::initializer_list<value_type> il);
      ~trivial_vector_impl() { clear(); }

      //! Assigns
      void assign(size_type count, const value_type &v)
      {
        clear();
        insert(begin(), count, v);
      }
      //! Assigns
      template <class InputIt> void assign(InputIt first, InputIt last)
      {
        clear();
        insert(begin(), first, last);
      }
      //! Assigns
      void assign(std::initializer_list<value_type> il) { assign(il.begin(), il.end()); }

      //! Item index, bounds checked
      reference at(size_type i)
      {
        if(i >= size())
        {
          throw std::out_of_range("bounds exceeded");  // NOLINT
        }
        return _begin[i];
      }
      //! Item index, bounds checked
      const_reference at(size_type i) const
      {
        if(i >= size())
        {
          throw std::out_of_range("bounds exceeded");  // NOLINT
        }
        return _begin[i];
      }
      //! Item index, unchecked
      reference operator[](size_type i) { return _begin[i]; }
      //! Item index, unchecked
      const_reference operator[](size_type i) const { return _begin[i]; }
      //! First element
      reference front() { return _begin[0]; }
      //! First element
      const_reference front() const { return _begin[0]; }
      //! Last element
      reference back() { return _end[-1]; }
      //! Last element
      const_reference back() const { return _end[-1]; }
      //! Underlying array
      pointer data() noexcept { return _begin; }
      //! Underlying array
      const_pointer data() const noexcept { return _begin; }

      //! Iterator to first item
      iterator begin() noexcept { return iterator(_begin); }
      //! Iterator to first item
      const_iterator begin() const noexcept { return const_iterator(_begin); }
      //! Iterator to first item
      const_iterator cbegin() const noexcept { return const_iterator(_begin); }
      //! Iterator to after last item
      iterator end() noexcept { return iterator(_end); }
      //! Iterator to after last item
      const_iterator end() const noexcept { return const_iterator(_end); }
      //! Iterator to after last item
      const_iterator cend() const noexcept { return const_iterator(_end); }
      //! Iterator to last item
      reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
      //! Iterator to last item
      const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
      //! Iterator to last item
      const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
      //! Iterator to before first item
      reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
      //! Iterator to before first item
      const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
      //! Iterator to before first item
      const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

      //! If empty
      bool empty() const noexcept { return _end == _begin; }
      //! Items in container
      size_type size() const noexcept { return _end - _begin; }
      //! Maximum items in container
      size_type max_size() const noexcept { return static_cast<map_handle::size_type>(-1) / sizeof(T); }
      //! Increase capacity
      void reserve(size_type n)
      {
        if(n > max_size())
        {
          throw std::length_error("Max size exceeded");  // NOLINT
        }
        size_type current_size = size();
        size_type bytes = n * sizeof(value_type);
        bytes = utils::round_up_to_page_size(bytes);
        if(!_sh.is_valid())
        {
          _sh = section_handle::section(bytes).value();
        }
        else if(n > capacity())
        {
          _mh.close().value();
          _sh.truncate(bytes).value();
        }
        else
        {
          return;
        }
        _mh = map_handle::map(_sh, bytes).value();
        _begin = reinterpret_cast<pointer>(_mh.address());
        _capacity = reinterpret_cast<pointer>(_mh.address() + bytes);
        _end = _begin + current_size;
      }
      //! Items can be stored until storage expanded
      size_type capacity() const noexcept { return _capacity - _begin; }
      //! Removes unused capacity
      void shrink_to_fit()
      {
        size_type current_size = size();
        size_type bytes = current_size * sizeof(value_type);
        bytes = utils::round_up_to_page_size(bytes);
        if(bytes / sizeof(value_type) == capacity())
        {
          return;
        }
        if(bytes == 0)
        {
          _mh.close().value();
          _sh.close().value();
          _begin = _end = _capacity = nullptr;
          return;
        }
        _mh.close().value();
        _sh.truncate(bytes).value();
        _mh = map_handle::map(_sh, bytes).value();
        _begin = reinterpret_cast<pointer>(_mh.address());
        _capacity = reinterpret_cast<pointer>(_mh.address() + bytes);
        _end = _begin + current_size;
      }
      //! Clears container
      void clear() noexcept
      {
        // Trivially copyable means trivial destructor
        _end = _begin;
      }

      //! Inserts item
      iterator insert(const_iterator pos, const value_type &v) { return emplace(pos, v); }
      //! Inserts item
      iterator insert(const_iterator pos, value_type &&v) { return emplace(pos, std::move(v)); }
      //! Inserts items
      iterator insert(const_iterator pos, size_type count, const value_type &v)
      {
        {
          size_type cap = capacity();
          while(size() + count < cap)
          {
            cap = _scale_capacity(cap);
          }
          reserve(cap);
        }
        memmove(pos._v + count, pos._v, _end - pos._v);
        size_type n = 0;
        try
        {
          for(; n < count; n++)
          {
            new(pos._v + n) value_type(v);
          }
        }
        catch(...)
        {
          memmove(pos._v, pos._v + count, _end - pos._v);
          throw;
        }
        _end += count;
        return iterator(pos._v);
      }
      //! Inserts items
      template <class InputIt> iterator insert(const_iterator pos, InputIt first, InputIt last)
      {
        size_type count = std::distance(first, last);
        {
          size_type cap = capacity();
          while(size() + count < cap)
          {
            cap = _scale_capacity(cap);
          }
          reserve(cap);
        }
        memmove(pos._v + count, pos._v, _end - pos._v);
        size_type n = 0;
        try
        {
          for(; n < count; n++)
          {
            new(pos._v + n) value_type(*first++);
          }
        }
        catch(...)
        {
          memmove(pos._v, pos._v + count, _end - pos._v);
          throw;
        }
        _end += count;
        return iterator(pos._v);
      }
      //! Inserts items
      iterator insert(const_iterator pos, std::initializer_list<value_type> il) { return insert(pos, il.begin(), il.end()); }
      //! Emplace item
      template <class... Args> iterator emplace(const_iterator pos, Args &&... args)
      {
        if(capacity() == size())
        {
          reserve(_scale_capacity(capacity()));
        }
        memmove(pos._v + 1, pos._v, _end - pos._v);
        try
        {
          new(pos._v) value_type(std::forward<Args>(args)...);
        }
        catch(...)
        {
          memmove(pos._v, pos._v + 1, _end - pos._v);
          throw;
        }
        ++_end;
        return iterator(pos._v);
      }

      //! Erases item
      iterator erase(const_iterator pos);
      //! Erases items
      iterator erase(const_iterator first, const_iterator last);
      //! Appends item
      void push_back(const value_type &v) { emplace_back(v); }
      //! Appends item
      void push_back(value_type &&v) { emplace_back(std::move(v)); }
      //! Appends item
      template <class... Args> reference emplace_back(Args &&... args)
      {
        if(size() == capacity())
        {
          reserve(_scale_capacity(capacity()));
        }
        new(_end) value_type(std::forward<Args>(args)...);
        ++_end;
        return *(_end - 1);
      }
      //! Removes last element
      void pop_back()
      {
        if(!empty())
        {
          --_end;
        }
      }

      //! Resizes container
      void resize(size_type count, const value_type &v)
      {
        if(count < size())
        {
          // Trivially copyable means trivial destructor
          _end = _begin + count;
          return;
        }
        if(count > capacity())
        {
          reserve(count);
        }
        // TODO(ned): Kinda assuming the compiler will do the right thing below, should really check that
        while(count > size())
        {
          emplace_back(v);
        }
      }
      //! Swaps
      void swap(trivial_vector_impl &o) noexcept
      {
        using namespace std;
        swap(_sh, o._sh);
        swap(_mh, o._mh);
        swap(_begin, o._begin);
        swap(_end, o._end);
        swap(_capacity, o._capacity);
      }
    };

    template <class T> struct trivial_vector_impl<true, T> : trivial_vector_impl<false, T>
    {
      //! Value type
      using value_type = T;
      //! Pointer type
      using pointer = value_type *;
      //! Const pointer type
      using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
      //! Difference type
      using difference_type = typename std::pointer_traits<pointer>::difference_type;
      //! Size type
      using size_type = typename std::make_unsigned<difference_type>::type;
      //! Reference type
      using reference = value_type &;
      //! Const reference type
      using const_reference = const value_type &;
      //! Iterator type
      using iterator = pointer;
      //! Const iterator type
      using const_iterator = const_pointer;
      //! Reverse iterator type
      using reverse_iterator = std::reverse_iterator<iterator>;
      //! Const reverse iterator type
      using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    public:
      trivial_vector_impl() = default;
      using trivial_vector_impl<false, T>::trivial_vector_impl;
      //! Filling constructor of default constructed `value_type`
      explicit trivial_vector_impl(size_type count)
          : trivial_vector_impl(count, value_type{})
      {
      }
      using trivial_vector_impl<false, T>::resize;
      //! Resizes container, filling any new items with default constructed `value_type`
      void resize(size_type count) { return resize(count, value_type{}); }
    };
  }  // namespace detail

/*! \class trivial_vector
\brief Provides a constant time capacity expanding move-only STL vector. Requires `T` to be
trivially copyable.

As a hand waving estimate for whether this vector implementation may be useful to you,
it roughly breaks even with `std::vector` on recent Intel CPUs at around the L2 cache
level. So if your vector fits into the L2 cache, this implementation will be no
better, and likely slower.

Note that no STL allocator support is provided as `T` must be trivially copyable
(for which most STL's simply use `memcpy()` anyway instead of the allocator's
`construct`), and an internal `section_handle` is used for the storage in order
to implement the constant time capacity resizing.

We also disable the copy constructor, as copying an entire backing file is expensive.
Use the iterator based copy constructor if you really want to copy one of these.

The very first item stored reserves a capacity of `utils::page_size()/sizeof(T)`.
Capacity is doubled in byte terms thereafter (i.e. 8Kb, 16Kb and so on).
Also be aware that the capacity of the vector needs to become reasonably large
before going to the kernel to resize the `section_handle` and remapping memory
becomes faster than `memcpy`. For these reasons, this vector implementation is
best suited to arrays of unknown in advance, but likely large, sizes.
*/
#ifndef DOXYGEN_IS_IN_THE_HOUSE
  template <class T> AFIO_REQUIRES(std::is_trivially_copyable<T>::value) class trivial_vector : public detail::trivial_vector_impl<std::is_default_constructible<T>::value, T>
#else
  template <class T> class trivial_vector : public impl::trivial_vector_impl<true, T>
#endif
  {
    static_assert(std::is_trivially_copyable<T>::value, "trivial_vector: Type T is not trivially copyable!");

  public:
    trivial_vector() = default;
    using detail::trivial_vector_impl<std::is_default_constructible<T>::value, T>::trivial_vector_impl;
  };

  //! Compare
  template <class T> inline bool operator==(const trivial_vector<T> &a, const trivial_vector<T> &b);
  //! Compare
  template <class T> inline bool operator!=(const trivial_vector<T> &a, const trivial_vector<T> &b);
  //! Compare
  template <class T> inline bool operator<(const trivial_vector<T> &a, const trivial_vector<T> &b);
  //! Compare
  template <class T> inline bool operator<=(const trivial_vector<T> &a, const trivial_vector<T> &b);
  //! Compare
  template <class T> inline bool operator>(const trivial_vector<T> &a, const trivial_vector<T> &b);
  //! Compare
  template <class T> inline bool operator>=(const trivial_vector<T> &a, const trivial_vector<T> &b);
  //! Swap
  template <class T> inline void swap(trivial_vector<T> &a, trivial_vector<T> &b) noexcept { a.swap(b); }

}  // namespace algorithm

AFIO_V2_NAMESPACE_END

#endif
