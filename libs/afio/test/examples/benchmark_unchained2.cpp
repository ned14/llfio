#define BOOST_AFIO_ENABLE_BENCHMARKING_COMPLETION
#include "boost/afio/afio.hpp"
#include <iostream>

/*  My Intel Core i7 3770K running Windows 8 x64:  911963 closures/sec
    My Intel Core i7 3770K running     Linux x64: 1094780 closures/sec
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
    
    std::vector<async_io_op> preconditions;
    std::vector<std::function<int()>> callbacks(1, callback);
    begin=chrono::high_resolution_clock::now();
#pragma omp parallel for
    for(int n=0; n<5000000; n++)
        dispatcher->call(preconditions, callbacks);
    while(dispatcher->wait_queue_depth())
        this_thread::sleep_for(chrono::milliseconds(1));
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to execute 5,000,000 closures which is " << (5000000/diff.count()) << " unchained closures/sec" << std::endl;
    std::cout << "\nPress Return to exit ..." << std::endl;
    getchar();
    return 0;
}
