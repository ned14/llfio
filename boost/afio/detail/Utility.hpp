/* 
 * File:   Utility.hpp
 * Author: atlas
 *
 * Created on June 25, 2013, 12:43 PM
 */

#ifndef BOOST_AFIO_UTILITY_HPP
#define BOOST_AFIO_UTILITY_HPP

#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#ifndef S_IFLNK
#define S_IFLNK 0x1400
#endif
#endif
#include "Undoer.hpp"
#include "ErrorHandling.hpp"

// We'll need some future checking before relying or including  std_filesystem.hpp
#include "std_filesystem.hpp"

// Map in either STL or Boost
#include "std_atomic_mutex_chrono.hpp"

namespace boost {
    namespace afio {
        namespace detail {
#ifdef _MSC_VER
            static inline void set_threadname(const char *threadName)
            {
                const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
                typedef struct tagTHREADNAME_INFO
                {
                    DWORD dwType; // Must be 0x1000.
                    LPCSTR szName; // Pointer to name (in user addr space).
                    DWORD dwThreadID; // Thread ID (-1=caller thread).
                    DWORD dwFlags; // Reserved for future use, must be zero.
                } THREADNAME_INFO;
#pragma pack(pop)
                THREADNAME_INFO info;
                info.dwType = 0x1000;
                info.szName = threadName;
                info.dwThreadID = -1;
                info.dwFlags = 0;

                __try
                {
                    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*) &info);
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                }
            }
#elif defined(__linux__)
            static inline void set_threadname(const char *threadName)
            {
                pthread_setname_np(pthread_self(), threadName);
            }
#else
            static inline void set_threadname(const char *threadName)
            {
            }
#endif
        }
    }
}

// Need some portable way of throwing a really absolutely definitely fatal exception
// If we guaranteed had noexcept, this would be easy, but for compilers without noexcept
// we'll bounce through extern "C" as well just to be sure
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4297) // function assumed not to throw an exception but does __declspec(nothrow) or throw() was specified on the function
#endif
#ifdef BOOST_AFIO_COMPILING_FOR_GCOV
#define BOOST_AFIO_THROW_FATAL(x) std::terminate()
#else
namespace boost {
    namespace afio {
        namespace detail {
            BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC void print_fatal_exception_message_to_stderr(const char *msg);
            template<class T> inline void do_throw_fatal_exception(const T &v) BOOST_NOEXCEPT_OR_NOTHROW
            {
                print_fatal_exception_message_to_stderr(v.what());
                throw v;
            }
            extern "C" inline void boost_afio_do_throw_fatal_exception(std::function<void()> impl) BOOST_NOEXCEPT_OR_NOTHROW{ impl(); }
            template<class T> inline void throw_fatal_exception(const T &v) BOOST_NOEXCEPT_OR_NOTHROW
            {
                // In case the extern "C" fails to terminate, trap and terminate here
                try
                {
                    std::function<void()> doer=std::bind(&do_throw_fatal_exception<T>, std::ref(v));
                    boost_afio_do_throw_fatal_exception(doer);
                }
                catch(...)
                {
                    std::terminate(); // Sadly won't produce much of a useful error message
                }
            }
        }
    }
}
#ifndef BOOST_AFIO_THROW_FATAL
#define BOOST_AFIO_THROW_FATAL(x) boost::afio::detail::throw_fatal_exception(x)
#endif
#endif
#ifdef _MSC_VER
#pragma warning(pop)
#endif



#endif  /* BOOST_AFIO_UTILITY_HPP */

