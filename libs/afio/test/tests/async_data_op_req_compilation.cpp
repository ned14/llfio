#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_data_op_req_compilation, "Tests that all the use cases for async_data_op_req compile", 10)
{
    using namespace boost::afio;
    try
    {
        // Note that this test is mainly for testing metaprogramming compilation.
        auto dispatcher=boost::afio::make_async_file_io_dispatcher();
        auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
        when_all(mkdir).wait();
        auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/foo", file_flags::Create|file_flags::ReadWrite)));
        when_all(mkfile).wait();
        auto last(dispatcher->truncate(mkfile, 1024));
        when_all(last).wait();
        char buffer[256];
        memset(buffer, 0, sizeof(buffer));
        size_t length=sizeof(buffer);

        // Base void * specialisation
        {
            typedef void type;
            typedef const type const_type;

            type *out=buffer;
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, length, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, length, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, length, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, length, 0));
            when_all(last).wait();
        }
        // char * specialisation
        {
            typedef char type;
            typedef const type const_type;

            type *out=buffer;
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, length, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, length, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, length, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, length, 0));
            when_all(last).wait();
        }
        // char array specialisation
        {
            typedef char type;
            typedef const type const_type;

            auto &out=buffer;
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // Arbitrary integral type array specialisation
        {
            typedef wchar_t type;
            typedef const type const_type;

            wchar_t out[sizeof(buffer)/sizeof(wchar_t)];
            memset(out, 0, sizeof(out));
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // vector specialisation
        {
            typedef std::vector<char> type;
            typedef const type const_type;

            type out(sizeof(buffer)/sizeof(type::value_type));
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // vector of boost::asio::mutable_buffer specialisation
        {
            typedef std::vector<boost::asio::mutable_buffer> type;
            typedef std::vector<boost::asio::const_buffer> const_type;

            unsigned word=0xdeadbeef;
            type out(1, boost::asio::mutable_buffer(&word, 1));
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // array specialisation
        {
            typedef std::array<char, sizeof(buffer)> type;
            typedef const type const_type;

            type out;
            out.fill(' ');
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // array of boost::asio::mutable_buffer specialisation
        {
            typedef std::array<boost::asio::mutable_buffer, 1> type;
            typedef std::array<boost::asio::const_buffer, 1> const_type;

            unsigned word=0xdeadbeef;
            type out;
            out[0]=boost::asio::mutable_buffer(&word, 1);
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // string specialisation
        {
            typedef std::string type;
            typedef const type const_type;

            type out(sizeof(buffer)/sizeof(type::value_type), ' ');
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        // boost::asio::mutable_buffer specialisation
        {
            typedef boost::asio::mutable_buffer type;
            typedef boost::asio::const_buffer const_type;

            unsigned word=0xdeadbeef;
            type out(&word, 1);
            // works
            last=dispatcher->write(async_data_op_req<const_type>(last, out, 0));
            when_all(last).wait();
            // auto-consts
            last=dispatcher->write(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // works
            last=dispatcher->read(async_data_op_req<type>(last, out, 0));
            when_all(last).wait();
            // deduces
            last=dispatcher->write(make_async_data_op_req(last, out, 0));
            when_all(last).wait();
        }
        last=dispatcher->close(last);
        when_all(last).wait();
        auto rmfile(dispatcher->rmfile(async_path_op_req(last, "testdir/foo")));
        when_all(rmfile).wait();
        auto rmdir(dispatcher->rmdir(async_path_op_req(rmfile, "testdir")));
        when_all(rmdir).wait();
        BOOST_CHECK(true);
    }
    catch(...)
    {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        BOOST_CHECK(false);
    }
}