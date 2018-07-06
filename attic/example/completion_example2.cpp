//#define BOOST_RESULT_OF_USE_DECLTYPE 1
#include "llfio_pch.hpp"

int main(void)
{
    //[completion_example2
    // Create a dispatcher instance
    std::shared_ptr<boost::llfio::dispatcher> dispatcher=
        boost::llfio::make_dispatcher().get();
    
    // One thing direct programming of completion handlers can do which call() cannot is immediate
    // completions. These run immediately after the precondition finishes by the thread worker
    // which executed the precondition rather than being appended to the FIFO queue. This can
    // be useful for ensuring data is still cache-local for example.
    
    // Create the completion, using the standard form
    auto completion=[](std::shared_ptr<boost::llfio::dispatcher> dispatcher,
        /* These are always the standard parameters */
        size_t id, boost::llfio::future<> precondition)
      /* This is always the return type */
      -> std::pair<bool, std::shared_ptr<boost::llfio::handle>>
    {
        std::cout << "I am completion" << std::endl;
      
        // Create some callable entity which will do the actual completion. It can be
        // anything you like, but you need a minimum of its integer id.
        auto completer=[](std::shared_ptr<boost::llfio::dispatcher> dispatcher,
                          size_t id, boost::llfio::future<> op) -> int
        {
            try
            {
                std::cout << "I am completer" << std::endl;

                // Do stuff, returning the handle you want passed onto dependencies.
                // Note that op.get() rethrows any exception in op. Normally you want this.
                dispatcher->complete_async_op(id, op.get_handle());
            }
            catch(...)
            {
                // In non-deferred completions AFIO traps exceptions for you. Here, you must
                // do it by hand and tell AFIO about what exception state to return.
                boost::llfio::exception_ptr e(boost::llfio::current_exception());
                dispatcher->complete_async_op(id, e);
            }
            return 0;
        };
        // Bind the id and handle to completer, and enqueue for later asynchronous execution.
        std::async(std::launch::async, completer, dispatcher, id, precondition);
        
        // Indicate we are not done yet
        return std::make_pair(false, precondition.get_handle());
    };
       
    // Bind any user defined parameters to create a proper boost::llfio::dispatcher::completion_t
    std::function<boost::llfio::dispatcher::completion_t> boundf=
        std::bind(completion, dispatcher,
            /* The standard parameters */
            std::placeholders::_1, std::placeholders::_2);

    // Schedule an asynchronous call of the completion
    boost::llfio::future<> op=
        dispatcher->completion(boost::llfio::future<>() /* no precondition */,
            std::make_pair(
                /* Complete boundf immediately after its precondition (in this
                case as there is no precondition that means right now before
                completion() returns) */
                boost::llfio::async_op_flags::immediate,
                boundf));
        
    // Create a boost::stl_future<> representing the ops passed to when_all_p()
    boost::llfio::stl_future<std::vector<std::shared_ptr<boost::llfio::handle>>> stl_future
        =boost::llfio::when_all_p(op);
    // ... and wait for it to complete
    stl_future.wait();
    //]
    return 0;
}
