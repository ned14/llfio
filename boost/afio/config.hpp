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
#define	BOOST_AFIO_CONFIG_HPP

#include <boost/config.hpp>
#include <boost/system/api_config.hpp>  // for BOOST_POSIX_API or BOOST_WINDOWS_API
#include <boost/detail/workaround.hpp> 

//Why do they use BOOST_CLANG and not BOOST_MSVC ???
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__GNUC__) || defined(BOOST_CLANG) || defined(BOOST_INTEL) || defined(__COMO__) || defined(__DMC__)
#define BOOST_AFIO_HAS_PRAGMA_ONCE
#endif


//Does this even help inside of an include guard?? 
#ifdef BOOST_AFIO_HAS_PRAGMA_ONCE
#pragma once
#endif

///////////////////////////////////////////////////////////////////////////////
//  Set up dll import/export options
#if (defined(BOOST_AFIO_DYN_LINK) || defined(BOOST_ALL_DYN_LINK)) && \
    !defined(BOOST_AFIO_STATIC_LINK)

#if defined(BOOST_AFIO_SOURCE)
#define BOOST_AFIO_DECL BOOST_SYMBOL_EXPORT
#define BOOST_AFIO_BUILD_DLL
#else
#define BOOST_AFIO_DECL BOOST_SYMBOL_IMPORT
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

#endif	/* BOOST_AFIO_CONFIG_HPP */

