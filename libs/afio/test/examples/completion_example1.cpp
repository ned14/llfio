//#define BOOST_RESULT_OF_USE_DECLTYPE 1
#include "boost/afio/afio.hpp"
#include <iostream>

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) // Don't bother with VS2010, its result_of can't cope.
    //[completion_example1
    // Create a dispatcher instance
    std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher=
        boost::afio::make_async_file_io_dispatcher();
    
    // Completion handlers are the lowest level completion routine available, and therefore the least
    // overhead but at the cost of considerable extra programmer effort. You almost certainly want
    // to use the call() member function instead.
    
    // First create some callable entity ...
    auto completer=[](
        /* These are always the standard parameters */
        size_t id, std::shared_ptr<boost::afio::async_io_handle> h, boost::afio::exception_ptr *e,
        /* From now on user defined parameters */
        std::string text)
      /* This is always the return type */
      -> std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>>
    {
        /* id is the unique, non-zero integer id of this op.
           h is a shared pointer to the file handle context returned by the precondition of this op.
           e is a pointer to an exception_ptr. It is ONLY non-null when this completion was
           called as an immediate completion. It MAY point to a valid exception_ptr if
           this immediate completion is being completed because its precondition threw
           an exception.
           
           If not an immediately completed completion, if you want to test if the precondition
           threw an exception, you'll need to pass in its op's shared state (the h member)
           so you can check the shared_future for an exception state.
        */
        std::cout << text << std::endl;
        
        // Return whether this completion has completed now or is it deferred,
        // along with the handle we pass onto any completions completing on this op
        return std::make_pair(true, h);
    };
    
    // Bind any user defined parameters to create a proper boost::afio::async_file_io_dispatcher_base::completion_t
    std::function<boost::afio::async_file_io_dispatcher_base::completion_t> boundf=
        std::bind(completer,
            /* The standard parameters */
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
            /* Any values for the user defined parameters. Remember ALWAYS to pass by value! */
            std::string("Hello world"));
    
    // Schedule an asynchronous call of the completion with some bound set of arguments
    boost::afio::async_io_op helloworld=
        dispatcher->completion(boost::afio::async_io_op() /* no precondition */,
            std::make_pair(boost::afio::async_op_flags::None, boundf));
        
    // Create a boost::future<> representing the ops passed to when_all()
    boost::afio::future<std::vector<std::shared_ptr<boost::afio::async_io_handle>>> future
        =boost::afio::when_all(helloworld);
    // ... and wait for it to complete
    future.wait();
    //]
#endif
    return 0;
}
