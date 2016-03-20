#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(free_functions_work, "Tests that the free functions work as intended", 5)
{
  using namespace BOOST_AFIO_V2_NAMESPACE;
  namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
  using BOOST_AFIO_V2_NAMESPACE::file;
  using BOOST_AFIO_V2_NAMESPACE::rmdir;
  current_dispatcher_guard h(make_dispatcher().get());

  {
    auto diropened = async_dir("testdir", file_flags::create | file_flags::write);
    auto fileopened = async_file(diropened, "testfile", file_flags::create | file_flags::write);
    auto filedeleted = async_rmfile(fileopened);
    auto dirdeleted = async_rmdir(depends(filedeleted, diropened));
    dirdeleted.get();
  }
  {
    auto diropened = dir("testdir", file_flags::create | file_flags::write);
    auto fileopened = file(diropened, "testfile", file_flags::create | file_flags::write);
    rmfile(fileopened);
    rmdir(diropened);
  }
  {
    error_code ec;
    auto diropened = dir(ec, "testdir", file_flags::create | file_flags::write);
    auto fileopened = file(ec, diropened, "testfile", file_flags::create | file_flags::write);
    rmfile(ec, fileopened);
    rmdir(ec, diropened);
  }
}
