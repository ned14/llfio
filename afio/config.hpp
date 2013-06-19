/* 
 * File:   config.hpp
 * Author: atlas
 *
 * Created on June 18, 2013, 7:30 PM
 */

#ifndef BOOST_AFIO_CONFIG_HPP
#define	BOOST_AFIO_CONFIG_HPP


// This header implements separate compilation features as described in
// http://www.boost.org/more/separate_compilation.html

#include <boost/config.hpp>
#include <boost/system/api_config.hpp>  // for BOOST_POSIX_API or BOOST_WINDOWS_API
#include <boost/detail/workaround.hpp> 

// straight from boost.org . . . will need some work
#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_AFIO_DYN_LINK)
# if defined(BOOST_AFIO_SOURCE)
#   define BOOST_AFIO_DECL BOOST_SYMBOL_EXPORT
# else
#   define BOOST_AFIO_DECL BOOST_SYMBOL_IMPORT
# endif
#else
# define BOOST_AFIO_DECL
#endif



#endif	/* BOOST_AFIO_CONFIG_HPP */

