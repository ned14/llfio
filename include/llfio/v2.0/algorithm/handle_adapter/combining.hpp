/* A handle which combines two other handles
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: Oct 2018


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

#ifndef LLFIO_ALGORITHM_HANDLE_ADAPTER_COMBINING_H
#define LLFIO_ALGORITHM_HANDLE_ADAPTER_COMBINING_H

#include "../../map_handle.hpp"

//! \file handle_adapter/combining.hpp Provides `combining_handle_adapter`.

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace algorithm
{
  namespace detail
  {
    // file_handle additional member functions
    struct file_handle_wrapper : public file_handle
    {
      file_handle_wrapper() = default;
      using file_handle::file_handle;
      file_handle_wrapper(native_handle_type nativeh, io_handle::caching _caching, io_handle::flag flags, io_multiplexer *ctx)
          : file_handle(nativeh, 0, 0, _caching, flags, ctx)
      {
      }
    };
    template <class Target, class Source>
    using combining_handle_adapter_choose_base = std::conditional_t<                                                              //
    std::is_base_of<file_handle, Target>::value && (std::is_void<Source>::value || std::is_base_of<file_handle, Source>::value),  //
    file_handle_wrapper,                                                                                                          //
    io_handle                                                                                                                     //
    >;
    template <class A, class B> struct is_void_or_io_request_compatible
    {
      static constexpr bool value = std::is_same<typename A::template io_request<typename A::buffers_type>, typename B::template io_request<typename B::buffers_type>>::value;
    };
    template <class T> struct is_void_or_io_request_compatible<T, void> : std::true_type
    {
    };

    template <template <class, class> class Op, class Target, class Source, class Base, bool with_file_handle_base = std::is_same<Base, file_handle_wrapper>::value> class combining_handle_adapter_base : public Base
    {
      static_assert(is_void_or_io_request_compatible<Target, Source>::value, "Combined handle types do not share io_request<buffers_type>");

    public:
      using path_type = io_handle::path_type;
      using extent_type = io_handle::extent_type;
      using size_type = io_handle::size_type;
      using mode = io_handle::mode;
      using creation = io_handle::creation;
      using caching = io_handle::caching;
      using flag = io_handle::flag;
      using buffer_type = io_handle::buffer_type;
      using const_buffer_type = io_handle::const_buffer_type;
      using buffers_type = io_handle::buffers_type;
      using const_buffers_type = io_handle::const_buffers_type;
      template <class T> using io_request = io_handle::io_request<T>;
      template <class T> using io_result = io_handle::io_result<T>;

      using target_handle_type = Target;
      using source_handle_type = Source;

    protected:
      static constexpr bool _have_source = !std::is_void<source_handle_type>::value;
      using _source_handle_type = std::conditional_t<!_have_source, io_handle, source_handle_type>;

      target_handle_type *_target{nullptr};
      _source_handle_type *_source{nullptr};

    private:
      static constexpr native_handle_type _native_handle(mode _mode)
      {
        native_handle_type nativeh;
        nativeh.behaviour |= native_handle_type::disposition::file;
        nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
        if(_mode == mode::write)
        {
          nativeh.behaviour |= native_handle_type::disposition::writable;
        }
        return nativeh;
      }
      static constexpr caching _combine_caching(target_handle_type *a, _source_handle_type *b)
      {
        caching _a = a->kernel_caching();
        caching _b = b->kernel_caching();
        caching least = caching::none;
        if(_a < _b)
          least = _a;
        else
          least = _b;
        // TODO: Bit 0 set means safety fsyncs on, should I reflect this?
        return least;
      }

    protected:
      combining_handle_adapter_base() = default;
      constexpr combining_handle_adapter_base(target_handle_type *a, _source_handle_type *b, mode _mode, flag flags, io_multiplexer *ctx)
          : Base(_native_handle(_mode), _combine_caching(a, b), flags, ctx)
          , _target(a)
          , _source(b)
      {
      }
      combining_handle_adapter_base(target_handle_type *a, void *b, mode _mode, flag flags, io_multiplexer *ctx)
          : Base(_native_handle(_mode), a->caching(), flags, ctx)
          , _target(a)
          , _source(reinterpret_cast<_source_handle_type *>(b))
      {
      }

    public:
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~combining_handle_adapter_base() override
      {
        // ignore
      }
      //! \brief Close either or both of the combined handles.
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
      {
        OUTCOME_TRY(_target->close());
        if(_have_source)
        {
          OUTCOME_TRY(_source->close());
        }
        return success();
      }

      protected:

      //! \brief Return the lowest of the two handles' maximum buffers
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC size_t _do_max_buffers() const noexcept override
      {
        size_t r = (size_t) -1;
        auto x = _target->max_buffers();
        if(x < r)
          r = x;
        if(_have_source)
        {
          auto y = _source->max_buffers();
          if(y < r)
            r = y;
        }
        return (r == (size_t) -1) ? 1 : r;
      }
      /*! Read from one or both of the attached handles into temporary buffers
      (stack allocated if below a page size), and perform the combining operation
      into the supplied buffers.
      */
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override
      {
        size_type bytes = 0;
        for(const auto &b : reqs.buffers)
        {
          bytes += b.size();
        }
        // If less than page size, use stack, else use free pages
        buffer_type buffers[2] = {{(byte *) ((bytes <= utils::page_size()) ? alloca(bytes + 64) : nullptr), bytes}, {(byte *) ((_have_source && bytes <= utils::page_size()) ? alloca(bytes + 64) : nullptr), bytes}};
        map_handle buffersh;
        if(buffers[0].data() != nullptr)
        {
          // Adjust to 64 byte multiple
          buffers[0] = buffer_type((byte *) (((uintptr_t) buffers[0].data() + 63) & ~63), bytes);
          buffers[1] = buffer_type((byte *) (((uintptr_t) buffers[1].data() + 63) & ~63), bytes);
        }
        else
        {
          auto _bytes = (bytes + 63) & ~63;
          OUTCOME_TRY(auto &&_, map_handle::map(_bytes * (1 + _have_source)));
          buffersh = std::move(_);
          buffers[0] = buffer_type{buffersh.address(), bytes};
          if(_have_source)
          {
            buffers[1] = buffer_type{buffersh.address() + _bytes, bytes};
          }
        }

        // Fill the temporary buffers
        optional<io_result<buffers_type>> _filleds[2];
#if !defined(LLFIO_DISABLE_OPENMP) && defined(_OPENMP)
#pragma omp parallel for if(_have_source && (this->_flags & flag::disable_parallelism) == 0)
#endif
        for(size_t n = 0; n < 2; n++)
        {
          io_request<buffers_type> req({&buffers[n], 1}, reqs.offset);
          if(n == 0)
          {
            _filleds[n] = _target->read(req, d);
          }
          else if(_have_source)
          {
            _filleds[n] = _source->read(req, d);
          }
        }
        // Handle any errors
        buffer_type filleds[2];
        {
          OUTCOME_TRY(auto &&_filled, std::move(*_filleds[0]));
          filleds[0] = std::move(_filled[0]);
        }
        if(_have_source)
        {
          OUTCOME_TRY(auto &&_filled, std::move(*_filleds[1]));
          filleds[1] = std::move(_filled[0]);
        }

        // For each buffer in the request, perform Op, consuming temporary buffers as we go
        for(auto &b : reqs.buffers)
        {
          OUTCOME_TRY(auto &&_b, Op<target_handle_type, source_handle_type>::do_read(b, filleds[0], filleds[1]));
          b = _b;
          filleds[0] = buffer_type{filleds[0].data() + b.size(), filleds[0].size() - b.size()};
          if(_have_source)
          {
            filleds[1] = buffer_type{filleds[1].data() + b.size(), filleds[1].size() - b.size()};
          }
        }
        return std::move(reqs.buffers);
      }

      /*! Perform the uncombining operation to the supplied buffers, splitting them
      into one or two temporary buffers (stack allocated if below a page size),
      followed by writing those to one or both of the attached handles.

      \todo Relative deadline is not being adjusted for source read time.
      */
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override
      {
        size_type bytes = 0;
        for(const auto &b : reqs.buffers)
        {
          bytes += b.size();
        }
        // If less than page size, use stack, else use free pages
        buffer_type buffers[2] = {{(byte *) ((bytes <= utils::page_size()) ? alloca(bytes + 64) : nullptr), bytes}, {(byte *) ((_have_source && bytes <= utils::page_size()) ? alloca(bytes + 64) : nullptr), bytes}};
        map_handle buffersh;
        if(buffers[0].data() != nullptr)
        {
          // Adjust to 64 byte multiple
          buffers[0] = buffer_type((byte *) (((uintptr_t) buffers[0].data() + 63) & ~63), bytes);
          buffers[1] = buffer_type((byte *) (((uintptr_t) buffers[1].data() + 63) & ~63), bytes);
        }
        else
        {
          auto _bytes = (bytes + 63) & ~63;
          OUTCOME_TRY(auto &&_, map_handle::map(_bytes * (1 + _have_source)));
          buffersh = std::move(_);
          buffers[0] = buffer_type{buffers[0].data(), bytes};
          if(_have_source)
          {
            buffers[1] = buffer_type{buffers[1].data() + _bytes, bytes};
          }
        }
        buffer_type tempbuffers[2] = {buffers[0], buffers[1]};

        // Read the source if we have one
        if(_have_source)
        {
          io_request<buffers_type> req({&buffers[1], 1}, reqs.offset);
          OUTCOME_TRY(auto &&_, _source->read(req, d));
          tempbuffers[1] = buffer_type{_[0].data(), _[0].size()};
        }

        // For each buffer in the request, perform Op, filling temporary buffers as we go
        for(auto &b : reqs.buffers)
        {
          // Returns buffers to be written into target
          OUTCOME_TRY(auto &&_b, Op<target_handle_type, source_handle_type>::do_write(tempbuffers[0], tempbuffers[1], b));
          // Adjust inputs to match
          tempbuffers[0] = buffer_type{tempbuffers[0].data() + _b.size(), tempbuffers[0].size() - _b.size()};
          if(_have_source)
          {
            tempbuffers[1] = buffer_type{tempbuffers[1].data() + _b.size(), tempbuffers[1].size() - _b.size()};
          }
        }
        // Resize buffers to what was written
        buffers[0] = buffer_type(buffers[0].data(), tempbuffers[0].data() - buffers[0].data());
        buffers[1] = buffer_type(buffers[1].data(), tempbuffers[1].data() - buffers[1].data());

        // Write the temporary buffer, and adjust the buffers written we return to match
        {
          const_buffer_type b(buffers[0]);
          io_request<const_buffers_type> req({&b, 1}, reqs.offset);
          OUTCOME_TRY(auto &&_, _target->write(req, d));
          OUTCOME_TRY((Op<target_handle_type, source_handle_type>::adjust_written_buffers(reqs.buffers, _[0], buffers[0])));
        }
        return std::move(reqs.buffers);
      }

    public:
    };
    template <template <class, class> class Op, class Target, class Source> class combining_handle_adapter_base<Op, Target, Source, file_handle_wrapper, true> : public combining_handle_adapter_base<Op, Target, Source, file_handle_wrapper, false>
    {
      using _base = combining_handle_adapter_base<Op, Target, Source, file_handle_wrapper, false>;

    protected:
      static constexpr bool _have_source = _base::_have_source;

    public:
      using path_type = io_handle::path_type;
      using extent_type = io_handle::extent_type;
      using size_type = io_handle::size_type;
      using mode = io_handle::mode;
      using creation = io_handle::creation;
      using caching = io_handle::caching;
      using flag = io_handle::flag;
      using buffer_type = io_handle::buffer_type;
      using const_buffer_type = io_handle::const_buffer_type;
      using buffers_type = io_handle::buffers_type;
      using const_buffers_type = io_handle::const_buffers_type;
      template <class T> using io_request = io_handle::io_request<T>;
      template <class T> using io_result = io_handle::io_result<T>;

      combining_handle_adapter_base() = default;
      using _base::_base;

      using extent_guard = typename _base::extent_guard;

    private:
      struct _extent_guard : public extent_guard
      {
        friend class combining_handle_adapter;
        _extent_guard() = default;
        constexpr _extent_guard(file_handle *h, extent_type offset, extent_type length, lock_kind kind)
            : extent_guard(h, offset, length, kind)
        {
        }
      };

    public:
      //! \brief Lock the given extent in one or both of the attached handles. Any second handle is always locked for shared.
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_guard> lock_file_range(extent_type offset, extent_type bytes, lock_kind kind, deadline d = deadline()) noexcept override
      {
        optional<result<extent_guard>> _locks[2];
#if !defined(LLFIO_DISABLE_OPENMP) && defined(_OPENMP)
#pragma omp parallel for if(_have_source && (this->_flags & flag::disable_parallelism) == 0)
#endif
        for(size_t n = 0; n < 2; n++)
        {
          if(n == 0)
          {
            _locks[n] = this->_target->lock_file_range(offset, bytes, kind, d);
          }
          else if(_have_source)
          {
            _locks[n] = this->_source->lock_file_range(offset, bytes, lock_kind::shared, d);
          }
        }
        // Handle any errors
        {
          OUTCOME_TRY(auto &&_, std::move(*_locks[0]));
          _.release();
        }
        if(_have_source)
        {
          OUTCOME_TRY(auto &&_, std::move(*_locks[1]));
          _.release();
        }
        return _extent_guard(this, offset, bytes, kind);
      }
      //! \brief Unlock the given extent in one or both of the attached handles.
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock_file_range(extent_type offset, extent_type bytes) noexcept override
      {
        this->_target->unlock_file_range(offset, bytes);
        if(_have_source)
        {
          this->_source->unlock_file_range(offset, bytes);
        }
      }

      //! \brief Return the lesser of one or both of the attached handles
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> maximum_extent() const noexcept override
      {
        extent_type r = (extent_type) -1;
        OUTCOME_TRY(auto &&x, this->_target->maximum_extent());
        if(x < r)
          r = x;
        if(_have_source)
        {
          OUTCOME_TRY(auto &&y, this->_source->maximum_extent());
          if(y < r)
            r = y;
        }
        return r;
      }
      //! \brief Truncate one or both of the attached handles
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> truncate(extent_type newsize) noexcept override
      {
        optional<result<extent_type>> r[2];
#if !defined(LLFIO_DISABLE_OPENMP) && defined(_OPENMP)
#pragma omp parallel for if(_have_source && (this->_flags & flag::disable_parallelism) == 0)
#endif
        for(size_t n = 0; n < 2; n++)
        {
          if(n == 0)
          {
            r[n] = this->_target->truncate(newsize);
          }
          else if(_have_source)
          {
            r[n] = this->_source->truncate(newsize);
          }
        }
        extent_type ret = (extent_type) -1;
        // Handle any errors
        {
          OUTCOME_TRY(auto &&_, std::move(*r[0]));
          if(_ < ret)
            ret = _;
        }
        if(_have_source)
        {
          OUTCOME_TRY(auto &&_, std::move(*r[1]));
          if(_ < ret)
            ret = _;
        }
        return ret;
      }
      //! \brief Always returns a failed matching `errc::operation_not_supported` as the meaning of combined valid extents is hard to discern here.
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<std::vector<file_handle::extent_pair>> extents() const noexcept override { return errc::operation_not_supported; }
      //! \brief Punches a hole in one or both attached handles. Note that no combination operation is performed.
      LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> zero(file_handle::extent_pair extent, deadline d = deadline()) noexcept override
      {
        optional<result<extent_type>> r[2];
#if !defined(LLFIO_DISABLE_OPENMP) && defined(_OPENMP)
#pragma omp parallel for if(_have_source && (this->_flags & flag::disable_parallelism) == 0)
#endif
        for(size_t n = 0; n < 2; n++)
        {
          if(n == 0)
          {
            r[n] = this->_target->zero(extent, d);
          }
          else if(_have_source)
          {
            r[n] = this->_source->zero(extent, d);
          }
        }
        extent_type ret = (extent_type) -1;
        // Handle any errors
        {
          OUTCOME_TRY(auto &&_, std::move(*r[0]));
          if(_ < ret)
            ret = _;
        }
        if(_have_source)
        {
          OUTCOME_TRY(auto &&_, std::move(*r[1]));
          if(_ < ret)
            ret = _;
        }
        return ret;
      }
    };
  }

  /*! \class combining_handle_adapter
  \brief A handle combining the data from one or two other handles.
  \tparam Op Policy class determining what kind of combination ought to be performed.
  \tparam Target The type of the target handle.
  \tparam Source The type of an optional additional source handle, or `void` to disable.

  \warning This class is still in development, do not use.

  This adapter class is a handle implementation which combines one or two other handle
  implementations in some way determined by `Op` which must match the form of:

  ~~~cpp
  template<class Target, class Source> struct Op
  {
    using buffer_type = typename Target::buffer_type;
    using const_buffer_type = typename Target::const_buffer_type;

    // Called by default implementation of read() to perform combines of reads
    static result<buffer_type> do_read(buffer_type out, buffer_type t, buffer_type s) noexcept;
    // Called by default implementation of write() to perform combines of writes
    static result<const_buffer_type> do_write(buffer_type t, buffer_type s, const_buffer_type in) noexcept;
    // Called by default implementation of write() to adjust returned buffers
    static result<const_buffers_type> adjust_written_buffers(const_buffers_type out, const_buffer_type twritten, const_buffer_type toriginal) noexcept;

    // Inherited into the resulting combining_handle_adapter
    // Used to inject/override custom member functions and/or eliminate the need for
    // do_read and do_write
    template<class Base> struct override : public Base { using Base::Base; };
  };
  ~~~

  If both input handle types have a base of `file_handle`, `combining_handle_adapter`
  inherits from `file_handle` and provides the extra member functions which `file_handle`
  provides over `io_handle`. If not, it inherits from `io_handle`, and provides that
  class' reduced functionality instead.

  The default implementation of `read()` and `write()` allocate temporary buffers, and
  run `Op::do_read()` and `Op::do_write()` on each individual buffer issued by the end
  user of the combined handles. If each total request is below a page size, the stack
  is used, else `map_handle::map()` is used to get whole pages.

  \note If OpenMP is available, `LLFIO_DISABLE_OPENMP` is not defined, and
  `flag::disable_parallelism` is not set, the buffer fill from the two attached handles
  will be done concurrently.

  Combined reads may read less than inputs, but note that offset and buffers fetched
  from inputs are those of the request. Combined writes may write less than inputs,
  but again offset used is that of the request. In other words, this adapter is intended
  for bulk, mostly 1-to-1, combination and transformation of scatter gather buffers. It
  is not intended for processing scatter gather buffers.

  If just one handle type is supplied (the additional source type is `void`), then instead of
  combining, this handle adapter is transforming. `Op::do_read()` and `Op::do_write()`
  will be called with the second of the input buffers empty.

  The defaults for the virtual functions may not suit your use case, in which
  case you can override them in the `Op::override` class.

  Destroying the adapter does not destroy the attached handles. Closing the adapter
  does close the attached handles.

  \todo I have been lazy and used public inheritance from `io_handle` and `file_handle`.
  I should use protected inheritance to prevent slicing, and expose all the public functions by hand.
  */
  template <template <class, class> class Op, class Target, class Source> class combining_handle_adapter : public Op<Target, Source>::template override_<detail::combining_handle_adapter_base<Op, Target, Source, detail::combining_handle_adapter_choose_base<Target, Source>>>
  {
    using _base = typename Op<Target, Source>::template override_<detail::combining_handle_adapter_base<Op, Target, Source, detail::combining_handle_adapter_choose_base<Target, Source>>>;

  public:
    using path_type = io_handle::path_type;
    using extent_type = io_handle::extent_type;
    using size_type = io_handle::size_type;
    using mode = io_handle::mode;
    using creation = io_handle::creation;
    using caching = io_handle::caching;
    using flag = io_handle::flag;
    using buffer_type = io_handle::buffer_type;
    using const_buffer_type = io_handle::const_buffer_type;
    using buffers_type = io_handle::buffers_type;
    using const_buffers_type = io_handle::const_buffers_type;
    template <class T> using io_request = io_handle::io_request<T>;
    template <class T> using io_result = io_handle::io_result<T>;

    using target_handle_type = Target;
    using source_handle_type = Source;

  public:
    //! Default constructor
    combining_handle_adapter() = default;
    //! Constructor, passing any extra arguments to `Op::override`.
    template <class... Args>
    combining_handle_adapter(target_handle_type *a, source_handle_type *b, mode _mode = mode::write, flag flags = flag::none, io_multiplexer *ctx = nullptr, Args &&... args)
        : _base(a, b, _mode, flags, ctx, std::forward<Args>(args)...)
    {
    }

    //! Implicit move construction of combining_handle_adapter permitted
    combining_handle_adapter(combining_handle_adapter &&o) noexcept : _base(std::move(o)) {}
    //! No copy construction (use `clone()`)
    combining_handle_adapter(const combining_handle_adapter &) = delete;
    //! Move assignment of combining_handle_adapter permitted
    combining_handle_adapter &operator=(combining_handle_adapter &&o) noexcept
    {
      if(this == &o)
      {
        return *this;
      }
      this->~combining_handle_adapter();
      new(this) combining_handle_adapter(std::move(o));
      return *this;
    }
    //! No copy assignment
    combining_handle_adapter &operator=(const combining_handle_adapter &) = delete;
    //! Swap with another instance
    LLFIO_MAKE_FREE_FUNCTION
    void swap(combining_handle_adapter &o) noexcept
    {
      combining_handle_adapter temp(std::move(*this));
      *this = std::move(o);
      o = std::move(temp);
    }
  };

  // BEGIN make_free_functions.py

  // END make_free_functions.py

}  // namespace algorithm

LLFIO_V2_NAMESPACE_END

#endif
