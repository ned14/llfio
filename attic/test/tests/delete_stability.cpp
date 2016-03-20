#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(delete_stability, "Tests that deleting files and directories always succeeds", 120)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    static BOOST_CONSTEXPR_OR_CONST size_t ITERATIONS=1000;
    static BOOST_CONSTEXPR_OR_CONST size_t ITEMS=10;
    // Oh Windows, oh Windows, how strange you are ...
    for (size_t n = 0; n < 10; n++)
    {
      try
      {
        if (filesystem::exists("testdir"))
          filesystem::remove_all("testdir");
        break;
      }
      catch (...)
      {
        this_thread::sleep_for(chrono::milliseconds(10));
      }
    }

    try
    {    
      // HoldParentOpen is actually ineffectual as renames zap the parent container, but it tests more code.
      auto dispatcher = make_dispatcher("file:///", file_flags::hold_parent_open).get();
      auto testdir = dispatcher->dir(path_req("testdir", file_flags::create));
      
      // Monte Carlo creating or deleting lots of directories containing a few files of 4Kb
      ranctx ctx;
      raninit(&ctx, 1);
      for(size_t iter=0; iter<ITERATIONS; iter++)
      {
        size_t idx=ranval(&ctx) % ITEMS;
        if(filesystem::exists("testdir/"+to_string(idx)))
        {
          auto dirh=dispatcher->dir(path_req::relative(testdir, to_string(idx), file_flags::read_write));
          std::vector<path_req> reqs={path_req::relative(dirh, "a", file_flags::read_write), path_req::relative(dirh, "b", file_flags::read_write), path_req::relative(dirh, "c", file_flags::read_write)};
          auto files=dispatcher->file(reqs);
          // Go synchronous for these
          for(auto &i : files)
            i->unlink();
          dirh->unlink();
        }
        else
        {
          static char buffer[4096];
          auto dirh=dispatcher->dir(path_req::relative(testdir, to_string(idx), file_flags::create));
          std::vector<path_req> reqs={path_req::relative(dirh, "a", file_flags::create|file_flags::read_write), path_req::relative(dirh, "b", file_flags::create|file_flags::read_write), path_req::relative(dirh, "c", file_flags::create|file_flags::read_write)};
          auto files=dispatcher->file(reqs);
          auto resized=dispatcher->truncate(files, { sizeof(buffer), sizeof(buffer), sizeof(buffer)});
          std::vector<io_req<char>> reqs2;
          for(auto &i : resized)
            reqs2.push_back(make_io_req(i, buffer, 0));
          auto written=dispatcher->write(reqs2);
          dirh.get();
          when_all_p(files.begin(), files.end()).get();
          when_all_p(resized.begin(), resized.end()).get();
          when_all_p(written.begin(), written.end()).get();
        }
      }
      BOOST_CHECK(true);
      
      filesystem::remove_all("testdir");
    }
    catch(...)
    {
      BOOST_CHECK(false);
      filesystem::remove_all("testdir");
      throw;
    }
//]
}
