#include "test_functions.hpp"

using namespace boost::afio;

struct test_handle : boost::afio::async_io_handle
{
    test_handle(async_file_io_dispatcher_base *parent) : boost::afio::async_io_handle(parent, std::shared_ptr<async_io_handle>(),
        "foo", file_flags::None) {}
    virtual void close()
    {
        // Do nothing
    }
    virtual void *native_handle() const
    {
        return nullptr;
    }
    virtual directory_entry direntry(metadata_flags wanted=directory_entry::metadata_fastpath()) const
    {
        return directory_entry();
    }
    virtual std::filesystem::path target() const
    {
        return std::filesystem::path();
    }
    virtual void *try_mapfile()
    {
        return nullptr;
    }
};

BOOST_AFIO_AUTO_TEST_CASE(async_io_adopt, "Tests foreign fd adoption", 5)
{
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(boost::afio::process_threadpool());
    std::cout << "\n\nTesting foreign fd adoption:\n";
    auto h=std::make_shared<test_handle>(dispatcher.get());
    auto adopted=dispatcher->adopt(h);
    BOOST_CHECK_NO_THROW(when_all(adopted).wait());
}