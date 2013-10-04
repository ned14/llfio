#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_pagesize, "Tests that the page size works, and other minor functions", 3)
{
    try
    {
        auto dispatcher=boost::afio::make_async_file_io_dispatcher();
        std::cout << "\n\nSystem page size is: " << dispatcher->page_size() << " bytes" << std::endl;
        std::cout << "\n\nThread source use count is: " << dispatcher->threadsource().use_count() << std::endl;
        BOOST_AFIO_CHECK_THROWS(dispatcher->op_from_scheduled_id(78));
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        BOOST_CHECK(false);
    }
}