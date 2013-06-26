/* NiallsCPP11Utilities
(C) 2012 Niall Douglas http://www.nedprod.com/
File Created: Nov 2012
*/

#ifndef NIALLSCPP11UTILITIES_ERRORHANDLING_H
#define NIALLSCPP11UTILITIES_ERRORHANDLING_H

#include "NiallsCPP11Utilities.hpp"
#include "std_filesystem.hpp"
#include <string>

#if defined(_MSC_VER) && _MSC_VER<=1700 && !defined(__func__)
#define __func__ __FUNCTION__
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

namespace NiallsCPP11Utilities
{
#ifdef WIN32
	extern NIALLSCPP11UTILITIES_API void int_throwWinError(const char *file, const char *function, int lineno, unsigned code, const std::filesystem::path *filename=0);
	extern "C" unsigned __stdcall GetLastError();
#define ERRGWIN(code)				{ NiallsCPP11Utilities::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
#define ERRGWINFN(code, filename)	{ NiallsCPP11Utilities::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
#define ERRHWIN(exp)				{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWIN(GetLastError()); }
#define ERRHWINFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWINFN(GetLastError(), filename); }
#endif

	extern NIALLSCPP11UTILITIES_API void int_throwOSError(const char *file, const char *function, int lineno, int code, const std::filesystem::path *filename=0);
#define ERRGWIN(code)				{ NiallsCPP11Utilities::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code); }
#define ERRGWINFN(code, filename)	{ NiallsCPP11Utilities::int_throwWinError(EXCEPTION_FILE(0), EXCEPTION_FUNCTION(0), EXCEPTION_LINE(0), code, &(filename)); }
/*! Use this macro to wrap Win32 functions. For anything setting errno, use ERRHOS().
*/
#define ERRHWIN(exp)				{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWIN(GetLastError()); }
/*! Use this macro to wrap Win32 functions taking a filename. For anything setting errno, use ERRHOS().
*/
#define ERRHWINFN(exp, filename)	{ unsigned __errcode=(unsigned)(exp); if(!__errcode) ERRGWINFN(GetLastError(), filename); }

#define ERRGOS(code)				{ NiallsCPP11Utilities::int_throwOSError(EXCEPTION_FILE(code), EXCEPTION_FUNCTION(code), EXCEPTION_LINE(code), code); }
#define ERRGOSFN(code, filename)	{ NiallsCPP11Utilities::int_throwOSError(EXCEPTION_FILE(code), EXCEPTION_FUNCTION(code), EXCEPTION_LINE(code), code, &(filename)); }
/*! Use this macro to wrap POSIX, UNIX or CLib functions. On Win32, the includes anything in
MSVCRT which sets errno
*/
#define ERRHOS(exp)					{ int __errcode=(exp); if(__errcode<0) ERRGOS(errno); }
/*! Use this macro to wrap POSIX, UNIX or CLib functions taking a filename. On Win32, the includes anything in
MSVCRT which sets errno
*/
#define ERRHOSFN(exp, filename)		{ int __errcode=(exp); if(__errcode<0) ERRGOSFN(errno, filename); }
}

#endif
