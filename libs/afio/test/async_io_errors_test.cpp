#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_errors)
{
    BOOST_TEST_MESSAGE("Tests that the async i/o error handling works");
    using namespace boost::afio;
    using namespace std;
    using boost::afio::future;

    {
        int hasErrorDirectly, hasErrorFromBarrier;
        auto dispatcher = make_async_file_io_dispatcher();
        auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
        vector<async_path_op_req> filereqs;
        filereqs.push_back(async_path_op_req(mkdir, "testdir/a", file_flags::CreateOnlyIfNotExist));
        filereqs.push_back(async_path_op_req(mkdir, "testdir/a", file_flags::CreateOnlyIfNotExist));

        // The following is a fundamentally unstable unit test - if manyfilecreates completes before
        // sync1, sync1 will throw on the spot
        // Similarly, if either manyfilecreates or sync1 completes before the first when_all(),
        // the nothrow_t when_all() will throw on the spot :)
        do
        {
            try
            {
                auto manyfilecreates = dispatcher->file(filereqs); // One or both of these will error
                auto sync1 = dispatcher->barrier(manyfilecreates); // If barrier() doesn't throw due to errored input, barrier() will replicate errors for you
                auto future1 = when_all(std::nothrow_t(), sync1.begin(), sync1.end());
                auto future2 = when_all(sync1.begin(), sync1.end());
                // If any of the above threw due to context switches, they'll repeat
                BOOST_CHECK_NO_THROW(future1.wait()); // nothrow variant must never throw
                BOOST_AFIO_CHECK_THROWS(future2.wait()); // throw variant must always throw
                hasErrorDirectly = 0;
                for (auto &i : manyfilecreates)
                {
                    // If we ask for has_exception() before the async thread has exited its packaged_task
                    // this will fail, so no choice but to try { wait(); } catch { success }
                    try
                    {
                        i.h->get();
                    }
                    catch (...)
                    {
                        hasErrorDirectly++;
                    }
                }
                BOOST_TEST_MESSAGE("hasErrorDirectly = " << hasErrorDirectly);
                BOOST_CHECK(hasErrorDirectly == 1);
                hasErrorFromBarrier = 0;
                for (auto &i : sync1)
                {
                    try
                    {
                         i.h->get();
                    }
                    catch (...)
                    {
                        hasErrorFromBarrier++;
                    }
                }
                BOOST_TEST_MESSAGE("hasErrorFromBarrier = " << hasErrorFromBarrier);
                BOOST_CHECK(hasErrorFromBarrier == 1);
            }
            catch(...)
            {
                // Restore state and repeat until it works
                if(filesystem::exists("testdir/a"))
                    filesystem::remove("testdir/a");
                continue;
            }
        } while(false);

        do
        {
            try
            {
                auto manyfiledeletes = dispatcher->rmfile(filereqs); // One or both of these will also error. Same as above.
                auto sync2 = dispatcher->barrier(manyfiledeletes);
                auto future1 = when_all(std::nothrow_t(), sync2.begin(), sync2.end());
                auto future2 = when_all(sync2.begin(), sync2.end());
                // If any of the above threw due to context switches, they'll repeat
                BOOST_CHECK_NO_THROW(future1.wait()); // nothrow variant must never throw
                BOOST_AFIO_CHECK_THROWS(future2.wait()); // throw variant must always throw
                hasErrorDirectly = 0;
                for (auto &i : manyfiledeletes)
                {
                    try
                    {
                        i.h->get();
                    }
                    catch (...)
                    {
                        hasErrorDirectly++;
                    }
                }
                BOOST_TEST_MESSAGE("hasErrorDirectly = " << hasErrorDirectly);
                BOOST_CHECK(hasErrorDirectly == 1);
                hasErrorFromBarrier = 0;
                for (auto &i : sync2)
                {
                    try
                    {
                        i.h->get();
                    }
                    catch (...)
                    {
                        hasErrorFromBarrier++;
                    }
                }
                BOOST_TEST_MESSAGE("hasErrorFromBarrier = " << hasErrorFromBarrier);
                BOOST_CHECK(hasErrorFromBarrier == 1);
            }
            catch(...)
            {
                // Restore state and repeat until it works
                if(!filesystem::exists("testdir/a"))
                {
                    ofstream("testdir/a", ofstream::out);
                }
                continue;
            }
        } while(false);

        auto rmdir = dispatcher->rmdir(async_path_op_req("testdir"));
    }
}
