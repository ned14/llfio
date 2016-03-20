//#define BOOST_RESULT_OF_USE_DECLTYPE 1
#include "afio_pch.hpp"

int main(void)
{
    //[completion_example1
    // Create a dispatcher instance
    std::shared_ptr<boost::afio::dispatcher> dispatcher=
        boost::afio::make_dispatcher().get();
    
    // Completion handlers are the lowest level completion routine available, and therefore the least
    // overhead but at the cost of considerable extra programmer effort. You almost certainly want
    // to use the call() member function instead.
    
    // First create some callable entity ...
    auto completer=[](
        /* These are always the standard parameters */
        size_t id, boost::afio::future<> precondition,
        /* From now on user defined parameters */
        std::string text)
      /* This is always the return type */
      -> std::pair<bool, std::shared_ptr<boost::afio::handle>>
    {
        /* id is the unique, non-zero integer id of this op.
           precondition is the op you supplied as precondition. As it will by definition
           have completed by now, you can fetch from its h member variable a shared pointer
           to the shared stl_future containing either the output handle or the error state.
        */
        std::cout << text << std::endl;
        
        // Return whether this completion has completed now or is it deferred,
        // along with the handle we pass onto any completions completing on this op
        // Note that op.get() by default rethrows any exception contained by the op.
        // Normally this is highly desirable.
        return std::make_pair(true, precondition.get_handle());
    };
    
    // Bind any user defined parameters to create a proper boost::afio::dispatcher::completion_t
    std::function<boost::afio::dispatcher::completion_t> boundf=
        std::bind(completer,
            /* The standard parameters */
            std::placeholders::_1, std::placeholders::_2,
            /* Any values for the user defined parameters. Remember ALWAYS to pass by value! */
            std::string("Hello world"));
    
    // Schedule an asynchronous call of the completion with some bound set of arguments
    boost::afio::future<> helloworld=
        dispatcher->completion(boost::afio::future<>() /* no precondition */,
            std::make_pair(boost::afio::async_op_flags::none, boundf));
        
    // Create a boost::stl_future<> representing the ops passed to when_all_p()
    boost::afio::stl_future<std::vector<std::shared_ptr<boost::afio::handle>>> stl_future
        =boost::afio::when_all_p(helloworld);
    // ... and wait for it to complete
    stl_future.wait();
    //]
    return 0;
}
