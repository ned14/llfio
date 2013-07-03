#include "boost/afio/afio.hpp"
#include <iostream>

using namespace std;
using namespace boost::afio;

int main(void)
{
    // Create a dispatcher instance
    auto dispatcher=async_file_io_dispatcher();
    // Schedule an asynchronous call of some function with some bound set of arguments
    auto helloworld=dispatcher.call([](string text){ cout << text << endl; return 42; }, "Hello world");
    // Create a boost::future<> representing the ops passed to when_all() and wait for it to complete.
    when_all(helloworld.second).wait();
    cout << "helloworld() returned " << helloworld.first.get() << endl;
    return 0;
}
