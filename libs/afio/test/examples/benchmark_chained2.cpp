#define BOOST_AFIO_ENABLE_BENCHMARKING_COMPLETION
#include "boost/afio/afio.hpp"
#include <iostream>

/*  My Intel Core i7 3770K running Windows 8 x64: 596318 closures/sec
    My Intel Core i7 3770K running     Linux x64: 794384 closures/sec
*/

static int callback()
{
#if 0
    Sleep(0);
#endif
    return 1;
};

int main(void)
{
    using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
    typedef chrono::duration<double, ratio<1>> secs_type;
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);
    
    atomic<size_t> threads(0);
    std::vector<std::function<int()>> callbacks(1, callback);
    begin=chrono::high_resolution_clock::now();
#pragma omp parallel
    {
        std::vector<async_io_op> preconditions(1);
        threads++;
        for(size_t n=0; n<500000; n++)
        {
            preconditions.front()=dispatcher->call(preconditions, callbacks).second.front();
        }
    }
    while(dispatcher->wait_queue_depth())
        this_thread::sleep_for(chrono::milliseconds(1));
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to execute " << (500000*threads) << " closures which is " << (500000*threads/diff.count()) << " chained closures/sec" << std::endl;
    std::cout << "\nPress Return to exit ..." << std::endl;
    getchar();
    return 0;
}
