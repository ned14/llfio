//#define BOOST_RESULT_OF_USE_DECLTYPE 1
#include "boost/afio/afio.hpp"
#include <iostream>
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && !(defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
#include <future>
#endif

int main(void)
{
#if !(defined(BOOST_MSVC) && BOOST_MSVC < 1700) && (defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */)
    //[completion_example2
    // Create a dispatcher instance
    std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher=
        boost::afio::make_async_file_io_dispatcher();
    
    // One thing direct programming of completion handlers can do which call() cannot is immediate
    // completions. These run immediately after the precondition finishes, but before the
    // precondition releases its result or normal enqueuing of dependencies. This makes them
    // suitable ONLY for very lightweight work, especially as they executed with the ops lock
    // held unlike normal completions i.e. no other op may be added, enqueued or completed
    // while an immediate completion is running. Classic uses for immediate completions are
    // initiating an async op. You must NEVER execute a blocking call inside an immediate
    // completion as you will hang the dispatcher.
    
    // Another thing direct programming can do is deferred completions, so completions which
    // do not complete immediately but instead at some later time. This combines naturally with
    // immediate completions: use an immediate completion to enqueue an async op and defer
    // completion until when the async op completes.
    
    // Create the completion, using the standard form
    auto completion=[](std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher,
        /* These are always the standard parameters */
        size_t id, std::shared_ptr<boost::afio::async_io_handle> h, boost::afio::exception_ptr *e)
      /* This is always the return type */
      -> std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>>
    {
        std::cout << "I am completion" << std::endl;
      
        // Create some callable entity which will do the actual completion. It can be
        // anything you like, but you need a minimum of its integer id.
        auto completer=[](std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher,
                          size_t id, std::shared_ptr<boost::afio::async_io_handle> h)
        {
            try
            {
                std::cout << "I am completer" << std::endl;

                // Do stuff, returning the handle you want passed onto dependencies.
                dispatcher->complete_async_op(id, h);
            }
            catch(...)
            {
                // In non-deferred completions AFIO traps exceptions for you. Here, you must
                // do it by hand and tell AFIO about what exception state to return.
                boost::afio::exception_ptr e(boost::afio::make_exception_ptr(
                    boost::afio::current_exception()));
                dispatcher->complete_async_op(id, h, e);
            }
        };
        // Bind the id and handle to completer, and enqueue for later asynchronous execution.
        std::async(std::launch::async, completer, dispatcher, id, h);
        
        // Indicate we are not done yet
        return std::make_pair(false, h);
    };
       
    // Bind any user defined parameters to create a proper boost::afio::async_file_io_dispatcher_base::completion_t
    std::function<boost::afio::async_file_io_dispatcher_base::completion_t> boundf=
        std::bind(completion, dispatcher,
            /* The standard parameters */
            std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    // Schedule an asynchronous call of the completion
    boost::afio::async_io_op op=
        dispatcher->completion(boost::afio::async_io_op() /* no precondition */,
            std::make_pair(boost::afio::async_op_flags::None, boundf));
        
    // Create a boost::future<> representing the ops passed to when_all()
    boost::afio::future<std::vector<std::shared_ptr<boost::afio::async_io_handle>>> future
        =boost::afio::when_all(op);
    // ... and wait for it to complete
    future.wait();
    //]
#endif
    return 0;
}
