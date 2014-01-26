#include "afio_pch.hpp"

/*  My Intel Core i7 3770K running Windows 8 x64: 726124 closures/sec
    My Intel Core i7 3770K running     Linux x64: 968005 closures/sec
*/

static std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>> _callback(size_t, std::shared_ptr<boost::afio::async_io_handle> h, boost::afio::exception_ptr *)
{
#if 0
    // Simulate an i/o op with a context switch
    Sleep(0);
#endif
    return std::make_pair(true, h);
};

int main(void)
{
    using namespace boost::afio;
    auto dispatcher=make_async_file_io_dispatcher();
    typedef chrono::duration<double, ratio<1>> secs_type;
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);
    
    std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *> callback(async_op_flags::none, _callback);
    atomic<size_t> threads(0);
#if 0
    std::cout << "Attach profiler now and hit Return" << std::endl;
    getchar();
#endif
    begin=chrono::high_resolution_clock::now();
#pragma omp parallel
    {
        async_io_op last;
        threads++;
        for(size_t n=0; n<500000; n++)
        {
            last=dispatcher->completion(last, callback);
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
