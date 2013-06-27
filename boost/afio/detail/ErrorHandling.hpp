/* NiallsCPP11Utilities
(C) 2012 Niall Douglas http://www.nedprod.com/
File Created: Nov 2012
*/

#ifndef NIALLSCPP11UTILITIES_ERRORHANDLING_H
#define NIALLSCPP11UTILITIES_ERRORHANDLING_H

//#include "NiallsCPP11Utilities.hpp"
#include "std_filesystem.hpp"
#include <string>
#include <boost/config.hpp>
#include <stdexcept>

#if defined(BOOST_MSVC) && BOOST_MSVC<=1700 && !defined(__func__)
#define __func__ __FUNCTION__
#endif

#ifndef BOOST_AFIO_API
#ifdef BOOST_AFIO_DLL_EXPORTS
//#define BOOST_AFIO_API DLLEXPORTMARKUP
#define BOOST_AFIO_API BOOST_SYMBOL_EXPORT
#else
//#define BOOST_AFIO_API DLLIMPORTMARKUP
#define BOOST_AFIO_API BOOST_SYMBOL_IMPORT
#endif
#endif

#ifdef EXCEPTION_DISABLESOURCEINFO
#define EXCEPTION_FILE(p) (const char *) 0
#define EXCEPTION_FUNCTION(p) (const char *) 0
#define EXCEPTION_LINE(p) 0
#else
#define EXCEPTION_FILE(p) __FILE__
#define EXCEPTION_FUNCTION(p) __func__
#define EXCEPTION_LINE(p) __LINE__
#endif

namespace boost{
    namespace afio{
        namespace detail{
    
            #ifdef WIN32
                    extern BOOST_AFIO_API void int_throwWinError(const char *file, const char *function, int lineno, unsigned code, const std::filesystem::path *filename=0);
                    extern "C" unsigned __stdcall GetLastError();
            #define ERRGWIN(code)				{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
            #define ERRGWINFN(code, filename)	{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
            #define ERRHWIN(exp)				{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWIN(GetLastError()); }
            #define ERRHWINFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWINFN(GetLastError(), filename); }
            #endif

                    extern BOOST_AFIO_API void int_throwOSError(const char *file, const char *function, int lineno, int code, const std::filesystem::path *filename=0);
            #define ERRGWIN(code)				{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
            #define ERRGWINFN(code, filename)	{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
            /*! Use this macro to wrap Win32 functions. For anything setting errno, use ERRHOS().
            */
            #define ERRHWIN(exp)				{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWIN(GetLastError()); }
            /*! Use this macro to wrap Win32 functions taking a filename. For anything setting errno, use ERRHOS().
            */
            #define ERRHWINFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWINFN(GetLastError(), filename); }

            #define ERRGOS(code)				{ boost::afio::detail::int_throwOSError(EXCEPTION_FILE(code), EXCEPTION_FUNCTION(code), EXCEPTION_LINE(code), code); }
            #define ERRGOSFN(code, filename)	{ boost::afio::detail::int_throwOSError(EXCEPTION_FILE(code), EXCEPTION_FUNCTION(code), EXCEPTION_LINE(code), code, &(filename)); }
            /*! Use this macro to wrap POSIX, UNIX or CLib functions. On Win32, the includes anything in
            MSVCRT which sets errno
            */
            #define ERRHOS(exp)					{ int __errcode=(exp); if(__errcode<0) ERRGOS(errno); }
            /*! Use this macro to wrap POSIX, UNIX or CLib functions taking a filename. On Win32, the includes anything in
            MSVCRT which sets errno
            */
            #define ERRHOSFN(exp, filename)		{ int __errcode=(exp); if(__errcode<0) ERRGOSFN(errno, filename); }
        }//namespace detail
    }//namespace afio
}// namespace boost
#endif
