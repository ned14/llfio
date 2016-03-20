#include "afio_pch.hpp"

//[adopt_example
struct test_handle : boost::afio::handle
{
    test_handle(boost::afio::dispatcher *parent) :
        boost::afio::handle(parent,
        boost::afio::file_flags::none) {}
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
    virtual boost::afio::path path(bool refresh=false) override final
    {
        return boost::afio::path();
    }
    virtual boost::afio::path path() const override final
    {
        return boost::afio::path();
    }
    virtual boost::afio::directory_entry direntry(boost::afio::metadata_flags
        wanted=boost::afio::directory_entry::metadata_fastpath()) override final
    {
        return boost::afio::directory_entry();
    }
    virtual boost::afio::path target() override final
    {
        return boost::afio::path();
    }
    virtual void link(const boost::afio::path_req &req) override final
    {
    }
    virtual void unlink() override final
    {
    }
    virtual void atomic_relink(const boost::afio::path_req &req) override final
    {
    }
};

int main(void)
{
  using namespace BOOST_AFIO_V2_NAMESPACE;
  auto dispatcher = boost::afio::make_dispatcher().get();
  current_dispatcher_guard h(dispatcher);
  auto foreignh=std::make_shared<test_handle>(dispatcher.get());
  return 0;
}
//]
