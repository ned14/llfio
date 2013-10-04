/*
* File:   config.hpp
* Author: Paul Kirth
*
* Created on June 18, 2013, 7:30 PM
*/

 //  Copyright (c) 2013 Paul Kirth
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)


// most of this comes from Boost.Atomic 

#ifndef BOOST_AFIO_CONFIG_HPP
#define BOOST_AFIO_CONFIG_HPP

#ifndef BOOST_AFIO_HEADERS_ONLY
#define BOOST_AFIO_HEADERS_ONLY 1
#endif

// Get Mingw to assume we are on at least Windows 2000
#if __MSVCRT_VERSION__ < 0x601
#undef __MSVCRT_VERSION__
#define __MSVCRT_VERSION__ 0x601
#endif


#include <boost/config.hpp>
#include <boost/system/api_config.hpp>  // for BOOST_POSIX_API or BOOST_WINDOWS_API
#include <boost/detail/workaround.hpp> 

///////////////////////////////////////////////////////////////////////////////
//  Set up dll import/export options
#if (defined(BOOST_AFIO_DYN_LINK) || defined(BOOST_ALL_DYN_LINK)) && \
    !defined(BOOST_AFIO_STATIC_LINK)

#if defined(BOOST_AFIO_SOURCE)
#undef BOOST_AFIO_HEADERS_ONLY
#define BOOST_AFIO_DECL BOOST_SYMBOL_EXPORT
#define BOOST_AFIO_BUILD_DLL
#else
#define BOOST_AFIO_DECL
#endif
#else
# define BOOST_AFIO_DECL
#endif // building a shared library


///////////////////////////////////////////////////////////////////////////////
//  Auto library naming
#if !defined(BOOST_AFIO_SOURCE) && !defined(BOOST_ALL_NO_LIB) && \
    !defined(BOOST_AFIO_NO_LIB)

#define BOOST_LIB_NAME boost_afio

// tell the auto-link code to select a dll when required:
#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_AFIO_DYN_LINK)
#define BOOST_DYN_LINK
#endif

#include <boost/config/auto_link.hpp>

#endif  // auto-linking disabled

//#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
//#define BOOST_THREAD_DONT_PROVIDE_FUTURE
//#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#if BOOST_AFIO_HEADERS_ONLY == 1
#define BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC inline
#define BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC inline
#define BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC inline virtual
// GCC gets upset if inline virtual functions aren't defined
#ifdef BOOST_GCC
#define BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC { BOOST_AFIO_THROW_FATAL(std::runtime_error("Attempt to call pure virtual member function")); abort(); }
#else
#define BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC =0;
#endif
#else
#define BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC extern BOOST_AFIO_DECL
#define BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC
#define BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC virtual
#define BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC =0;
#endif

#endif  /* BOOST_AFIO_CONFIG_HPP */

