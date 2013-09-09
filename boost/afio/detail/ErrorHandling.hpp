/* NiallsCPP11Utilities
(C) 2012 Niall Douglas http://www.nedprod.com/
File Created: Nov 2012
*/

#ifndef NIALLSCPP11UTILITIES_ERRORHANDLING_H
#define NIALLSCPP11UTILITIES_ERRORHANDLING_H

#include "../config.hpp"
#include "std_filesystem.hpp"
#include <string>
#include <stdexcept>



#if defined(BOOST_MSVC) && BOOST_MSVC<=1800 && !defined(__func__)
#define __func__ __FUNCTION__
#endif

#ifndef BOOST_AFIO_DECL
#ifdef BOOST_AFIO_DLL_EXPORTS
#define BOOST_AFIO_DECL BOOST_SYMBOL_EXPORT
#else
/*! \brief Defines the API decoration for any exportable symbols
\ingroup macros
*/
#define BOOST_AFIO_DECL BOOST_SYMBOL_IMPORT
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
    
#ifdef BOOST_WINDOWS
                    extern BOOST_AFIO_DECL void int_throwWinError(const char *file, const char *function, int lineno, unsigned code, const std::filesystem::path *filename=0);
                    extern "C" unsigned __stdcall GetLastError();
#define BOOST_AFIO_ERRGWIN(code)				{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
#define BOOST_AFIO_ERRGWINFN(code, filename)	{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
#define BOOST_AFIO_ERRHWIN(exp)				{ unsigned __errcode=(unsigned)(exp); if(!__errcode) BOOST_AFIO_ERRGWIN(GetLastError()); }
#define BOOST_AFIO_ERRHWINFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(!__errcode) BOOST_AFIO_ERRGWINFN(GetLastError(), filename); }

                    extern BOOST_AFIO_DECL void int_throwNTError(const char *file, const char *function, int lineno, unsigned code, const std::filesystem::path *filename=0);
#define BOOST_AFIO_ERRGNT(code)				{ boost::afio::detail::int_throwNTError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
#define BOOST_AFIO_ERRGNTFN(code, filename)	{ boost::afio::detail::int_throwNTError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
#define BOOST_AFIO_ERRHNT(exp)				{ unsigned __errcode=(unsigned)(exp); if(0/*STATUS_SUCCESS*/!=__errcode) BOOST_AFIO_ERRGNT(__errcode); }
#define BOOST_AFIO_ERRHNTFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(0/*STATUS_SUCCESS*/!=__errcode) BOOST_AFIO_ERRGNTFN(__errcode, filename); }
#endif

                    extern BOOST_AFIO_DECL void int_throwOSError(const char *file, const char *function, int lineno, int code, const std::filesystem::path *filename=0);
#define BOOST_AFIO_ERRGWIN(code)				{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
#define BOOST_AFIO_ERRGWINFN(code, filename)	{ boost::afio::detail::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
            /*! Use this macro to wrap BOOST_WINDOWS functions. For anything setting errno, use ERRHOS().
            */
#define BOOST_AFIO_ERRHWIN(exp)				{ unsigned __errcode=(unsigned)(exp); if(!__errcode) BOOST_AFIO_ERRGWIN(GetLastError()); }
            /*! Use this macro to wrap BOOST_WINDOWS functions taking a filename. For anything setting errno, use ERRHOS().
            */
#define BOOST_AFIO_ERRHWINFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(!__errcode) BOOST_AFIO_ERRGWINFN(GetLastError(), filename); }

#define BOOST_AFIO_ERRGOS(code)				{ boost::afio::detail::int_throwOSError(EXCEPTION_FILE(code), EXCEPTION_FUNCTION(code), EXCEPTION_LINE(code), code); }
#define BOOST_AFIO_ERRGOSFN(code, filename)	{ boost::afio::detail::int_throwOSError(EXCEPTION_FILE(code), EXCEPTION_FUNCTION(code), EXCEPTION_LINE(code), code, &(filename)); }
            /*! Use this macro to wrap POSIX, UNIX or CLib functions. On BOOST_WINDOWS, the includes anything in
            MSVCRT which sets errno
            */
#define BOOST_AFIO_ERRHOS(exp)					{ int __errcode=(exp); if(__errcode<0) BOOST_AFIO_ERRGOS(errno); }
            /*! Use this macro to wrap POSIX, UNIX or CLib functions taking a filename. On BOOST_WINDOWS, the includes anything in
            MSVCRT which sets errno
            */
#define BOOST_AFIO_ERRHOSFN(exp, filename)		{ int __errcode=(exp); if(__errcode<0) BOOST_AFIO_ERRGOSFN(errno, filename); }
        }//namespace detail
    }//namespace afio
}// namespace boost
#endif
