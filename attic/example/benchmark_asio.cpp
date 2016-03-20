#include "afio_pch.hpp"

/*  My Intel Core i7 3770K running Windows 8 x64: 2591360 closures/sec
    My Intel Core i7 3770K running     Linux x64: 1611040 closures/sec (4 threads)
*/

static boost::afio::atomic<size_t> togo(0);
static int callback()
{
#if 0
    Sleep(0);
#endif
    --togo;
    return 1;
};
int main(void)
{
    using namespace boost::afio;
    typedef chrono::duration<double, ratio<1, 1>> secs_type;
    auto threadpool=process_threadpool();
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);
    
    atomic<size_t> threads(0);
#if 0
    std::cout << "Attach profiler now and hit Return" << std::endl;
    getchar();
#endif
    begin=chrono::high_resolution_clock::now();
#pragma omp parallel
    {
        ++threads;
        for(size_t n=0; n<5000000; n++)
        {
            ++togo;
            threadpool->enqueue(callback);
        }
    }
    while(togo)
        this_thread::sleep_for(chrono::milliseconds(1));
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to execute " << (5000000*threads) << " closures which is " << (5000000*threads/diff.count()) << " closures/sec" << std::endl;
    std::cout << "\nPress Return to exit ..." << std::endl;
    getchar();
    return 0;
}
