#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_barrier, "Tests that the async i/o barrier works correctly under load", 180)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    typedef chrono::duration<double, ratio<1, 1>> secs_type;
    std::vector<std::pair<size_t, int>> groups;
    // Generate 500,000 sorted random numbers between 0-10000
    static const size_t numbers=
#if defined(BOOST_AFIO_RUNNING_IN_CI) || defined(BOOST_AFIO_COMPILING_FOR_GCOV)
        1600
#else
        160000
#endif
        ;
    {
        ranctx gen;
        raninit(&gen, 0x78adbcff);
        std::vector<int> manynumbers;
        manynumbers.reserve(numbers);
        for (size_t n = 0; n < numbers; n++)
            manynumbers.push_back(ranval(&gen) % 10000);
        std::sort(manynumbers.begin(), manynumbers.end());

        // Collapse into a collection of runs of the same number
        int lastnumber = -1;
        for(auto &i: manynumbers)
        {
            if (i != lastnumber)
                groups.push_back(std::make_pair(0, i));
            groups.back().first++;
            lastnumber = i;
        }
    }
    atomic<size_t> callcount[10000];
    memset(&callcount, 0, sizeof(callcount));
    std::vector<future<bool>> verifies;
    verifies.reserve(groups.size());
    auto inccount = [](atomic<size_t> *count){ /*for (volatile size_t n = 0; n < 10000; n++);*/ (*count)++; };
    auto verifybarrier = [](atomic<size_t> *count, size_t shouldbe)
    {
        if (*count != shouldbe)
        {
            BOOST_CHECK((*count == shouldbe));
            throw std::runtime_error("Count was not what it should have been!");
        }
        return true;
    };
    // For each of those runs, dispatch ops and a barrier for them
    auto dispatcher = make_dispatcher().get();
    auto begin = chrono::high_resolution_clock::now();
    size_t opscount = 0;
    future<> next;
    bool isfirst = true;
    for(auto &run: groups)
    {
        assert(run.first>0);
        std::vector<std::function<void()>> thisgroupcalls(run.first, std::bind(inccount, &callcount[run.second]));
        std::vector<future<>> thisgroupcallops;
        if (isfirst)
        {
            thisgroupcallops = dispatcher->call(thisgroupcalls);
            isfirst = false;
        }
        else
        {
            std::vector<future<>> dependency(run.first, next);
            thisgroupcallops = dispatcher->call(dependency, thisgroupcalls);
        }
        auto thisgroupbarriered = dispatcher->barrier(thisgroupcallops);
        auto verify = dispatcher->call(thisgroupbarriered.front(), std::function<bool()>(std::bind(verifybarrier, &callcount[run.second], run.first)));
        next = verify;
        verifies.push_back(std::move(verify));
        // barrier() adds an immediate op per op
        opscount += run.first*2 + 1;
    }
    auto dispatched = chrono::high_resolution_clock::now();
    std::cout << "There are now " << std::dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << std::endl;
    BOOST_AFIO_CHECK_NO_THROW(when_all_p(next).get());
    std::cout << "There are now " << std::dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << std::endl;
    // Retrieve any errors
    for(auto &i: verifies)
    {
        BOOST_AFIO_CHECK_NO_THROW(i.get());
    }
    auto end = chrono::high_resolution_clock::now();
    auto diff = chrono::duration_cast<secs_type>(end - begin);
    std::cout << "It took " << diff.count() << " secs to do " << opscount << " operations" << std::endl;
    diff = chrono::duration_cast<secs_type>(dispatched - begin);
    std::cout << "  It took " << diff.count() << " secs to dispatch all operations" << std::endl;
    diff = chrono::duration_cast<secs_type>(end - dispatched);
    std::cout << "  It took " << diff.count() << " secs to finish all operations" << std::endl << std::endl;
    diff = chrono::duration_cast<secs_type>(end - begin);
    std::cout << "That's a throughput of " << opscount / diff.count() << " ops/sec" << std::endl;
    // Add a single output to validate the test
    BOOST_CHECK(true);
}