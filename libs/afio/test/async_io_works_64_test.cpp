#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_works_64)
{
    BOOST_TEST_MESSAGE( "Tests that the async i/o implementation works");
    
    auto dispatcher=boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::None);
    std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes:\n";
    _1000_open_write_close_deletes(dispatcher, 65536);
}