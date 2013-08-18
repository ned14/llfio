#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_works_64_sync, "Tests that the synchronous async i/o implementation works", 60)
{
#ifndef BOOST_AFIO_RUNNING_IN_CI
    auto dispatcher=boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AlwaysSync);
    std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 65536);
#endif
}