#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_works_64_autoflush, "Tests that the autoflush async i/o implementation works", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
#ifndef BOOST_AFIO_RUNNING_IN_CI
    auto dispatcher=make_dispatcher("file:///", file_flags::sync_on_close).get();
    std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with autoflush i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 65536);
#endif
}