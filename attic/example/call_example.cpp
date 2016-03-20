//#define BOOST_RESULT_OF_USE_DECLTYPE 1
#include "afio_pch.hpp"

int main(void)
{
    //[call_example
    // Create a dispatcher instance
    auto dispatcher=boost::afio::make_dispatcher().get();
    
    // Schedule an asynchronous call of some function with some bound set of arguments
    auto helloworld=dispatcher->call(boost::afio::future<>() /* no precondition */, [](std::string text) -> int {
        std::cout << text << std::endl;
        return 42;
    }, std::string("Hello world"));

    // Schedule as asynchronous call of some function to occur only after helloworld completes
    auto addtovalue=dispatcher->call(helloworld, [&helloworld]() -> int {
        return helloworld.get()+1;
    });
    
    // Print the result returned by the future for the lambda, which will be 43
    std::cout << "addtovalue() returned " << addtovalue.get() << std::endl;
    //]
    return 0;
}
