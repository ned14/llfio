#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_works_64_directsync)
{
    BOOST_TEST_MESSAGE("Tests that the direct synchronous async i/o implementation works");
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect | boost::afio::file_flags::AlwaysSync);
    std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with direct synchronous i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 65536);
}