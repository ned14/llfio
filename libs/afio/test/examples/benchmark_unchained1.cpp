#include "afio_pch.hpp"

/*  My Intel Core i7 3770K running Windows 8 x64:  1555990 closures/sec
    My Intel Core i7 3770K running     Linux x64:  1432810 closures/sec
*/

static std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>> callback(size_t, std::shared_ptr<boost::afio::async_io_handle> h, boost::afio::exception_ptr *)
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
    
    std::vector<async_io_op> preconditions;
    std::vector<std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *>> callbacks(1,
        std::make_pair(async_op_flags::none, callback));
#if 0
    std::cout << "Attach profiler now and hit Return" << std::endl;
    getchar();
#endif
    begin=chrono::high_resolution_clock::now();
#pragma omp parallel for
    for(int n=0; n<5000000; n++)
        dispatcher->completion(preconditions, callbacks);
    while(dispatcher->wait_queue_depth())
        this_thread::sleep_for(chrono::milliseconds(1));
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to execute 5,000,000 closures which is " << (5000000/diff.count()) << " unchained closures/sec" << std::endl;
    std::cout << "\nPress Return to exit ..." << std::endl;
    getchar();
    return 0;
}
