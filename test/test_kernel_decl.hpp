/* Allows the test kernel to be compiled standalone
(C) 2016 Niall Douglas http://www.nedproductions.biz/
Created: May 2016
*/

#ifndef BOOST_AFIO_TEST_KERNEL_DECL_HPP
#define BOOST_AFIO_TEST_KERNEL_DECL_HPP

#ifdef BOOST_KERNELTEST_PLEASE_INLINE_TEST_KERNELS
// We have been included as part of an inline test suite
#define BOOST_AFIO_TEST_KERNEL_DECL inline
#else
// We are standalone
#include "boost/afio/afio.hpp"
#define BOOST_AFIO_TEST_KERNEL_DECL extern BOOST_SYMBOL_EXPORT
#endif

#endif  // namespace
