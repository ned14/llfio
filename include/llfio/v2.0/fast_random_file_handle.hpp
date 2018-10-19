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

//! \file fast_random_file_handle.hpp Provides `fast_random_file_handle`.

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

/*! \class fast_random_file_handle
\brief A handle to synthesised, non-cryptographic, pseudo-random data.

This implementation of file handle provides read-only file data of random bits
based on Bob Jenkin's 128-bit SpookyHash (http://burtleburtle.net/bob/hash/spooky.html).
It works by initialising the hash state from the 128-bit seed you provide at construction,
and then for each 16 byte block it copies the 304 bytes of hash state, adds the scatter buffer
offset divided by 16 to the hash, generating a unique 128-bit hash for that seed and
file offset. SpookyHash is not a cryptographically secure hash, so do not use this
class for anything secure. However it certainly ought to produce very random data
indeed, as SpookyHash is an excellent quality fast hash.

Note that writes to this handle are permitted if it was opened with write permission,
but writes have no effect.

The use for a file handle full of very random data may not be obvious. The first is
to obfuscate another file's data using `algorithm::xor_file_handle`. The second is
for mock ups in testing, where this file handle stands in for some other (large) file,
and you are testing throughput or latency in processing code.

The third is for unit testing randomly corrupted file data. `algorithm::mix_file_handle`
can randomly mix scatter gather buffers from this file handle into another file handle
in order to test how well handling code copes with random data corruption.

VS2017:
- 416Mb/sec static function
- 539Mb/sec preinit, add, finalise
*/
class fast_random_file_handle : public file_handle
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
  uint64_t _iv[12]{};  // sc_blockSize from SpookyHash
  extent_type _length{0};

  result<void> _perms_check() const noexcept
  {
    if(!this->is_writable())
    {
      return errc::permission_denied;
    }
    return success();
  }

public:
  //! Default constructor
  constexpr fast_random_file_handle() {}  // NOLINT
                                          //! Constructor. Seed can be of up to 88 bytes.
  fast_random_file_handle(extent_type length, span<const byte> seed)
      : _length(length)
  {
    memcpy(&_iv[1], seed.begin(), (seed.size() < (sizeof(_iv) - sizeof(_iv[0]))) ? seed.size() : (sizeof(_iv) - sizeof(_iv[0])));
  }

  //! Implicit move construction of fast_random_file_handle permitted
  fast_random_file_handle(fast_random_file_handle &&o) noexcept : _length(o._length) { memcpy(_iv, o._iv, sizeof(_iv)); }
  //! No copy construction (use `clone()`)
  fast_random_file_handle(const fast_random_file_handle &) = delete;
  //! Move assignment of fast_random_file_handle permitted
  fast_random_file_handle &operator=(fast_random_file_handle &&o) noexcept
  {
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
  static inline result<fast_random_file_handle> fast_random_file(extent_type bytes, mode _mode = mode::read, span<const byte> seed = {}) noexcept
  {
    if(_mode == mode::append)
    {
      return errc::invalid_argument;
    }
    byte _seed[88];
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
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> truncate(extent_type newsize) noexcept override
  {
    OUTCOME_TRY(_perms_check());
    _length = newsize;
    return _length;
  }

  //! \brief Zero a portion of the random file (does nothing).
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<extent_type> zero(extent_type /*unused*/, extent_type bytes, deadline /*unused*/ = deadline()) noexcept override
  {
    OUTCOME_TRY(_perms_check());
    return bytes;
  }

  using file_handle::read;
  using file_handle::write;

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
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<buffers_type> read(io_request<buffers_type> reqs, deadline /* unused */ = deadline()) noexcept override;

  /*! \brief Fails to write to the random file.

  If the handle was not opened with write permissions, this will fail with a code comparing equal to `errc::permission_denied`.
  Otherwise the writes have no effect, and the buffers returned have all zero lengths.

  \return The buffers written with all buffer lengths zeroed.
  \param reqs A scatter-gather and offset request.
  \param d Ignored.
  \errors None possible if handle was opened with write permissions.
  \mallocs None possible.
  */
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC io_result<const_buffers_type> write(io_request<const_buffers_type> reqs, deadline /* unused */ = deadline()) noexcept override
  {
    OUTCOME_TRY(_perms_check());
    // Return null written
    for(auto &buffer : reqs.buffers)
    {
      buffer = {buffer.data(), 0};
    }
    return std::move(reqs.buffers);
  }
};

//! \brief Constructor for `fast_random_file_handle`
template <> struct construct<fast_random_file_handle>
{
  fast_random_file_handle::extent_type bytes{0};
  fast_random_file_handle::mode _mode{fast_random_file_handle::mode::read};
  span<const byte> seed{};
  result<fast_random_file_handle> operator()() const noexcept { return fast_random_file_handle::fast_random_file(bytes, _mode, seed); }
};

// BEGIN make_free_functions.py

// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/fast_random_file_handle.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#endif
