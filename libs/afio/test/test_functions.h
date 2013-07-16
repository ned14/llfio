/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

// If defined, uses a ton more memory and is many orders of magnitude slower.
#define DEBUG_TORTURE_TEST 1

// Get Boost.ASIO on Windows IOCP working
#if defined(_DEBUG) && defined(WIN32)
#define BOOST_ASIO_BUG_WORKAROUND 1
#endif

#include <utility>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "../../../boost/afio/afio.hpp"
#include "../../../boost/afio/detail/SpookyV2.h"
#include "../../../boost/afio/detail/Aligned_Allocator.hpp"
#include "boost/lockfree/queue.hpp"
#include "../../../boost/afio/detail/Undoer.hpp"


//if we're building the tests all together don't define the test main
#ifdef BOOST_AFIO_STANDALONE_TESTS
    #define BOOST_TEST_MODULE tester   //must be defined before unit_test.hpp is included
#endif

#include <boost/test/included/unit_test.hpp>

//define a simple macro to check any exception using Boost.Test
#define BOOST_AFIO_CHECK_THROWS(expr)\
try{\
    expr;\
    BOOST_FAIL("Exception was not thrown");\
}catch(...){BOOST_CHECK(true);}


static void _1000_open_write_close_deletes(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher, size_t bytes);
static void evil_random_io(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher, size_t no, size_t bytes, size_t alignment=0);