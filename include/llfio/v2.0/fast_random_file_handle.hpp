/* A handle to a pseudo random file
(C) 2018 Niall Douglas <http://www.nedproductions.biz/> (14 commits)
File Created: Sept 2018


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

#ifndef LLFIO_FAST_RANDOM_FILE_HANDLE_H
#define LLFIO_FAST_RANDOM_FILE_HANDLE_H

#include "file_handle.hpp"

#include "quickcpplib/algorithm/small_prng.hpp"

//! \file fast_random_file_handle.hpp Provides `fast_random_file_handle`.

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4251)  // subclass needs to have dll interface
#endif

/*! \class fast_random_file_handle
\brief A handle to synthesised, non-cryptographic, pseudo-random data.

This implementation of file handle provides read-only file data of random bits
based on Bob Jenkins' 32-bit JSF PRNG (http://burtleburtle.net/bob/rand/smallprng.html).
It works by initialising the prng state from the seed you provide at construction,
and then for each 4 byte block it copies the 16 bytes of prng state, overwrites the
first eight bytes with the offset divided by 4, and performs a PRNG round to generate
a fairly unique number. As there are eight bytes of randomness being mixed with eight
bytes of counter, this will not be a particularly random stream, but it's probably not
awful either.

Note that writes to this handle are permitted if it was opened with write permission,
but writes have no effect.

The use for a file handle full of random data may not be obvious. The first is
to obfuscate another file's data using `algorithm::xor_handle_adapter`. The second is
for mock ups in testing, where this file handle stands in for some other (large) file,
and you are testing throughput or latency in processing code.

The third is for unit testing randomly corrupted file data. `algorithm::mix_handle_adapter`
can randomly mix scatter gather buffers from this file handle into another file handle
in order to test how well handling code copes with random data corruption.

## Benchmarks:

On a 3.1Ghz Intel Skylake CPU where `memcpy()` can do ~12Gb/sec:

- GCC7: 4659 Mb/sec
- VS2017: 3653 Mb/sec

The current implementation spots when it can do 16x simultaneous PRNG rounds, and thus
can fill a cache line at a time. The Skylake CPU used to benchmark the code dispatches
around four times the throughput with this, however there is likely still performance
left on the table.

If someone were bothered to rewrite the JSF PRNG into SIMD, it is possible one could
approach `memcpy()` in performance. One would probably need to use AVX-512 however,
as the JSF PRNG makes heavy use of bit rotation, which is slow before AVX-512 as it
must be emulated with copious bit shifting and masking.
*/
class LLFIO_DECL fast_random_file_handle : public file_handle
{
public:
  using dev_t = file_handle::dev_t;
  using ino_t = file_handle::ino_t;
  using path_view_type = file_handle::path_view_type;
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

protected:
  struct prng : public QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng
  {
    using _base = QUICKCPPLIB_NAMESPACE::algorithm::small_prng::small_prng;
    prng() = default;
    // Construct an instance with `seed`
    explicit prng(span<const byte> seed) noexcept
    {
      a = 0xf1ea5eed;
      b = c = d = 0xdeadbeef;
      memcpy(&a, seed.data(), std::min(seed.size(), sizeof(*this)));
      for(size_t i = 0; i < 20; ++i)
        _base::operator()();
    }
    // Return randomness for a given extent
    _base::value_type operator()(extent_type offset) noexcept
    {
      a = offset & 0xffffffff;
      b = (offset >> 32) & 0xffffffff;
      return _base::operator()();
    }
  } _prng;
  extent_type _length{0};

  result<void> _perms_check() const noexcept
  {
    if(!this->is_writable())
    {
      return errc::permission_denied;
    }
    return success();
  }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC size_t _do_max_buffers() const noexcept override { return 0; }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_barrier(io_request<const_buffers_type> reqs = io_request<const_buffers_type>(), barrier_kind /*unused*/ = barrier_kind::nowait_data_only, deadline /* unused */ = deadline()) noexcept override
  {
    OUTCOME_TRY(_perms_check());
    // Return null written
    for(auto &buffer : reqs.buffers)
    {
      buffer = {buffer.data(), 0};
    }
    return std::move(reqs.buffers);
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> _do_read(io_request<buffers_type> reqs, deadline d = deadline()) noexcept override;
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> _do_write(io_request<const_buffers_type> reqs, deadline d = deadline()) noexcept override
  {
    (void) d;
    OUTCOME_TRY(_perms_check());
    // Return null written
    for(auto &buffer : reqs.buffers)
    {
      buffer = {buffer.data(), 0};
    }
    return std::move(reqs.buffers);
  }

public:
  //! Default constructor
  fast_random_file_handle() = default;
  //! Constructor. Seed is not much use past sixteen bytes.
  fast_random_file_handle(extent_type length, span<const byte> seed)
      : _prng(seed)
      , _length(length)
  {
  }

  //! Implicit move construction of fast_random_file_handle permitted
  fast_random_file_handle(fast_random_file_handle &&o) noexcept : _prng(o._prng), _length(o._length) {}
  //! No copy construction (use `clone()`)
  fast_random_file_handle(const fast_random_file_handle &) = delete;
  //! Move assignment of fast_random_file_handle permitted
  fast_random_file_handle &operator=(fast_random_file_handle &&o) noexcept
  {
    if(this == &o)
    {
      return *this;
    }
    this->~fast_random_file_handle();
    new(this) fast_random_file_handle(std::move(o));
    return *this;
  }
  //! No copy assignment
  fast_random_file_handle &operator=(const fast_random_file_handle &) = delete;
  //! Swap with another instance
  LLFIO_MAKE_FREE_FUNCTION
  void swap(fast_random_file_handle &o) noexcept
  {
    fast_random_file_handle temp(std::move(*this));
    *this = std::move(o);
    o = std::move(temp);
  }

  /*! Create a random file handle.
  \param bytes How long the random file ought to report itself being.
  \param _mode How to open the file.
  \param seed Up to 88 bytes with which to seed the randomness. The default means use `utils::random_fill()`.
  */
  LLFIO_MAKE_FREE_FUNCTION
  static inline result<fast_random_file_handle> fast_random_file(extent_type bytes = (extent_type) -1, mode _mode = mode::read, span<const byte> seed = {}) noexcept
  {
    if(_mode == mode::append)
    {
      return errc::invalid_argument;
    }
    byte _seed[16];
    if(seed.empty())
    {
      utils::random_fill((char *) _seed, sizeof(_seed));
      seed = _seed;
    }
    result<fast_random_file_handle> ret(fast_random_file_handle(bytes, seed));
    native_handle_type &nativeh = ret.value()._v;
    LLFIO_LOG_FUNCTION_CALL(&ret);
    nativeh.behaviour |= native_handle_type::disposition::file;
    nativeh.behaviour |= native_handle_type::disposition::seekable | native_handle_type::disposition::readable;
    if(_mode == mode::write)
    {
      nativeh.behaviour |= native_handle_type::disposition::writable;
    }
    return ret;
  }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC ~fast_random_file_handle() override
  {
    // ignore
  }
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<void> close() noexcept override
  {
    // ignore
    return success();
  }
  //! Return the current maximum permitted extent of the file.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> maximum_extent() const noexcept override { return _length; }

  /*! \brief Resize the current maximum permitted extent of the random file to the given extent.
  */
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> truncate(extent_type newsize) noexcept override
  {
    OUTCOME_TRY(_perms_check());
    _length = newsize;
    return _length;
  }

  //! \brief Zero a portion of the random file (does nothing).
  LLFIO_MAKE_FREE_FUNCTION
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> zero(file_handle::extent_pair extent, deadline /*unused*/ = deadline()) noexcept override
  {
    OUTCOME_TRY(_perms_check());
    return extent.length;
  }

  //! \brief Return a single extent of the maximum extent
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<std::vector<file_handle::extent_pair>> extents() const noexcept override { return std::vector<file_handle::extent_pair>{{0, _length}}; }

#if 0
  /*! \brief Read data from the random file.

  Note that ensuring that the scatter buffers are address and size aligned to 16 byte (128 bit) multiples will give
  maximum performance.

  \return The buffers input, with the size of each scatter-gather buffer updated with the number of bytes of that
  buffer transferred.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None possible.
  \mallocs None possible.
  */
#endif
  using file_handle::read;

#if 0
  /*! \brief Fails to write to the random file.

  If the handle was not opened with write permissions, this will fail with a code comparing equal to `errc::permission_denied`.
  Otherwise the writes have no effect, and the buffers returned have all zero lengths.

  \return The buffers written with all buffer lengths zeroed.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None possible if handle was opened with write permissions.
  \mallocs None possible.
  */
#endif
  using file_handle::write;

private:
  struct _extent_guard : public extent_guard
  {
    friend class fast_random_file_handle;
    using extent_guard::extent_guard;
  };

public:
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_guard> lock_file_range(extent_type offset, extent_type bytes, lock_kind kind, deadline /* unused */ = deadline()) noexcept override
  {
    // Lock nothing
    return _extent_guard(this, offset, bytes, kind);
  }

  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC void unlock_file_range(extent_type /*unused*/, extent_type /*unused*/) noexcept override
  {
    // Unlock nothing
  }
};

//! \brief Constructor for `fast_random_file_handle`
template <> struct construct<fast_random_file_handle>
{
  fast_random_file_handle::extent_type bytes{(fast_random_file_handle::extent_type) -1};
  fast_random_file_handle::mode _mode{fast_random_file_handle::mode::read};
  span<const byte> seed{};
  result<fast_random_file_handle> operator()() const noexcept { return fast_random_file_handle::fast_random_file(bytes, _mode, seed); }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

// BEGIN make_free_functions.py

// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/fast_random_file_handle.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif
