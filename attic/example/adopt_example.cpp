#include "llfio_pch.hpp"

//[adopt_example
struct test_handle : boost::llfio::handle
{
    test_handle(boost::llfio::dispatcher *parent) :
        boost::llfio::handle(parent,
        boost::llfio::file_flags::none) {}
    virtual void close() override final
    {
        // Do nothing
    }
    virtual handle::open_states is_open() const override final
    {
        return handle::open_states::open;
    }
    virtual void *native_handle() const override final
    {
        return nullptr;
    }
    virtual boost::llfio::path path(bool refresh=false) override final
    {
        return boost::llfio::path();
    }
    virtual boost::llfio::path path() const override final
    {
        return boost::llfio::path();
    }
    virtual boost::llfio::directory_entry direntry(boost::llfio::metadata_flags
        wanted=boost::llfio::directory_entry::metadata_fastpath()) override final
    {
        return boost::llfio::directory_entry();
    }
    virtual boost::llfio::path target() override final
    {
        return boost::llfio::path();
    }
    virtual void link(const boost::llfio::path_req &req) override final
    {
    }
    virtual void unlink() override final
    {
    }
    virtual void atomic_relink(const boost::llfio::path_req &req) override final
    {
    }
};

int main(void)
{
  using namespace BOOST_AFIO_V2_NAMESPACE;
  auto dispatcher = boost::llfio::make_dispatcher().get();
  current_dispatcher_guard h(dispatcher);
  auto foreignh=std::make_shared<test_handle>(dispatcher.get());
  return 0;
}
//]
