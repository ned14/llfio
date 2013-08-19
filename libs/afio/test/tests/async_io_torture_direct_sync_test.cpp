#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_torture_directsync, "Tortures the direct synchronous async i/o implementation", 60)
{
#ifndef BOOST_AFIO_RUNNING_IN_CI
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect | boost::afio::file_flags::AlwaysSync);
    std::cout << "\n\nSustained random direct synchronous i/o to 10 files of 1Mb:\n";
    evil_random_io(dispatcher, 10, 1 * 1024 * 1024, 4096);
#endif
}