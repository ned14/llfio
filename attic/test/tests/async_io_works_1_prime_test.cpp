#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_works_1prime, "Tests that the async i/o implementation works (primes system)", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    auto dispatcher=make_dispatcher("file:///", file_flags::none).get();
    std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes (primes system):\n";
    _1000_open_write_close_deletes(dispatcher, 1);
}