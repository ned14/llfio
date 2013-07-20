#include "test_functions.hpp"

BOOST_AUTO_TEST_CASE(async_io_errors)
{
    BOOST_MESSAGE("Tests that the async i/o error handling works");
    using namespace boost::afio;
    using namespace std;
    using boost::afio::future;

    {
        int hasErrorDirectly, hasErrorFromBarrier;
        auto dispatcher = async_file_io_dispatcher();
        auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
        vector<async_path_op_req> filereqs;
        filereqs.push_back(async_path_op_req(mkdir, "testdir/a", file_flags::CreateOnlyIfNotExist));
        filereqs.push_back(async_path_op_req(mkdir, "testdir/a", file_flags::CreateOnlyIfNotExist));
        {
            auto manyfilecreates = dispatcher->file(filereqs); // One of these will error
            auto sync1 = dispatcher->barrier(manyfilecreates); // If barrier() doesn't throw due to errored input, barrier() will replicate errors for you
            BOOST_CHECK_NO_THROW(when_all(std::nothrow_t(), sync1.begin(), sync1.end()).wait()); // nothrow variant must never throw
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
            BOOST_CHECK(hasErrorFromBarrier == 1);
            BOOST_AFIO_CHECK_THROWS(when_all(sync1.begin(), sync1.end()).wait()); // throw variant must always throw
        }

        auto manyfiledeletes = dispatcher->rmfile(filereqs); // One of these will also error. Same as above.
        auto sync2 = dispatcher->barrier(manyfiledeletes);
        BOOST_CHECK_NO_THROW(when_all(std::nothrow_t(), sync2.begin(), sync2.end()).wait());
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
        BOOST_CHECK(hasErrorFromBarrier == 1);

        BOOST_AFIO_CHECK_THROWS(when_all(sync2.begin(), sync2.end()).wait());
        auto rmdir = dispatcher->rmdir(async_path_op_req("testdir"));
    }
}