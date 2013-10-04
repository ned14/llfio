#include "../test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_barrier, "Tests that the async i/o barrier works correctly under load", 120)
{
    using namespace boost::afio;
    using namespace std;
    using boost::afio::future;
    using boost::afio::ratio;
    using namespace boost::afio::detail;
    using boost::afio::off_t;
    namespace chrono = boost::afio::chrono;
    typedef chrono::duration<double, ratio<1>> secs_type;
    vector<pair<size_t, int>> groups;
    // Generate 500,000 sorted random numbers between 0-10000
    static const size_t numbers=
#if defined(BOOST_AFIO_RUNNING_IN_CI) || defined(BOOST_AFIO_COMPILING_FOR_GCOV)
        1600
#elif defined(BOOST_MSVC) && BOOST_MSVC < 1700 /* <= VS2010 */ && (defined(DEBUG) || defined(_DEBUG))
        16000
#else
        160000
#endif
        ;
    {
        ranctx gen;
        raninit(&gen, 0x78adbcff);
        vector<int> manynumbers;
        manynumbers.reserve(numbers);
        for (size_t n = 0; n < numbers; n++)
            manynumbers.push_back(ranval(&gen) % 10000);
        sort(manynumbers.begin(), manynumbers.end());

        // Collapse into a collection of runs of the same number
        int lastnumber = -1;
        BOOST_FOREACH(auto &i, manynumbers)
        {
            if (i != lastnumber)
                groups.push_back(make_pair(0, i));
            groups.back().first++;
            lastnumber = i;
        }
    }
    boost::afio::atomic<size_t> callcount[10000];
    memset(&callcount, 0, sizeof(callcount));
    vector<future<bool>> verifies;
    verifies.reserve(groups.size());
#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 // <= VS2010
    std::function<void (boost::afio::atomic<size_t> *count)> inccount = [](boost::afio::atomic<size_t> *count){ /*for (volatile size_t n = 0; n < 10000; n++);*/ (*count)++; };
    std::function<bool(boost::afio::atomic<size_t> *, size_t)> verifybarrier = [](boost::afio::atomic<size_t> *count, size_t shouldbe) ->bool
#else
    auto inccount = [](boost::afio::atomic<size_t> *count){ /*for (volatile size_t n = 0; n < 10000; n++);*/ (*count)++; };
    auto verifybarrier = [](boost::afio::atomic<size_t> *count, size_t shouldbe)
#endif
    {
        if (*count != shouldbe)
        {
            BOOST_CHECK((*count == shouldbe));
            throw std::runtime_error("Count was not what it should have been!");
        }
        return true;
    };
    // For each of those runs, dispatch ops and a barrier for them
    auto dispatcher = make_async_file_io_dispatcher();
    auto begin = chrono::high_resolution_clock::now();
    size_t opscount = 0;
    async_io_op next;
    bool isfirst = true;
    BOOST_FOREACH(auto &run, groups)
    {
        assert(run.first>0);
        vector<function<void()>> thisgroupcalls(run.first, std::bind(inccount, &callcount[run.second]));
        vector<async_io_op> thisgroupcallops;
        if (isfirst)
        {
            thisgroupcallops = dispatcher->call(thisgroupcalls).second;
            isfirst = false;
        }
        else
        {
            vector<async_io_op> dependency(run.first, next);
            thisgroupcallops = dispatcher->call(dependency, thisgroupcalls).second;
        }
        auto thisgroupbarriered = dispatcher->barrier(thisgroupcallops);
        auto verify = dispatcher->call(thisgroupbarriered.front(), std::function<bool()>(std::bind(verifybarrier, &callcount[run.second], run.first)));
        verifies.push_back(std::move(verify.first));
        next = verify.second;
        // barrier() adds an immediate op per op
        opscount += run.first*2 + 1;
    }
    auto dispatched = chrono::high_resolution_clock::now();
    cout << "There are now " << dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;
    BOOST_AFIO_CHECK_NO_THROW(when_all(next).wait());
    cout << "There are now " << dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;
    // Retrieve any errors
    BOOST_FOREACH(auto &i, verifies)
    {
        BOOST_AFIO_CHECK_NO_THROW(i.get());
    }
    auto end = chrono::high_resolution_clock::now();
    auto diff = chrono::duration_cast<secs_type>(end - begin);
    cout << "It took " << diff.count() << " secs to do " << opscount << " operations" << endl;
    diff = chrono::duration_cast<secs_type>(dispatched - begin);
    cout << "  It took " << diff.count() << " secs to dispatch all operations" << endl;
    diff = chrono::duration_cast<secs_type>(end - dispatched);
    cout << "  It took " << diff.count() << " secs to finish all operations" << endl << endl;
    diff = chrono::duration_cast<secs_type>(end - begin);
    cout << "That's a throughput of " << opscount / diff.count() << " ops/sec" << endl;
    // Add a single output to validate the test
    BOOST_CHECK(true);
}