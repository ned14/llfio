/* A source of byte-orientated socket handles
(C) 2021-2021 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Dec 2021


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

#ifndef LLFIO_BYTE_SOCKET_SOURCE_H
#define LLFIO_BYTE_SOCKET_SOURCE_H

#include "byte_io_handle.hpp"

//! \file byte_socket_source.hpp Provides a source of `byte_socket_handle`.

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nameless struct/union
#pragma warning(disable : 4251)  // dll interface
#endif

LLFIO_V2_NAMESPACE_EXPORT_BEGIN

namespace detail
{
  struct LLFIO_DECL byte_socket_source_handle_deleter
  {
    void operator()(byte_socket_handle *p) const;
  };
  struct LLFIO_DECL listening_socket_source_handle_deleter
  {
    void operator()(listening_socket_handle *p) const;
  };
}  // namespace detail

//! A pointer to a byte socket handle returned by a byte socket source
using byte_socket_source_handle_ptr = std::unique_ptr<byte_socket_handle, detail::byte_socket_source_handle_deleter>;
//! A pointer to a listening socket handle returned by a byte socket source
using listening_socket_source_handle_ptr = std::unique_ptr<listening_socket_handle, detail::listening_socket_source_handle_deleter>;

/*! \class byte_socket_source
\brief A source of `byte_socket_handle` and `listening_socket` and optionally a `byte_io_multiplexer`
to multiplex i/o on multiple instances at the same time.

The socket handles returned by this source may be _very_ different implementations
to the kernel socket handles returned by `byte_socket_handle::byte_socket()` -- they
may have all virtual functions overridden AND a custom i/o multiplexer set, plus RTTI
may show an internal derived type from the public types.

However, they may still be a valid kernel socket handle e.g. a TLS implementation
may act upon a base kernel socket handle. If so, `is_kernel_handle()` will be true.
*/
class byte_socket_source
{
  byte_io_multiplexer_ptr _multiplexer;

public:
  using check_for_any_completed_io_statistics = byte_io_multiplexer::check_for_any_completed_io_statistics;

public:
  virtual ~byte_socket_source();

  //! Returns the i/o multiplexer for this byte socket handle source. All handles created from this source will have this multiplexer set upon them.
  byte_io_multiplexer *multiplexer() const noexcept { return _multiplexer.get(); }

  //! Convenience indirect to `multiplexer()->check_for_any_completed_io()`
  result<check_for_any_completed_io_statistics> check_for_any_completed_io(deadline d = std::chrono::seconds(0), size_t max_completions = (size_t) -1) noexcept
  {
    return multiplexer()->check_for_any_completed_io(d, max_completions);
  }

  //! Returns a pointer to a new `byte_socket_handle` instance, which may be a recycled instance recently deleted.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<byte_socket_source_handle_ptr>
  byte_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
              byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
              byte_socket_handle::flag flags = byte_socket_handle::flag::none) noexcept = 0;

  //! Returns a pointer to a new `listening_socket_handle` instance, which may be a recycled instance recently deleted.
  LLFIO_HEADERS_ONLY_VIRTUAL_SPEC result<listening_socket_source_handle_ptr>
  listening_socket(ip::family family, byte_socket_handle::mode _mode = byte_socket_handle::mode::write,
                   byte_socket_handle::caching _caching = byte_socket_handle::caching::all,
                   byte_socket_handle::flag flags = byte_socket_handle::flag::none) noexcept = 0;
};

// BEGIN make_free_functions.py
// END make_free_functions.py

LLFIO_V2_NAMESPACE_END

#if LLFIO_HEADERS_ONLY == 1 && !defined(DOXYGEN_SHOULD_SKIP_THIS)
#define LLFIO_INCLUDED_BY_HEADER 1
#include "detail/impl/byte_socket_source.ipp"
#undef LLFIO_INCLUDED_BY_HEADER
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
