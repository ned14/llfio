/* A view of a path to something
(C) 2017-2020 Niall Douglas <http://www.nedproductions.biz/> (20 commits)
File Created: Jul 2017


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

#include "../../path_view.hpp"

#include <locale>

#ifdef _WIN32
#include "windows/import.hpp"
#endif

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"  // for codecvt
#endif

LLFIO_V2_NAMESPACE_BEGIN

namespace detail
{
  template <class T> inline T *cast_char8_t_ptr(T *v) { return v; }
  template <class InternT, class ExternT> struct _codecvt : std::codecvt<InternT, ExternT, std::mbstate_t>
  {
    template <class... Args>
    _codecvt(Args &&... args)
        : std::codecvt<InternT, ExternT, std::mbstate_t>(std::forward<Args>(args)...)
    {
    }
    ~_codecvt() {}
  };
#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED
  inline const char *cast_char8_t_ptr(const char8_t *v) { return (const char *) v; }
  inline char *cast_char8_t_ptr(char8_t *v) { return (char *) v; }
  template <> struct _codecvt<char8_t, char> : std::codecvt<char, char, std::mbstate_t>
  {
    template <class... Args>
    _codecvt(Args &&... args)
        : std::codecvt<char, char, std::mbstate_t>(std::forward<Args>(args)...)
    {
    }
    ~_codecvt() {}
  };
#endif
  template <class InT, class OutT> static _codecvt<InT, OutT> &_get_codecvt() noexcept
  {
#ifdef _MSC_VER
    static_assert(std::is_same<OutT, char>::value || std::is_same<OutT, char8_t>::value || std::is_same<OutT, wchar_t>::value,
                  "MSVC only supports OutT = char|char8_t|wchar_t!");
#else
    static_assert(std::is_same<OutT, char>::value || std::is_same<OutT, char8_t>::value, "Standard C++ only supports OutT = char|char8_t!");
#endif
    static _codecvt<InT, OutT> ret;
    return ret;
  }

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127)  // conditional expression is constant
#endif
  /* Returns where the last character was written to.
  toallocate may be written with how much must be written if there wasn't enough space.
  */
  template <class DestT, class SrcT>
  inline DestT *_reencode_path_to(size_t &toallocate, DestT *dest_buffer, size_t dest_buffer_length, const SrcT *src_buffer, size_t src_buffer_length,
                                  const std::locale *loc)
  {
    auto &convert = (loc == nullptr) ? _get_codecvt<SrcT, DestT>() : std::use_facet<_codecvt<SrcT, DestT>>(*loc);
    std::mbstate_t cstate{};
    auto *src_ptr = src_buffer;
    auto *dest_ptr = dest_buffer;

    // Try the internal buffer first
    if(dest_buffer_length != 0)
    {
      // First try the internal buffer, if we overflow, fall back to the allocator
      auto result = convert.out(cstate, src_ptr, src_buffer + src_buffer_length, src_ptr, dest_ptr, dest_buffer + dest_buffer_length - 1, dest_ptr);
      assert(std::codecvt_base::noconv != result);
      if(std::codecvt_base::noconv == result)
      {
        LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path should never do identity reencoding");
        abort();
      }
      if(std::codecvt_base::error == result)
      {
        // If input is supposed to be valid UTF, barf
        if(std::is_same<SrcT, char8_t>::value || std::is_same<SrcT, char16_t>::value)
        {
          throw std::system_error(make_error_code(std::errc::illegal_byte_sequence));
        }
        // Otherwise proceed anyway :)
        LLFIO_LOG_WARN(nullptr, "path_view_component::rendered_path saw failure to completely convert input encoding");
        result = std::codecvt_base::ok;
      }
      if(std::codecvt_base::ok == result)
      {
        *dest_ptr = 0;
        toallocate = 0;
        return dest_ptr;
      }
    }

    // This is a bit crap, but codecvt is hardly the epitome of good design :(
    toallocate = convert.max_length() * (1 + src_buffer_length);
#ifdef _WIN32
    const size_t required_bytes = toallocate * sizeof(DestT);
    if(required_bytes > 65535)
    {
      LLFIO_LOG_FATAL(nullptr, "Paths exceeding 64Kb are impossible on Microsoft Windows");
      abort();
    }
#endif
    return dest_ptr;
  }
#ifdef _MSC_VER
#pragma warning(pop)
#endif

  char *reencode_path_to(size_t & /*unused*/, char * /*unused*/, size_t /*unused*/, const LLFIO_V2_NAMESPACE::byte * /*unused*/, size_t /*unused*/,
                         const std::locale * /*unused*/)
  {
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function should never see passthrough.");
    abort();
  }
  char *reencode_path_to(size_t & /*unused*/, char * /*unused*/, size_t /*unused*/, const char * /*unused*/, size_t /*unused*/, const std::locale * /*unused*/)
  {
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function should never see identity.");
    abort();
  }
  char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const wchar_t *src_buffer, size_t src_buffer_length,
                         const std::locale *loc)
  {
    return _reencode_path_to(toallocate, dest_buffer, dest_buffer_length, src_buffer, src_buffer_length, loc);
  }
  char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const char8_t *src_buffer, size_t src_buffer_length,
                         const std::locale *loc)
  {
#ifdef _WIN32
    if(dest_buffer_length * sizeof(wchar_t) > 65535)
    {
      LLFIO_LOG_FATAL(nullptr, "Paths exceeding 64Kb are impossible on Microsoft Windows");
      abort();
    }
    // Need to perform a UTF-8 to ANSI conversion using the Windows API
    // *Probably* the safest way of doing this is from UTF-8 to wchar_t,
    // then from wchar_t to ANSI.
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    static thread_local struct state_t
    {
      const char8_t *src_buffer{nullptr};  // src
      UNICODE_STRING buffer{};             // intermediate
      ANSI_STRING str{};                   // out
      ~state_t()
      {
        if(buffer.Buffer != nullptr)
        {
          free(buffer.Buffer);
        }
        if(str.Buffer != nullptr)
        {
          RtlFreeAnsiString(&str);
        }
      }
    } state;
    if(state.src_buffer != nullptr)
    {
      if(state.src_buffer == src_buffer && state.str.Length == dest_buffer_length - 1)
      {
      successful:
        // Use the precalculated reencoding
        memcpy(dest_buffer, state.str.Buffer, state.str.Length);
        dest_buffer[state.str.Length] = 0;
        free(state.buffer.Buffer);
        RtlFreeUnicodeString(&state.buffer);
        state.buffer.Buffer = nullptr;
        state.str.Buffer = nullptr;
        state.src_buffer = nullptr;
        // Successful
        toallocate = 0;
        return dest_buffer + dest_buffer_length - 1;
      }
      free(state.buffer.Buffer);
      RtlFreeAnsiString(&state.str);
      state.buffer.Buffer = nullptr;
      state.str.Buffer = nullptr;
      state.src_buffer = nullptr;
    }
    ULONG written = 0;
    NTSTATUS ntstat;
    if(loc == nullptr)
    {
      // Ask for the length needed
      ntstat = RtlUTF8ToUnicodeN(nullptr, 0, &written, (const char *) src_buffer, static_cast<ULONG>(src_buffer_length));
      if(ntstat < 0)
      {
        LLFIO_LOG_FATAL(ntstat, ntkernel_error(ntstat).message().c_str());
        abort();
      }
      state.buffer.Length = (USHORT) written;
      state.buffer.MaximumLength = (USHORT)(written + sizeof(wchar_t));
      state.buffer.Buffer = (wchar_t *) malloc(state.buffer.MaximumLength);
      if(nullptr == state.buffer.Buffer)
      {
        throw std::bad_alloc();
      }
      // Do the conversion UTF-8 to UTF-16
      ntstat = RtlUTF8ToUnicodeN(state.buffer.Buffer, state.buffer.Length, &written, (const char *) src_buffer, static_cast<ULONG>(src_buffer_length));
      if(ntstat < 0)
      {
        LLFIO_LOG_FATAL(ntstat, ntkernel_error(ntstat).message().c_str());
        abort();
      }
    }
    else
    {
      auto &convert = std::use_facet<_codecvt<char8_t, wchar_t>>(*loc);
      std::mbstate_t cstate{};
      written = (DWORD)(src_buffer_length * convert.max_length() * sizeof(wchar_t));
      state.buffer.Length = (USHORT) written;
      state.buffer.MaximumLength = (USHORT)(written + sizeof(wchar_t));
      state.buffer.Buffer = (wchar_t *) malloc(state.buffer.MaximumLength);
      if(nullptr == state.buffer.Buffer)
      {
        throw std::bad_alloc();
      }
      // Do the conversion UTF-8 to UTF-16
      auto *src_ptr = src_buffer;
      auto *dest_ptr = state.buffer.Buffer;
      auto result =
      convert.out(cstate, src_ptr, src_buffer + src_buffer_length, src_ptr, dest_ptr, state.buffer.Buffer + state.buffer.Length / sizeof(wchar_t), dest_ptr);
      assert(std::codecvt_base::noconv != result);
      if(std::codecvt_base::noconv == result)
      {
        LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path should never do identity reencoding");
        abort();
      }
      if(std::codecvt_base::error == result)
      {
        // If input is supposed to be valid UTF, barf
        throw std::system_error(make_error_code(std::errc::illegal_byte_sequence));
      }
      written = (DWORD)((dest_ptr - state.buffer.Buffer) * sizeof(wchar_t));
      if(written > 65535)
      {
        throw std::system_error(make_error_code(std::errc::no_buffer_space));
      }
      state.buffer.Length = (USHORT) written;
    }
    // Do any / to \ conversion now
    for(size_t n = 0; n < state.buffer.Length / sizeof(wchar_t); n++)
    {
      if(state.buffer.Buffer[n] == '/')
      {
        state.buffer.Buffer[n] = '\\';
      }
    }
    // Convert from UTF-16 to ANSI
    ntstat = AreFileApisANSI() ? RtlUnicodeStringToAnsiString(&state.str, (PCUNICODE_STRING) &state.buffer, true) :
                                 RtlUnicodeStringToOemString(&state.str, (PCUNICODE_STRING) &state.buffer, true);
    if(ntstat < 0)
    {
      LLFIO_LOG_FATAL(ntstat, ntkernel_error(ntstat).message().c_str());
      abort();
    }
    state.str.MaximumLength = state.str.Length + 1;
    if(dest_buffer_length >= state.str.MaximumLength)
    {
      goto successful;
    }
    // Retry me with this dynamic allocation
    state.src_buffer = src_buffer;
    toallocate = state.str.MaximumLength;
    return dest_buffer;
#else
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function should never see identity.");
    abort();
#endif
  }
  char *reencode_path_to(size_t &toallocate, char *dest_buffer, size_t dest_buffer_length, const char16_t *src_buffer, size_t src_buffer_length,
                         const std::locale *loc)
  {
#if(__cplusplus >= 202000 || (_HAS_CXX20 && _MSC_VER >= 1921)) && !defined(_LIBCPP_VERSION)
    return (char *) _reencode_path_to(toallocate, (char8_t *) dest_buffer, dest_buffer_length, src_buffer, src_buffer_length, loc);
#elif defined(_MSC_VER) && _MSC_VER < 1920
    return (char *) _reencode_path_to(toallocate,dest_buffer, dest_buffer_length, (const wchar_t *) src_buffer, src_buffer_length, loc);
#else
    return _reencode_path_to(toallocate, dest_buffer, dest_buffer_length, src_buffer, src_buffer_length, loc);
#endif
  }

  wchar_t *reencode_path_to(size_t & /*unused*/, wchar_t * /*unused*/, size_t /*unused*/, const LLFIO_V2_NAMESPACE::byte * /*unused*/, size_t /*unused*/,
                            const std::locale * /*unused*/)
  {
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function should never see passthrough.");
    abort();
  }
  wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const char *src_buffer, size_t src_buffer_length,
                            const std::locale *loc)
  {
#ifdef _WIN32
    (void) loc;  // can't use on windows
    if(dest_buffer_length * sizeof(wchar_t) > 65535)
    {
      LLFIO_LOG_FATAL(nullptr, "Paths exceeding 64Kb are impossible on Microsoft Windows");
      abort();
    }
    // Need to perform an ANSI to Unicode conversion using the Windows API
    windows_nt_kernel::init();
    using namespace windows_nt_kernel;
    static thread_local struct state_t
    {
      UNICODE_STRING buffer{};  // out
      ANSI_STRING str{};        // in
      ~state_t()
      {
        if(buffer.Buffer != nullptr)
        {
          RtlFreeUnicodeString(&buffer);
        }
      }
    } state;
    if(state.buffer.Buffer != nullptr)
    {
      if(state.str.Buffer == src_buffer && state.buffer.MaximumLength == dest_buffer_length * sizeof(wchar_t))
      {
        // Use the precalculated reencoding
        memcpy(dest_buffer, state.buffer.Buffer, state.buffer.Length);
        dest_buffer[state.buffer.Length] = 0;
        RtlFreeUnicodeString(&state.buffer);
        state.buffer.Buffer = nullptr;
        // Successful
        toallocate = 0;
        return dest_buffer + state.buffer.Length;
      }
      RtlFreeUnicodeString(&state.buffer);
      state.buffer.Buffer = nullptr;
    }
    state.buffer.Buffer = dest_buffer;
    state.buffer.Length = state.buffer.MaximumLength = (USHORT)((dest_buffer_length - 1) * sizeof(wchar_t));
    state.str.Buffer = const_cast<char *>(src_buffer);
    state.str.Length = state.str.MaximumLength = (USHORT) src_buffer_length;
    // Try using the internal buffer
    NTSTATUS ntstat =
    AreFileApisANSI() ? RtlAnsiStringToUnicodeString(&state.buffer, &state.str, false) : RtlOemStringToUnicodeString(&state.buffer, &state.str, false);
    if(ntstat >= 0)
    {
      // Do any / to \ conversion now
      for(size_t n = 0; n < state.buffer.Length / sizeof(wchar_t); n++)
      {
        if(state.buffer.Buffer[n] == '/')
        {
          state.buffer.Buffer[n] = '\\';
        }
      }
      // Successful
      toallocate = 0;
      state.buffer.Buffer[(state.buffer.Length / sizeof(wchar_t))] = 0;
      state.buffer.Buffer = nullptr;
      return dest_buffer + (state.buffer.Length / sizeof(wchar_t));
    }
    if(ntstat != STATUS_BUFFER_OVERFLOW)
    {
      LLFIO_LOG_FATAL(ntstat, ntkernel_error(ntstat).message().c_str());
      abort();
    }
    // Dynamically allocate
    ntstat = AreFileApisANSI() ? RtlAnsiStringToUnicodeString(&state.buffer, &state.str, true) : RtlOemStringToUnicodeString(&state.buffer, &state.str, true);
    if(ntstat < 0)
    {
      LLFIO_LOG_FATAL(ntstat, ntkernel_error(ntstat).message().c_str());
      abort();
    }
    // Do any / to \ conversion now
    for(size_t n = 0; n < state.buffer.Length / sizeof(wchar_t); n++)
    {
      if(state.buffer.Buffer[n] == '/')
      {
        state.buffer.Buffer[n] = '\\';
      }
    }
    // The thread local state will cache this for later
    state.buffer.MaximumLength = state.buffer.Length + sizeof(wchar_t);
    // Retry me with this dynamic allocation
    toallocate = state.buffer.MaximumLength / sizeof(wchar_t);
    return dest_buffer;
#elif defined(_LIBCPP_VERSION)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char to wchar_t conversion on libc++.");
    abort();
#elif defined(__GLIBCXX__)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char to wchar_t conversion on libstdc++.");
    abort();
#else
    return _reencode_path_to(toallocate, dest_buffer, dest_buffer_length, src_buffer, src_buffer_length, loc);
#endif
  }
  wchar_t *reencode_path_to(size_t & /*unused*/, wchar_t * /*unused*/, size_t /*unused*/, const wchar_t * /*unused*/, size_t /*unused*/,
                            const std::locale * /*unused*/)
  {
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function should never see identity.");
    abort();
  }
  wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const char8_t *src_buffer, size_t src_buffer_length,
                            const std::locale *loc)
  {
#if LLFIO_PATH_VIEW_CHAR8_TYPE_EMULATED
#if defined(_LIBCPP_VERSION)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char8_t to wchar_t conversion on libc++.");
    abort();
#elif defined(__GLIBCXX__)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char8_t to wchar_t conversion on libstdc++.");
    abort();
#else
    return _reencode_path_to(toallocate, dest_buffer, dest_buffer_length, (const char *) src_buffer, src_buffer_length, loc);
#endif
#else
#if defined(_LIBCPP_VERSION)
    // libc++ appears to be missing anything to wchar_t codecvt entirely
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char8_t to wchar_t conversion on libc++.");
    abort();
#elif defined(__GLIBCXX__)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char8_t to wchar_t conversion on libstdc++.");
    abort();
#else
    return _reencode_path_to(toallocate, dest_buffer, dest_buffer_length, src_buffer, src_buffer_length, loc);
#endif
#endif
  }
  wchar_t *reencode_path_to(size_t &toallocate, wchar_t *dest_buffer, size_t dest_buffer_length, const char16_t *src_buffer, size_t src_buffer_length,
                            const std::locale *loc)
  {
#ifdef _WIN32
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function should never see identity.");
    abort();
#elif defined(_LIBCPP_VERSION)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char16_t to wchar_t conversion on libc++.");
    abort();
#elif defined(__GLIBCXX__)
    (void) toallocate;
    (void) dest_buffer;
    (void) dest_buffer_length;
    (void) src_buffer;
    (void) src_buffer_length;
    (void) loc;
    LLFIO_LOG_FATAL(nullptr, "path_view_component::rendered_path reencoding function does not support char16_t to wchar_t conversion on libstdc++.");
    abort();
#else
    return _reencode_path_to(toallocate, dest_buffer, dest_buffer_length, src_buffer, src_buffer_length, loc);
#endif
  }

}  // namespace detail

LLFIO_V2_NAMESPACE_END

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

