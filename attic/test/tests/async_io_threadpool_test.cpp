/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#include "test_functions.hpp"

static int task()
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    thread::id this_id = this_thread::get_id();
        std::cout << "I am worker thread " << this_id << std::endl;
        return 78;
}

 
BOOST_AFIO_AUTO_TEST_CASE(async_io_thread_pool_works, "Tests that the async i/o thread pool implementation works", 10)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    
    thread::id this_id = this_thread::get_id();
    
        std::cout << "I am main thread " << this_id << std::endl;
        std_thread_pool pool(4);
        auto r=task();
        BOOST_CHECK(r==78);
        std::vector<shared_future<int>> results(8);
        
        for(auto &i: results)
        {
            i=pool.enqueue(task);
        }
        
#if BOOST_AFIO_USE_BOOST_THREAD && defined(BOOST_THREAD_PROVIDES_FUTURE_WHEN_ALL_WHEN_ANY)
        std::vector<shared_future<int>> results2;
        results2.push_back(pool.enqueue(task));
        results2.push_back(pool.enqueue(task));
        std::pair<size_t, int> allresults2=boost::when_any(results2.begin(), results2.end()).get();
        BOOST_CHECK(allresults2.first<2);
        BOOST_CHECK(allresults2.second==78);
        std::vector<int> allresults=boost::when_all_p(results.begin(), results.end()).get();
#else
        std::vector<int> allresults;
        for(auto &i : results)
          allresults.push_back(i.get());
#endif
        
        for(int i: allresults)
        {
            BOOST_CHECK(i==78);
        }
}
