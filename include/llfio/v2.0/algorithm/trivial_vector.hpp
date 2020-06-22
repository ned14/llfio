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

#ifndef LLFIO_ALGORITHM_VECTOR_HPP
#define LLFIO_ALGORITHM_VECTOR_HPP

#include "../map_handle.hpp"
#include "../utils.hpp"


//! \file trivial_vector.hpp Provides constant time reallocating STL vector.

LLFIO_V2_NAMESPACE_BEGIN

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
      constexpr trivial_vector_impl() {}  // NOLINT
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
        if(this == &o)
        {
          return *this;
        }
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
        bytes = utils::round_up_to_page_size(bytes, utils::page_size());
        if(!_sh.is_valid())
        {
          _sh = section_handle::section(bytes).value();
          _mh = map_handle::map(_sh, bytes).value();
        }
        else if(n > capacity())
        {
          // We can always grow a section even with maps open on it
          _sh.truncate(bytes).value();
          // Attempt to resize the map in place
          if(!_mh.truncate(bytes, true))
          {
            // std::cerr << "truncate fail" << std::endl;
            // If can't resize, close the map and reopen it into a new address
            _mh.close().value();
            _mh = map_handle::map(_sh, bytes).value();
          }
        }
        else
        {
          return;
        }
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
        bytes = utils::round_up_to_page_size(bytes, _mh.page_size());
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
        // Trivially copyable, so memmove and we know copy construction can't fail
        memmove(pos._v + count, pos._v, (_end - pos._v) * sizeof(value_type));
        for(size_type n = 0; n < count; n++)
        {
          new(pos._v + n) value_type(v);
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
        // Trivially copyable, so memmove and we know copy construction can't fail
        memmove(pos._v + count, pos._v, (_end - pos._v) * sizeof(value_type));
        for(size_type n = 0; n < count; n++)
        {
          new(pos._v + n) value_type(*first++);
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
        // Trivially copyable, so memmove
        memmove(pos._v + 1, pos._v, (_end - pos._v) * sizeof(value_type));
        // BUT complex constructors may throw!
        try
        {
          new(pos._v) value_type(std::forward<Args>(args)...);
        }
        catch(...)
        {
          memmove(pos._v, pos._v + 1, (_end - pos._v) * sizeof(value_type));
          throw;
        }
        ++_end;
        return iterator(pos._v);
      }

      //! Erases item
      iterator erase(const_iterator pos)
      {
        // Trivially copyable
        memmove(pos._v, pos._v + 1, ((_end - 1) - pos._v) * sizeof(value_type));
        --_end;
        return iterator(pos._v);
      }
      //! Erases items
      iterator erase(const_iterator first, const_iterator last)
      {
        // Trivially copyable
        memmove(first._v, last._v, (_end - last._v) * sizeof(value_type));
        _end -= last._v - first._v;
        return iterator(first._v);
      }
      //! Appends item
      void push_back(const value_type &v)
      {
        if(_end == _capacity)
        {
          reserve(_scale_capacity(capacity()));
        }
        new(_end++) value_type(v);
      }
      //! Appends item
      void push_back(value_type &&v) { push_back(v); }
      //! Appends item
      template <class... Args> reference emplace_back(Args &&... args)
      {
        if(_end == _capacity)
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
        while(count - size() >= 16)
        {
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
          new(_end++) value_type(v);
        }
        while(count > size())
        {
          new(_end++) value_type(v);
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
      constexpr trivial_vector_impl() {}  // NOLINT
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
it usually roughly breaks even with `std::vector` on recent Intel CPUs at around the L2 cache
boundary. So if your vector fits into the L2 cache, this implementation will be no
better, but no worse. If your vector fits into the L1 cache, this implementation will be
worse, often considerably so.

Note that no STL allocator support is provided as `T` must be trivially copyable
(for which most STL's simply use `memcpy()` anyway instead of the allocator's
`construct`), and an internal `section_handle` is used for the storage in order
to implement the constant time capacity resizing.

We also disable the copy constructor, as copying an entire backing file is expensive.
Use the iterator based copy constructor if you really want to copy one of these.

The very first item stored reserves a capacity of `utils::page_size()/sizeof(T)`
on POSIX and `65536/sizeof(T)` on Windows.
Capacity is doubled in byte terms thereafter (i.e. 8Kb, 16Kb and so on).
As mentioned earlier, the capacity of the vector needs to become reasonably large
before going to the kernel to resize the `section_handle` and remapping memory
becomes faster than `memcpy`. For these reasons, this vector implementation is
best suited to arrays of unknown in advance, but likely large, sizes.

Benchmarking notes for Skylake 3.1Ghz Intel Core i5 with 2133Mhz DDR3 RAM, L2 256Kb,
L3 4Mb:
- OS X with clang 5.0 and libc++
  - push_back():
    32768,40,59
    65536,36,76
    131072,78,159
    262144,166,198
    524288,284,299
    1048576,558,572
    2097152,1074,1175
    4194304,2201,2394
    8388608,4520,5503
    16777216,9339,9327
    33554432,24853,17754
    67108864,55876,40973
    134217728,122577,86331
    268435456,269004,178751
    536870912,586466,330716
  - resize():
    8192,9,18
    16384,14,20
    32768,27,32
    65536,66,43
    131072,107,59
    262144,222,131
    524288,445,322
    1048576,885,500
    2097152,1785,1007
    4194304,3623,2133
    8388608,7334,4082
    16777216,17096,8713
    33554432,36890,18421
    67108864,73593,40702
- Windows 10 with VS2017.5:
  - push_back():
    8192,17,70
    16384,22,108
    32768,36,117
    65536,51,152
    131072,174,209
    262144,336,309
    524288,661,471
    1048576,1027,787
    2097152,2513,1361
    4194304,5948,2595
    8388608,9919,4820
    16777216,23789,9716
    33554432,52997,19558
    67108864,86468,39240
    134217728,193013,76298
    268435456,450059,149644
    536870912,983442,318078
  - resize():
    8192,9,48
    16384,17,44
    32768,35,48
    65536,62,72
    131072,134,114
    262144,132,168
    524288,505,330
    1048576,988,582
    2097152,1501,1152
    4194304,2972,2231
    8388608,6122,4436
    16777216,12483,8906
    33554432,25203,17847
    67108864,52005,53646
    134217728,102942,86502
    268435456,246367,177999
    536870912,405524,294685
  */
#ifndef DOXYGEN_IS_IN_THE_HOUSE
  template <class T> LLFIO_REQUIRES(std::is_trivially_copyable<T>::value) class trivial_vector : public detail::trivial_vector_impl<std::is_default_constructible<T>::value, T>
#else
  template <class T> class trivial_vector : public impl::trivial_vector_impl<true, T>
#endif
  {
    static_assert(std::is_trivially_copyable<T>::value, "trivial_vector: Type T is not trivially copyable!");

  public:
    constexpr trivial_vector() {}  // NOLINT
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

LLFIO_V2_NAMESPACE_END

#endif
