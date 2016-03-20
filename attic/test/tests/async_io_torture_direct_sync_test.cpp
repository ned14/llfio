#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_torture_directsync, "Tortures the direct synchronous async i/o implementation", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
#ifndef BOOST_AFIO_RUNNING_IN_CI
    auto dispatcher = make_dispatcher("file:///", file_flags::os_direct | file_flags::always_sync).get();
    std::cout << "\n\nSustained random direct synchronous i/o to 10 files of 1Mb:\n";
    evil_random_io(dispatcher, 10, 1 * 1024 * 1024, 4096);
#endif
}