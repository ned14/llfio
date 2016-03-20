#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(op_container_deduced_compilation, "Tests that all the op container classes compile with single arg deduced construction", 10)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    // Note that this test is mainly for testing metaprogramming compilation.
    if(false)
    {
        auto dispatcher=make_dispatcher().get();
        // Test that path_req constructs from path
        auto file1(dispatcher->file(filesystem::path("foo")));
        // Test that path_req constructs from literal
        auto file2(dispatcher->file("foo"));
        // Test that detail::io_req_impl<false> constructs from ?
        // Test that detail::io_req_impl<true> constructs from ?
        // Test that enumerate_req constructs from future<>
        auto enum1(dispatcher->enumerate(file1));       
    }
}