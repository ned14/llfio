#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_torture_direct)
{
    BOOST_TEST_MESSAGE("Tortures the direct async i/o implementation");
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect);
    std::cout << "\n\nSustained random direct i/o to 10 files of 10Mb:\n";
    evil_random_io(dispatcher, 10, 10 * 1024 * 1024, 4096);
}