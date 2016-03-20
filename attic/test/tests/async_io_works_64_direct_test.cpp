#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_works_64_direct, "Tests that the direct async i/o implementation works", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    auto dispatcher = make_dispatcher("file:///", file_flags::os_direct).get();
    std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with direct i/o:\n";
    _1000_open_write_close_deletes(dispatcher, 65536);
}