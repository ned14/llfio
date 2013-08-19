#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_pagesize, "Tests that the page size works", 3)
{
    auto dispatcher=boost::afio::make_async_file_io_dispatcher();
    std::cout << "\n\nSystem page size is: " << dispatcher->page_size() << " bytes" << std::endl;
}