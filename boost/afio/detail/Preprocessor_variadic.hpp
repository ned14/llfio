// Preprocessor_variadic.hpp

#ifndef BOOST_AFIO_PREPROCESSOR_VARIADIC_HPP
#define BOOST_AFIO_PREPROCESSOR_VARIADIC_HPP
#include <boost/preprocessor.hpp>

#ifndef BOOST_AFIO_MAX_PARAMETERS
#   define BOOST_AFIO_MAX_PARAMETERS 10     // this number is arbitrary
#endif

#if BOOST_AFIO_MAX_PARAMETERS < 0
#   error "Invlaid BOOST_AFIO_MAX_PARAMETERS value."
#endif

#endif