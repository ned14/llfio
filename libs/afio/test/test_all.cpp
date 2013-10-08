// Mingw can't cope with the size of binary header only generates
#ifdef __MINGW32__
#define BOOST_AFIO_HEADERS_ONLY 0
#endif

// Make sure we properly define the main for the test suite
#define BOOST_AFIO_TEST_ALL            

#include "test_functions.hpp"

BOOST_AUTO_TEST_SUITE(all)
        
#include "test_cpps.txt"        
/*     
#include "async_io_threadpool_test.cpp"
#include "async_io_works_1_prime_test.cpp"
#include "async_io_works_1_test.cpp"
#include "async_io_works_64_test.cpp"
#include "async_io_works_1_sync_test.cpp"
#include "async_io_works_64_sync_test.cpp"
#include "async_io_works_1_autoflush_test.cpp"
#include "async_io_works_64_autoflush_test.cpp"
#include "async_io_works_1_direct_test.cpp"
#include "async_io_works_64_direct_test.cpp"
#include "async_io_works_1_directsync_test.cpp"
#include "async_io_works_64_directsync_test.cpp"
#include "async_io_barrier_test.cpp"
#include "async_io_errors_test.cpp"
#include "async_io_torture_test.cpp"
#include "async_io_torture_sync_test.cpp"
#include "async_io_torture_autoflush_test.cpp"
#include "async_io_torture_direct_test.cpp"
#include "async_io_torture_direct_sync_test.cpp"
#include "async_io_sync_test.cpp" // */
BOOST_AUTO_TEST_SUITE_END()


