#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(race_protection_works, "Tests that the race protection works", 300)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
#ifdef __APPLE__
    // This test cannot pass on OS X currently, so exit immediately.
    return;
#endif
#if defined(__FreeBSD__) || defined(BOOST_AFIO_COMPILING_FOR_GCOV)
    // ZFS is not a fan of this test
    static BOOST_CONSTEXPR_OR_CONST size_t ITERATIONS=50;
    static BOOST_CONSTEXPR_OR_CONST size_t ITEMS=10;
#elif defined(WIN32)
    // NTFS is punished by very slow file handle opening
    static BOOST_CONSTEXPR_OR_CONST size_t ITERATIONS=50;
    static BOOST_CONSTEXPR_OR_CONST size_t ITEMS=100;
#else
    static BOOST_CONSTEXPR_OR_CONST size_t ITERATIONS=100;
    static BOOST_CONSTEXPR_OR_CONST size_t ITEMS=100;
#endif
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
    
//[race_protection_example
    try
    {
      // HoldParentOpen is actually ineffectual as renames zap the parent container, but it tests more code.
      auto dispatcher = make_dispatcher("file:///", file_flags::hold_parent_open).get();
      auto testdir = dispatcher->dir(path_req("testdir", file_flags::create));
      future<> dirh;

      try
      {
        // We can only reliably track directory renames on all platforms, so let's create 100 directories
        // which will be constantly renamed to something different by a worker thread
        std::vector<path_req> dirreqs;
        for(size_t n=0; n<ITEMS; n++)
          dirreqs.push_back(path_req::relative(testdir, to_string(n), file_flags::create|file_flags::read_write));
        // Windows needs write access to the directory to enable relinking, but opening a handle
        // with write access causes any renames into that directory to fail. So mark the first
        // directory which is always the destination for renames as non-writable
        dirreqs.front().flags=file_flags::create;
        std::cout << "Creating " << ITEMS << " directories ..." << std::endl;
        auto dirs=dispatcher->dir(dirreqs);
        when_all_p(dirs).get();
        dirh=dirs.front();
        atomic<bool> done(false);
        std::cout << "Creating worker thread to constantly rename those " << ITEMS << " directories ..." << std::endl;
        thread worker([&done, &testdir, &dirs]{
          for(size_t number=0; !done; number++)
          {
            try
            {
#ifdef WIN32
              for(size_t n=1; n<ITEMS; n++)
#else
              for(size_t n=0; n<ITEMS; n++)
#endif
              {
                path_req::relative req(testdir, to_string(number)+"_"+to_string(n));
                //std::cout << "Renaming " << dirs[n].get()->path(true) << " ..." << std::endl;
                try
                {
                  dirs[n]->atomic_relink(req);
                }
#ifdef WIN32
                catch(const system_error &/*e*/)
                {
                  // Windows does not permit renaming a directory containing open file handles
                  //std::cout << "NOTE: Failed to rename directory " << dirs[n]->path() << " due to " << e.what() << ", this is usual on Windows." << std::endl;
                }
#else
                catch(...)
                {
                  throw;
                }
#endif
              }
              std::cout << "Worker relinked all dirs to " << number << std::endl;
            }
            catch(const system_error &e) { std::cerr << "ERROR: worker thread exits via system_error code " << e.code().value() << "(" << e.what() << ")" << std::endl; BOOST_CHECK(false); }
            catch(const std::exception &e) { std::cerr << "ERROR: worker thread exits via exception (" << e.what() << ")" << std::endl; BOOST_CHECK(false); }
            catch(...) { std::cerr << "ERROR: worker thread exits via unknown exception" << std::endl; BOOST_CHECK(false); }
          }
        });
        auto unworker=detail::Undoer([&done, &worker]{done=true; worker.join();});
        
        // Create some files inside the changing directories and rename them across changing directories
        std::vector<future<>> newfiles;
        for(size_t n=0; n<ITEMS; n++)
        {
          dirreqs[n].precondition=dirs[n];
          dirreqs[n].flags=file_flags::create_only_if_not_exist|file_flags::read_write;
        }
        for(size_t i=0; i<ITERATIONS; i++)
        {
          if(!newfiles.empty())
            std::cout << "Iteration " << i << ": Renaming " << ITEMS << " files and directories inside the " << ITEMS << " constantly changing directories ..." << std::endl;
          for(size_t n=0; n<ITEMS; n++)
          {
            if(!newfiles.empty())
            {
              // Relink previous new file into first directory
              //std::cout << "Renaming " << newfiles[n].get()->path() << std::endl;
              newfiles[n]->atomic_relink(path_req::relative(dirh, to_string(n)+"_"+to_string(i)));
              // Note that on FreeBSD if this is a file its path would be now be incorrect and moreover lost due to lack of
              // path enumeration support for files. As we throw away the handle, this doesn't show up here.

              // Have the file creation depend on the previous file creation
              dirreqs[n].precondition=dispatcher->depends(newfiles[n], dirs[n]);
            }
            dirreqs[n].path=to_string(i);
          }
          // Split into two
          std::vector<path_req> front(dirreqs.begin(), dirreqs.begin()+ITEMS/2), back(dirreqs.begin()+ITEMS/2, dirreqs.end());
          std::cout << "Iteration " << i << ": Creating " << ITEMS << " files and directories inside the " << ITEMS << " constantly changing directories ..." << std::endl;
#ifdef __FreeBSD__  // FreeBSD can only track directories not files when their parent directories change
          newfiles=dispatcher->dir(front);
#else          
          newfiles=dispatcher->file(front);
#endif
          auto newfiles2=dispatcher->dir(back);
          newfiles.insert(newfiles.end(), std::make_move_iterator(newfiles2.begin()), std::make_move_iterator(newfiles2.end()));
          
          // Pace the scheduling, else we slow things down a ton. Also retrieve and throw any errors.
          when_all_p(newfiles).get();
        }
        // Wait around for all that to process
        do
        {
          this_thread::sleep_for(chrono::seconds(1));
        } while(dispatcher->wait_queue_depth());
        // Close all handles opened during this context except for dirh
      }
      catch(const system_error &e) { std::cerr << "ERROR: test exits via system_error code " << e.code().value() << "(" << e.what() << ")" << std::endl; BOOST_REQUIRE(false); }
      catch(const std::exception &e) { std::cerr << "ERROR: test exits via exception (" << e.what() << ")" << std::endl; BOOST_REQUIRE(false); }
      catch(...) { std::cerr << "ERROR: test exits via unknown exception" << std::endl; BOOST_REQUIRE(false); }

      // Check that everything is as it ought to be
      auto _contents = dispatcher->enumerate(enumerate_req(dirh, metadata_flags::All, 10*ITEMS*ITERATIONS)).get().first;
      testdir=future<>();  // Kick out AFIO now so NTFS has itself cleaned up by the end of the checks
      dirh=future<>();
      dispatcher.reset();
      std::cout << "Checking that we successfully renamed " << (ITEMS*(ITERATIONS-1)+1) << " items into the same directory ..." << std::endl;
      BOOST_CHECK(_contents.size() == (ITEMS*(ITERATIONS-1)+1));
      std::set<BOOST_AFIO_V2_NAMESPACE::filesystem::path> contents;
      for(auto &i : _contents)
        contents.insert(i.name());
      BOOST_CHECK(contents.size() == (ITEMS*(ITERATIONS-1)+1));
      for(size_t i=1; i<ITERATIONS; i++)
      {
        for(size_t n=0; n<ITEMS; n++)
        {
          if(contents.count(to_string(n)+"_"+to_string(i))==0)
            std::cerr << to_string(n)+"_"+to_string(i) << std::endl;
          BOOST_CHECK(contents.count(to_string(n)+"_"+to_string(i))>0);
        }
      }
      filesystem::remove_all("testdir");
    }
    catch(...)
    {
      filesystem::remove_all("testdir");
      throw;
    }
//]
}
