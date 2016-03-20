#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_works_1_sync, "Tests that the synchronous async i/o implementation works", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
#ifndef BOOST_AFIO_RUNNING_IN_CI
    auto dispatcher=make_dispatcher("file:///", file_flags::always_sync).get();
    std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes with synchronous i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 1);
#endif
}