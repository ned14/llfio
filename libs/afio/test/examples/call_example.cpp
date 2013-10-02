//#define BOOST_RESULT_OF_USE_DECLTYPE 1
#include "boost/afio/afio.hpp"
#include <iostream>

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) // Don't bother with VS2010, its result_of can't cope.
    //[call_example
    // Create a dispatcher instance
    auto dispatcher=boost::afio::make_async_file_io_dispatcher();
    
    // Schedule an asynchronous call of some function with some bound set of arguments
    auto helloworld=dispatcher->call(boost::afio::async_io_op() /* no precondition */, [](std::string text) -> int {
        std::cout << text << std::endl;
        return 42;
    }, std::string("Hello world")); // Returns a pair of <future, op ref>
    
    // Schedule as asynchronous call of some function to occur only after helloworld completes
    auto addtovalue=dispatcher->call(helloworld.second, [](boost::afio::future<int> &v) -> int {
        // v is highly likely to be ready very soon by the time we are called
        return v.get()+1;
    }, std::ref(helloworld.first));
    
    // Create a boost::future<> representing the ops passed to when_all()
    auto future=boost::afio::when_all(addtovalue.second);
    // ... and wait for it to complete
    future.wait();
    
    // Print the result returned by the future for the lambda, which will be 43
    std::cout << "addtovalue() returned " << addtovalue.first.get() << std::endl;
    //]
#endif
    return 0;
}
