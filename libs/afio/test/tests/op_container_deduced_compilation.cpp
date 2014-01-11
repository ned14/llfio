#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(op_container_deduced_compilation, "Tests that all the op container classes compile with single arg deduced construction", 10)
{
    using namespace boost::afio;
    // Note that this test is mainly for testing metaprogramming compilation.
    if(false)
    {
        auto dispatcher=boost::afio::make_async_file_io_dispatcher();
        // Test that async_path_op_req constructs from path
        auto file1(dispatcher->file(std::filesystem::path("foo")));
        // Test that async_path_op_req constructs from literal
        auto file2(dispatcher->file("foo"));
        // Test that detail::async_data_op_req_impl<false> constructs from ?
        // Test that detail::async_data_op_req_impl<true> constructs from ?
        // Test that async_enumerate_op_req constructs from async_io_op
        auto enum1(dispatcher->enumerate(file1));       
    }
}