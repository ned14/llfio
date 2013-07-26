// Preprocessor_variadic.hpp

#ifndef PREPROCESSOR_VARIADIC_HPP
#define PREPROCESSOR_VARIADIC_HPP
#include <boost/preprocessor.hpp>




#ifndef BOOST_AFIO_MAX_PARAMETERS
#   define BOOST_AFIO_MAX_PARAMETERS 10     // this number is arbitrary
#endif

#if BOOST_AFIO_MAX_PARAMETERS < 0
#   error "Invlaid BOOST_AFIO_MAX_PARAMETERS value."
#endif

//#include "Premade.hpp"
#define BOOST_AFIO_PREMADE_PARAMETERS 0

//#define BOOST_AFIO_VARIADIC_TEMPLATE(RETURN_TYPE, NAME, PARAMETERS, BODY )\
//{

/*
#if BOOST_AFIO_MAX_PARAMETERS > BOOST_AFIO_PREMADE_PARAMETERS
#   define BOOST_PP_ITERATION_LIMITS (BOOST_PP_INC(BOOST_AFIO_PREMADE_PARAMETERS) , BOOST_AFIO_MAX_PARAMETERS)
#   define BOOST_PP_FILENAME_1 "variadic.hpp"
#   include BOOST_PP_ITERATE()
#endif
*/

#endif