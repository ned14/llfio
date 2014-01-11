/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

// Boost ASIO needs this
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif

// Need to include a copy of ASIO
#ifdef BOOST_ASIO_SEPARATE_COMPILATION
#include "../../../boost/asio/impl/src.hpp"
#endif

#define BOOST_AFIO_HEADERS_ONLY 0
#include "../../../boost/afio/detail/impl/afio.ipp"

