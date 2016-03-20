// Reduce unit testing
#define BOOST_AFIO_RUNNING_IN_CI 1
// Use the Boost.Test emulation in Boost.BindLib as Boost.Test isn't multi ABI capable.
#define BOOST_AFIO_USE_BOOST_UNIT_TEST 0
// Have Boost.Spinlock also use Boost.BindLib
#define SPINLOCK_STANDALONE 1

#define STRINGIZE2(a) #a
#define STRINGIZE(a, b) STRINGIZE2(a ## b)

// Make unit test names be different
#define BOOST_CATCH_AUTO_TEST_CASE_NAME(name) STRINGIZE(1_, name)

#if 1
// A copy of AFIO + unit tests completely standalone apart from Boost.Filesystem
#define BOOST_AFIO_USE_BOOST_THREAD 0
#define BOOST_AFIO_USE_BOOST_FILESYSTEM 1
#define ASIO_STANDALONE 1
#include "test_all.cpp"
#undef BOOST_AFIO_USE_BOOST_THREAD
#undef BOOST_AFIO_USE_BOOST_FILESYSTEM
#undef ASIO_STANDALONE
#endif

// Force unit test utilities to be reincluded
#undef BOOST_AFIO_TEST_FUNCTIONS_HPP
#undef BOOST_CATCH_AUTO_TEST_CASE_NAME
#define BOOST_CATCH_AUTO_TEST_CASE_NAME(name) STRINGIZE(2_, name)

#if 1
// A copy of AFIO + unit tests using Boost.Thread, Boost.Filesystem and Boost.ASIO
#define BOOST_AFIO_USE_BOOST_THREAD 1
#define BOOST_AFIO_USE_BOOST_FILESYSTEM 1
// ASIO_STANDALONE undefined
#include "test_all.cpp"
#endif
