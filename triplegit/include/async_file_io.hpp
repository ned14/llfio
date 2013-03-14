/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef TRIPLEGIT_ASYNC_FILE_IO_H
#define TRIPLEGIT_ASYNC_FILE_IO_H

#include "triplegit.hpp"
#include <thread>
#include "boost/asio.hpp"
#include "boost/thread/future.hpp"

#ifdef TRIPLEGIT_DLL_EXPORTS
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLEXPORTMARKUP
#else
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLIMPORTMARKUP
#endif


namespace triplegit { namespace async_io {

/*! \class thread_pool
\brief A very simple thread pool based on Boost.ASIO and std::thread
*/
class thread_pool {
	class worker
	{
		thread_pool *pool;
	public:
		explicit worker(thread_pool *p) : pool(p) { }
		void operator()()
		{
			pool->service.run();
		}
	};
	friend class worker;

	std::vector< std::unique_ptr<std::thread> > workers;
    boost::asio::io_service service;
    boost::asio::io_service::work working;
public:
	//! Constructs a thread pool of \em no workers
    explicit thread_pool(size_t no) : working(service)
	{
		workers.reserve(no);
		for(size_t n=0; n<no; n++)
			workers.push_back(std::unique_ptr<std::thread>(new std::thread(worker(this))));
	}
    ~thread_pool()
	{
		service.stop();
		for(auto &i : workers)
			i->join();
	}
	//! Sends some callable entity to the thread pool for execution
	template<class R> future<R> enqueue(R (*f)())
	{
		// Somewhat annoyingly, io_service.post() needs its parameter to be copy constructible,
		// and packaged_task is only move constructible, so ...
		auto task=std::make_shared<boost::packaged_task<R>>(f); // NOTE to self later: this ought to go to std::packaged_task<R()>
		struct foo {
			std::shared_ptr<boost::packaged_task<R>> t; 
			foo(std::shared_ptr<boost::packaged_task<R>> f) : t(f) { }
			void operator()() { (*t)(); }
		} thunk(task);
		service.post(thunk);
		return task->get_future();
	}
};
//! Returns the process threadpool
extern TRIPLEGIT_ASYNC_FILE_IO_API thread_pool &process_threadpool();

} } // namespace

#endif
