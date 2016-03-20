#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_errors, "Tests that the async i/o error handling works", 300)
{
#ifndef BOOST_AFIO_THREAD_SANITIZING
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;

    // Oh Windows, oh Windows, how strange you are ...
    for (size_t n = 0; n < 10; n++)
    {
      try
      {
        if (filesystem::exists("testdir/a"))
          filesystem::remove("testdir/a");
        if (filesystem::exists("testdir"))
          filesystem::remove("testdir");
        break;
      }
      catch (...)
      {
        this_thread::sleep_for(chrono::milliseconds(10));
      }
    }
    {
        int hasErrorDirectly, hasErrorFromBarrier;
        auto dispatcher = make_dispatcher().get();
        dispatcher->testing_flags(detail::unit_testing_flags::no_symbol_lookup);
        auto mkdir(dispatcher->dir(path_req("testdir", file_flags::create)));
        std::vector<path_req> filereqs;

        /* There was once a rare race condition in barrier() which took many, many days
         * to discover and solve, some of which involved much painful refactoring (which
         * was for the good anyway, but still painful). So, let's really hammer this API
         * such that it never, ever slightly fails to function ever again!
         */
#if defined(BOOST_AFIO_RUNNING_IN_CI) || defined(BOOST_AFIO_COMPILING_FOR_GCOV)
#if defined(WIN32)
        for(size_t n=0; n<200; n++)
#else
        for(size_t n=0; n<500; n++)
#endif
#else
        for(size_t n=0; n<50000; n++)
#endif
        {
            // The following is a fundamentally unstable unit test - if manyfilecreates completes before
            // sync1, sync1 will throw on the spot
            // Similarly, if either manyfilecreates or sync1 completes before the first when_all_p(),
            // the nothrow_t when_all_p() will throw on the spot :)
            do
            {
                filereqs.clear();
                filereqs.push_back(path_req::relative(mkdir, "a", file_flags::create_only_if_not_exist));
                filereqs.push_back(path_req::relative(mkdir, "a", file_flags::create_only_if_not_exist));
                // Windows won't let you delete a file still open
                while(dispatcher->fd_count()>1)
                    this_thread::sleep_for(chrono::milliseconds(1));
                // Unfortunately Windows does not behave here, and deleting the file if a handle is still
                // open to it appears to no op
                for(size_t n=0; n<100; n++)
                {
                    try
                    {
                      if (!filesystem::exists("testdir/a"))
                        break;
                      filesystem::remove("testdir/a");
                    } catch(...) { }
                    if(n>10) this_thread::sleep_for(chrono::milliseconds(1));
                }
                if(filesystem::exists("testdir/a"))
                {
                    std::cerr << "FATAL: Something weird is happening, I can't delete my test file!" << std::endl;
                    abort();
                }
                try
                {
                    auto manyfilecreates = dispatcher->file(filereqs); // One or both of these will error
                    auto sync1 = dispatcher->barrier(manyfilecreates); // If barrier() doesn't throw due to errored input, barrier() will replicate errors for you
                    auto future1 = when_all_p(std::nothrow_t(), sync1.begin(), sync1.end());
                    auto future1e = when_all_p(sync1.begin(), sync1.end());
                    auto future2 = when_all_p(std::nothrow_t(), sync1.begin(), sync1.end());
                    auto future2e = when_all_p(sync1.begin(), sync1.end());
                    // If any of the above threw due to context switches, they'll repeat

                    BOOST_AFIO_CHECK_NO_THROW(future1.get()); // nothrow variant must never throw
                    BOOST_AFIO_CHECK_THROWS(future1e.get()); // throw variant must always throw
                    BOOST_AFIO_CHECK_NO_THROW(future2.get()); // nothrow variant must never throw
                    BOOST_AFIO_CHECK_THROWS(future2e.get()); // throw variant must always throw
                    hasErrorDirectly = 0;
                    for (auto &i : manyfilecreates)
                    {
                        // If we ask for has_exception() before the async thread has exited its packaged_task
                        // this will fail, so no choice but to try { wait(); } catch { success }
                        try
                        {
                            i.get();
                        }
#ifdef WIN32
                        // Sometimes Windows gets a bit stuck on slower CPUs when deleting files and occasionally
                        // returns this error. It's rare, but it confounds the test results :(
                        catch(const std::exception &e)
                        {
                            if(!strncmp(e.what(), "A non close operation has been requested of a file object with a delete pending", 79))
                              throw; // reset and retry
                            hasErrorDirectly++;                              
                        }
#endif
                        catch (...)
                        {
                            hasErrorDirectly++;
                        }
                    }
                    if(hasErrorDirectly != 1)
                    {
                        std::cout << "hasErrorDirectly = " << hasErrorDirectly << std::endl;
                        BOOST_CHECK(hasErrorDirectly == 1);
                        for (auto &i : manyfilecreates)
                        {
                            try { i.get(); } catch(const std::runtime_error &e) { std::cerr << "Error was " << e.what() << std::endl; } catch(...) { std::cerr << "Error was unknown type" << std::endl; }
                        }
                    }
                    hasErrorFromBarrier = 0;
                    for (auto &i : sync1)
                    {
                        try
                        {
                             i.get();
                        }
                        catch (...)
                        {
                            hasErrorFromBarrier++;
                        }
                    }
                    if(hasErrorFromBarrier != 1)
                    {
                        std::cout << "hasErrorFromBarrier = " << hasErrorFromBarrier << std::endl;
                        BOOST_CHECK(hasErrorFromBarrier == 1);
                    }
                }
                catch(...)
                {
                    // Restore state and repeat until it works
                    continue;
                }
            } while(false);
        }
    }
    for (size_t n = 0; n < 10; n++)
    {
      try
      {
        if (filesystem::exists("testdir/a"))
          filesystem::remove("testdir/a");
        if (filesystem::exists("testdir"))
          filesystem::remove("testdir");
        break;
      }
      catch (...)
      {
        this_thread::sleep_for(chrono::milliseconds(10));
      }
    }
#endif
    // Add a single output to validate the test
    BOOST_CHECK(true);
}
