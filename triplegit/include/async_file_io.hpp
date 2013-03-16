/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef TRIPLEGIT_ASYNC_FILE_IO_H
#define TRIPLEGIT_ASYNC_FILE_IO_H

#include "../../NiallsCPP11Utilities/NiallsCPP11Utilities.hpp"
#include <thread>
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif
#define BOOST_THREAD_VERSION 3
#include "boost/asio.hpp"
#include "boost/thread/future.hpp"

#ifdef TRIPLEGIT_DLL_EXPORTS
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLEXPORTMARKUP
#else
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLIMPORTMARKUP
#endif


namespace triplegit { namespace async_io {

/*! \class future
\brief For now, this is boost's future. Will be replaced when C++'s future catches up with boost's

when_all() and when_any() definitions borrowed from http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3428.pdf
*/
template<class T> class future : public boost::future<T>
{
public:
	future() { }
	future(boost::future<T> &&o) : boost::future<T>(std::move(o)) { }
	future(future &&o) : boost::future<T>(std::move(o)) { }
	future &operator=(boost::future<T> &&o) { static_cast<boost::future<T> &&>(*this)=std::move(o); return *this; }
	future &operator=(future &&o) { static_cast<boost::future<T> &&>(*this)=std::move(o); return *this; }
};
/*! \class shared_future
\brief For now, this is boost's shared_future. Will be replaced when C++'s shared_future catches up with boost's

when_all() and when_any() definitions borrowed from http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3428.pdf
*/
template<class T> class shared_future : public boost::shared_future<T>
{
public:
	shared_future() { }
	shared_future(boost::shared_future<T> &&o) : boost::shared_future<T>(std::move(o)) { }
	shared_future(shared_future &&o) : boost::shared_future<T>(std::move(o)) { }
	shared_future &operator=(boost::shared_future<T> &&o) { static_cast<boost::shared_future<T> &&>(*this)=std::move(o); return *this; }
	shared_future &operator=(shared_future &&o) { static_cast<boost::shared_future<T> &&>(*this)=std::move(o); return *this; }
};

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
	template<class F> future<typename std::result_of<F()>::type> enqueue(F f)
	{
		typedef typename std::result_of<F()>::type R;
		// Somewhat annoyingly, io_service.post() needs its parameter to be copy constructible,
		// and packaged_task is only move constructible, so ...
		auto task=std::make_shared<boost::packaged_task<R>>(std::move(f)); // NOTE to self later: this ought to go to std::packaged_task<R()>
		service.post(std::bind([](std::shared_ptr<boost::packaged_task<R>> t) { (*t)(); }, task));
		return task->get_future();
	}
};
//! Returns the process threadpool
extern TRIPLEGIT_ASYNC_FILE_IO_API thread_pool &process_threadpool();

namespace detail {
	template<class returns_t, class future_type> inline returns_t when_all_do(std::shared_ptr<std::vector<future_type>> futures)
	{
		boost::wait_for_all(futures->begin(), futures->end());
		returns_t ret;
		ret.reserve(futures->size());
		std::for_each(futures->begin(), futures->end(), [&ret](future_type &f) { ret.push_back(std::move(f.get())); });
		return ret;
	}
	template<class returns_t, class future_type> inline returns_t when_any_do(std::shared_ptr<std::vector<future_type>> futures)
	{
		typename std::vector<future_type>::iterator it=boost::wait_for_any(futures->begin(), futures->end());
		return std::make_pair(std::distance(futures->begin(), it), std::move(it->get()));
	}
}
//! Returns a future vector of results from all the supplied futures
template <class InputIterator> future<std::vector<typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type>> when_all(InputIterator first, InputIterator last)
{
	typedef typename InputIterator::value_type future_type;
	typedef typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type value_type;
	typedef std::vector<value_type> returns_t;
	// Take a copy of the futures supplied to us (which may invalidate them)
	auto futures=std::make_shared<std::vector<future_type>>(std::make_move_iterator(first), std::make_move_iterator(last));
	// Bind to my delegate and invoke
	std::function<returns_t ()> waitforall(std::move(std::bind(&detail::when_all_do<returns_t, future_type>, futures)));
	return process_threadpool().enqueue(std::move(waitforall));
}
//! Returns a future tuple of results from all the supplied futures
//template <typename... T> future<std::tuple<typename std::decay<T...>::type>> when_all(T&&... futures);
//! Returns a future result from the first of the supplied futures
template <class InputIterator> future<std::pair<size_t, typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type>> when_any(InputIterator first, InputIterator last)
{
	typedef typename InputIterator::value_type future_type;
	typedef typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type value_type;
	typedef std::pair<size_t, typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type> returns_t;
	// Take a copy of the futures supplied to us (which may invalidate them)
	auto futures=std::make_shared<std::vector<future_type>>(std::make_move_iterator(first), std::make_move_iterator(last));
	// Bind to my delegate and invoke
	std::function<returns_t ()> waitforall(std::move(std::bind(&detail::when_any_do<returns_t, future_type>, futures)));
	return process_threadpool().enqueue(std::move(waitforall));
}
//! Returns a future result from the first of the supplied futures
/*template <typename... T> future<std::pair<size_t, typename std::decay<T>::type>> when_any(T&&... futures)
{
	std::array<T..., sizeof...(T)> f={ futures...};
	return when_any(f.begin(), f.end());
}*/

namespace detail {
	class async_io_handle
	{
		async_io_handle() { }
	public:
		virtual ~async_io_handle();
	};
}

//! A reference to an async file handle
typedef std::shared_ptr<detail::async_io_handle> async_io_handle;

namespace detail { struct async_file_io_dispatcher_base_p; class async_file_io_dispatcher_compat; class async_file_io_dispatcher_windows; class async_file_io_dispatcher_linux; class async_file_io_dispatcher_qnx; }

/*! \class async_file_io_dispatcher_base
\brief Abstract base class for dispatching file i/o asynchronously
*/
class async_file_io_dispatcher_base
{
	friend TRIPLEGIT_ASYNC_FILE_IO_API std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), bool start_io_service_in_own_thread=true);
	friend class detail::async_file_io_dispatcher_compat;
	friend class detail::async_file_io_dispatcher_windows;
	friend class detail::async_file_io_dispatcher_linux;
	friend class detail::async_file_io_dispatcher_qnx;

	detail::async_file_io_dispatcher_base_p *p;
	async_file_io_dispatcher_base(thread_pool &threadpool);
public:
	virtual ~async_file_io_dispatcher_base();

	//! Returns the current wait queue depth of this dispatcher
	size_t wait_queue_depth() const;
	//! Synchronises all outstanding operations in this dispatcher
	virtual void sync()=0;

	//! Asynchronously creates a directory
	virtual shared_future<async_io_handle> mkdir(const std::string &path)=0;
	//! Asynchronously creates a directory once \em first is completed
	virtual shared_future<async_io_handle> mkdir(shared_future<async_io_handle> first, const std::string &path)=0;
	//! Asynchronously creates a file
	virtual shared_future<async_io_handle> mkfile(const std::string &path)=0;
	//! Asynchronously creates a file once \em first is completed
	virtual shared_future<async_io_handle> mkfile(shared_future<async_io_handle> first, const std::string &path)=0;
	//! Asynchronously closes a file or directory handle once that handle becomes available
	virtual shared_future<async_io_handle> close(shared_future<async_io_handle> h)=0;
	//! Asynchronously closes a file or directory handle
	virtual shared_future<async_io_handle> close(async_io_handle h)=0;

};
extern TRIPLEGIT_ASYNC_FILE_IO_API std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool, bool start_io_service_in_own_thread);

} } // namespace

#endif
