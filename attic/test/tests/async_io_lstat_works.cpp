#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_lstat_works, "Tests that async i/o lstat() works", 60)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    // Oh Windows, oh Windows, how strange you are ...
    for (size_t n = 0; n < 10; n++)
    {
      try
      {
        if (filesystem::exists("testdir"))
          filesystem::remove("testdir");
        break;
      }
      catch (...)
      {
        this_thread::sleep_for(chrono::milliseconds(10));
      }
    }

    auto dispatcher=make_dispatcher().get();
    auto test(dispatcher->dir(path_req("testdir", file_flags::create|file_flags::write)));
    {
      auto mkdir(dispatcher->dir(path_req::relative(test, "dir", file_flags::create|file_flags::write)));
      auto mkfile(dispatcher->file(path_req::relative(mkdir, "file", file_flags::create|file_flags::write)));
      auto mklink(dispatcher->symlink(path_req::absolute(mkdir, "testdir/linktodir", file_flags::create|file_flags::write)));

      auto mkdirstat=print_stat(when_all_p(mkdir).get().front());
      auto mkfilestat=print_stat(when_all_p(mkfile).get().front());
      auto mklinkstat=print_stat(when_all_p(mklink).get().front());
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
      BOOST_CHECK(mkdirstat.st_type==filesystem::file_type::directory_file);
      BOOST_CHECK(mkfilestat.st_type==filesystem::file_type::regular_file);
      BOOST_CHECK(mklinkstat.st_type==filesystem::file_type::symlink_file);
#else
      BOOST_CHECK(mkdirstat.st_type==filesystem::file_type::directory);
      BOOST_CHECK(mkfilestat.st_type==filesystem::file_type::regular);
      BOOST_CHECK(mklinkstat.st_type==filesystem::file_type::symlink);
#endif

      // Some sanity stuff
      BOOST_CHECK(mkdirstat.st_ino!=mkfilestat.st_ino);
      BOOST_CHECK(mkfilestat.st_ino!=mklinkstat.st_ino);
      BOOST_CHECK(mkdirstat.st_ino!=mklinkstat.st_ino);
      BOOST_CHECK(mklink.get_handle()->target()==mkdir.get_handle()->path());
//      BOOST_CHECK(mkdir.get()->container()->native_handle()==test.get()->native_handle());

      // Enumerate the directory and make sure it returns the correct metadata too
      auto items = enumerate(test, 10).first;
      BOOST_CHECK(items.size() == 2);
      for (auto &item : items)
      {
        if (item.name() == "dir")
        {
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
          BOOST_CHECK(item.fetch_lstat(test.get_handle()).st_type == filesystem::file_type::directory_file);
#else
          BOOST_CHECK(item.fetch_lstat(test.get_handle()).st_type == filesystem::file_type::directory);
#endif
          BOOST_CHECK(item.fetch_lstat(test.get_handle()).st_ino == mkdirstat.st_ino);
        }
        else if (item.name() == "linktodir")
        {
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
          BOOST_CHECK(item.fetch_lstat(test.get_handle()).st_type == filesystem::file_type::symlink_file);
#else
          BOOST_CHECK(item.fetch_lstat(test.get_handle()).st_type == filesystem::file_type::symlink);
#endif
          BOOST_CHECK(item.fetch_lstat(test.get_handle()).st_ino == mklinkstat.st_ino);
        }
      }

      auto rmlink(dispatcher->close(dispatcher->rmsymlink(mklink)));
      auto rmfile(dispatcher->close(dispatcher->rmfile(dispatcher->depends(rmlink, mkfile))));
      auto rmdir(dispatcher->close(dispatcher->rmdir(dispatcher->depends(rmfile, mkdir))));
      when_all_p(rmlink, rmfile, rmdir).get();
    }
    // For the laugh, do it synchronously
    test->unlink();
}
