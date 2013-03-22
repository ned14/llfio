/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef TRIPLEGIT_ASYNC_FILE_IO_H
#define TRIPLEGIT_ASYNC_FILE_IO_H

#include "../../NiallsCPP11Utilities/NiallsCPP11Utilities.hpp"
#include "std_filesystem.hpp"
#include <thread>
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif
#define BOOST_THREAD_VERSION 3
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION
#include "boost/asio.hpp"
#include "boost/thread/future.hpp"

#ifdef TRIPLEGIT_DLL_EXPORTS
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLEXPORTMARKUP
#else
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLIMPORTMARKUP
#endif


namespace triplegit { namespace async_io {

#ifdef __GNUC__
typedef boost::thread thread;
#else
typedef std::thread thread;
#endif

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
	shared_future(const boost::shared_future<T> &o) : boost::shared_future<T>(o) { }
	shared_future &operator=(boost::shared_future<T> &&o) { static_cast<boost::shared_future<T> &&>(*this)=std::move(o); return *this; }
	shared_future &operator=(const boost::shared_future<T> &o) { static_cast<boost::shared_future<T> &>(*this)=o; return *this; }
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

	std::vector< std::unique_ptr<thread> > workers;
    boost::asio::io_service service;
    boost::asio::io_service::work working;
public:
	//! Constructs a thread pool of \em no workers
    explicit thread_pool(size_t no) : working(service)
	{
		workers.reserve(no);
		for(size_t n=0; n<no; n++)
			workers.push_back(std::unique_ptr<thread>(new thread(worker(this))));
	}
    ~thread_pool()
	{
		service.stop();
		for(auto &i : workers)
			i->join();
	}
	//! Returns the underlying io_service
	boost::asio::io_service &io_service() { return service; }
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
template <class InputIterator> inline future<std::vector<typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type>> when_all(InputIterator first, InputIterator last)
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
//template <typename... T> inline future<std::tuple<typename std::decay<T...>::type>> when_all(T&&... futures);
//! Returns a future result from the first of the supplied futures
template <class InputIterator> inline future<std::pair<size_t, typename std::decay<decltype(((typename InputIterator::value_type *) 0)->get())>::type>> when_any(InputIterator first, InputIterator last)
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
/*template <typename... T> inline future<std::pair<size_t, typename std::decay<T>::type>> when_any(T&&... futures)
{
	std::array<T..., sizeof...(T)> f={ futures...};
	return when_any(f.begin(), f.end());
}*/

class async_file_io_dispatcher_base;

namespace detail {

	class async_io_handle : public std::enable_shared_from_this<async_io_handle>
	{
		friend class async_file_io_dispatcher_base;
		async_file_io_dispatcher_base *_parent;
		std::filesystem::path _path; // guaranteed canonical
	protected:
		async_io_handle(async_file_io_dispatcher_base *parent, const std::filesystem::path &path) : _parent(parent), _path(std::filesystem::canonical(path)) { }
	public:
		virtual ~async_io_handle() { }
		//! Returns the parent of this io handle
		async_file_io_dispatcher_base *parent() const { return _parent; }
		//! Returns the path of this io handle
		const std::filesystem::path &path() const { return _path; }
	};
	struct async_io_handle_posix;
	struct async_io_handle_windows;
}

struct async_io_op;

namespace detail { struct async_file_io_dispatcher_base_p; class async_file_io_dispatcher_compat; class async_file_io_dispatcher_windows; class async_file_io_dispatcher_linux; class async_file_io_dispatcher_qnx; }

/*! \enum open_flags
\brief Bitwise file and directory open flags
*/
enum class file_flags : size_t
{
	None=0,		//!< No flags set
	Read=1,		//!< Read access
	Write=2,	//!< Write access
	Append=4,	//!< Append only
	Truncate=8, //!< Truncate existing file to zero
	Create=16,	//!< Open and create if doesn't exist
	CreateOnlyIfNotExist=32, //!< Create and open only if doesn't exist
	AutoFlush=64,	//!< Automatically initiate an asynchronous flush just before file close, and fuse both operations so both must complete for close to complete.

	OSDirect=(1<<16),		//!< Bypass the OS file buffers (only really useful for writing large files. Note you must 4Kb align everything if this is on)
	OSSync=(1<<17)		//!< Ask the OS to not complete until the data is on the physical storage. Best used only with Direct, otherwise use AutoFlush.

};
inline file_flags operator&(file_flags a, file_flags b)
{
	return static_cast<file_flags>(static_cast<size_t>(a) & static_cast<size_t>(b));
}
inline file_flags operator|(file_flags a, file_flags b)
{
	return static_cast<file_flags>(static_cast<size_t>(a) | static_cast<size_t>(b));
}

struct async_path_op_req;

/*! \class async_file_io_dispatcher_base
\brief Abstract base class for dispatching file i/o asynchronously
*/
class async_file_io_dispatcher_base : public std::enable_shared_from_this<async_file_io_dispatcher_base>
{
	//friend TRIPLEGIT_ASYNC_FILE_IO_API std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::AutoFlush, file_flags flagsmask=file_flags::None);
	friend struct detail::async_io_handle_posix;
	friend struct detail::async_io_handle_windows;
	friend class detail::async_file_io_dispatcher_compat;
	friend class detail::async_file_io_dispatcher_windows;
	friend class detail::async_file_io_dispatcher_linux;
	friend class detail::async_file_io_dispatcher_qnx;

	detail::async_file_io_dispatcher_base_p *p;
	void int_add_io_handle(void *key, std::shared_ptr<detail::async_io_handle> h);
	void int_del_io_handle(void *key);
protected:
	async_file_io_dispatcher_base(thread_pool &threadpool, file_flags flagsforce, file_flags flagsmask);
public:
	virtual ~async_file_io_dispatcher_base();

	//! Returns the thread pool used by this dispatcher
	thread_pool &threadpool() const;
	//! Returns file flags as would be used after forcing and masking
	file_flags fileflags(file_flags flags) const;
	//! Returns the current wait queue depth of this dispatcher
	size_t wait_queue_depth() const;
	//! Returns the number of open items in this dispatcher
	size_t count() const;

	//! Asynchronously creates directories
	virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)=0;
	//! Asynchronously creates a directory
	inline async_io_op dir(const async_path_op_req &req);
	//! Asynchronously creates files
	virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)=0;
	//! Asynchronously creates a file
	inline async_io_op file(const async_path_op_req &req);
	//! Asynchronously synchronises items with physical storage once they complete
	virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)=0;
	//! Asynchronously synchronises an item with physical storage once it completes
	inline async_io_op sync(const async_io_op &req);
	//! Asynchronously closes connections to items once they complete
	virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)=0;
	//! Asynchronously closes the connection to an item once it completes
	inline async_io_op close(const async_io_op &req);
protected:
	template<class F, class... Args> std::shared_ptr<detail::async_io_handle> invoke_async_op_completions(size_t id, std::shared_ptr<detail::async_io_handle> h, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args);
	template<class F, class... Args> async_io_op chain_async_op(const async_io_op &precondition, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args);
	template<class F, class T> std::vector<async_io_op> chain_async_ops(const std::vector<T> &container, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, T));
	template<class F> std::vector<async_io_op> chain_async_ops(const std::vector<async_path_op_req> &container, std::shared_ptr<detail::async_io_handle> (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_path_op_req));
};
extern TRIPLEGIT_ASYNC_FILE_IO_API std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::AutoFlush, file_flags flagsmask=file_flags::None);

/*! \struct async_io_op
\brief A reference to an async operation
*/
struct async_io_op
{
	std::shared_ptr<async_file_io_dispatcher_base> parent; //!< The parent dispatcher
	size_t id;											//!< A unique id for this operation
	shared_future<std::shared_ptr<detail::async_io_handle>> h;	//!< A future handle to the item being operated upon

	async_io_op() : id(0) { }
	async_io_op(std::shared_ptr<async_file_io_dispatcher_base> _parent, size_t _id, shared_future<std::shared_ptr<detail::async_io_handle>> _handle) : parent(_parent), id(_id), h(_handle) { }
	async_io_op(std::shared_ptr<async_file_io_dispatcher_base> _parent, size_t _id) : parent(_parent), id(_id) { }
};
//! Convenience overload for a vector of async_io_op
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
	std::vector<shared_future<std::shared_ptr<detail::async_io_handle>>> allhs;
	allhs.reserve(std::distance(first, last));
	std::for_each(first, last, [&allhs](const async_io_op &i){ allhs.push_back(i.h); });
	return when_all(allhs.begin(), allhs.end());
}

/*! \struct async_file_io_op_req
\brief A convenience bundle of path and flags, with optional precondition
*/
struct async_path_op_req
{
	std::filesystem::path path;
	file_flags flags;
	async_io_op precondition;
	async_path_op_req(std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags) { }
	async_path_op_req(std::filesystem::path _path, file_flags _flags, async_io_op _precondition) : path(_path), flags(_flags), precondition(_precondition) { }
	async_path_op_req(async_io_op _precondition, std::filesystem::path _path) : path(_path), flags(file_flags::None), precondition(_precondition) { }
	async_path_op_req(const char *_path, file_flags _flags=file_flags::None) : path(_path), flags(_flags) { }
};

inline async_io_op async_file_io_dispatcher_base::dir(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return dir(i).front();
}
inline async_io_op async_file_io_dispatcher_base::file(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return file(i).front();
}
inline async_io_op async_file_io_dispatcher_base::sync(const async_io_op &req)
{
	std::vector<async_io_op> i;
	i.reserve(1);
	i.push_back(req);
	return sync(i).front();
}
inline async_io_op async_file_io_dispatcher_base::close(const async_io_op &req)
{
	std::vector<async_io_op> i;
	i.reserve(1);
	i.push_back(req);
	return close(i).front();
}


} } // namespace

#endif
