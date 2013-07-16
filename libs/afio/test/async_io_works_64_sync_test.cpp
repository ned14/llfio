#include "test_functions.h"

BOOST_AUTO_TEST_CASE(async_io_works_64_sync)
{
    BOOST_TEST_MESSAGE("Tests that the synchronous async i/o implementation works");

    auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSSync);
    std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 65536);
}