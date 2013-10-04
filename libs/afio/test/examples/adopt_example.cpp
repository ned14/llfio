#include "boost/afio/afio.hpp"

//[adopt_example
struct test_handle : boost::afio::async_io_handle
{
    test_handle(boost::afio::async_file_io_dispatcher_base *parent) :
        boost::afio::async_io_handle(parent,
        std::shared_ptr<boost::afio::async_io_handle>(),
        "foo", boost::afio::file_flags::None) {}
    virtual void close()
    {
        // Do nothing
    }
    virtual void *native_handle() const
    {
        return nullptr;
    }
    virtual boost::afio::directory_entry direntry(boost::afio::metadata_flags
        wanted=boost::afio::directory_entry::metadata_fastpath()) const
    {
        return boost::afio::directory_entry();
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

int main(void)
{
    auto dispatcher = boost::afio::make_async_file_io_dispatcher(
        boost::afio::process_threadpool());
    auto h=std::make_shared<test_handle>(dispatcher.get());
    auto adopted=dispatcher->adopt(h);
    when_all(adopted).wait();
    return 0;
}
//]
