#include "boost/afio/afio.hpp"
#include <iostream>

int main(void)
{
    //[call_example
    
    // Create a dispatcher instance
    auto dispatcher=boost::afio::async_file_io_dispatcher();
    
    // Schedule an asynchronous call of some function with some bound set of arguments
    auto helloworld=dispatcher.call([](std::string text) {
        std::cout << text << std::endl;
        return 42;
    }, "Hello world");
    
    // Schedule as asynchronous call of some function to occur only after helloworld completes
    auto addtovalue=dispatcher.call(helloworld.second, [](std::future<int> v) {
        return v.get()+1;
    }, helloworld.first);
    
    // Create a boost::future<> representing the ops passed to when_all() and wait for it to complete
    boost::afio::when_all(addtovalue.second).wait();
    
    // Print the result returned by the future for the lambda, which will be 43
    std::cout << "addtovalue() returned " << addtovalue.first.get() << std::endl;
    //]
    return 0;
}
