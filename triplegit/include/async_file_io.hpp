/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef TRIPLEGIT_ASYNC_FILE_IO_H
#define TRIPLEGIT_ASYNC_FILE_IO_H

#include "../../NiallsCPP11Utilities/NiallsCPP11Utilities.hpp"
#include "../../NiallsCPP11Utilities/std_filesystem.hpp"
#include <type_traits>
#include <initializer_list>
#include <thread>
#include <atomic>
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif
#define BOOST_THREAD_VERSION 3
#define BOOST_THREAD_DONT_PROVIDE_FUTURE
#include "boost/asio.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"

#ifdef TRIPLEGIT_DLL_EXPORTS
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLEXPORTMARKUP
#else
#define TRIPLEGIT_ASYNC_FILE_IO_API DLLIMPORTMARKUP
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4251) // type needs to have dll-interface to be used by clients of class
#endif

/*! \file async_file_io.hpp
\brief Provides a batch asynchronous file i/o layer based on Boost.ASIO

Some quick benchmarks:

For 1000 file open, write one byte, fsync + close, then delete:

Windows IOCP on Windows 7 x64, 2.4Ghz Sandy Bridge on 256Gb Samsung 840 Pro SSD:
\code
It took 0.010001 secs to dispatch all operations
It took 0.375038 secs to finish all operations

It took 0.154015 secs to do 6492.86 file opens per sec
   It took 0.144014 secs to synchronise file opens
It took 0.0130013 secs to do 76915.4 file writes per sec
It took 0.0860086 secs to do 11626.7 file closes per sec
It took 0.132013 secs to do 7575 file deletions per sec
\endcode

POSIX compat on Windows 7 x64, 2.4Ghz Sandy Bridge on 256Gb Samsung 840 Pro SSD:
\code
It took 0.010001 secs to dispatch all operations
It took 0.39804 secs to finish all operations

It took 0.135014 secs to do 7406.67 file opens per sec
   It took 0.125013 secs to synchronise file opens
It took 0.0490049 secs to do 20406.1 file writes per sec
It took 0.0920092 secs to do 10868.5 file closes per sec
It took 0.132013 secs to do 7575 file deletions per sec
\endcode
*/

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
template<class T> class future : public boost::unique_future<T>
{
public:
	future() { }
	future(boost::unique_future<T> &&o) : boost::unique_future<T>(std::move(o)) { }
	future(future &&o) : boost::unique_future<T>(std::move(o)) { }
	future &operator=(boost::unique_future<T> &&o) { static_cast<boost::unique_future<T> &&>(*this)=std::move(o); return *this; }
	future &operator=(future &&o) { static_cast<boost::unique_future<T> &&>(*this)=std::move(o); return *this; }
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
/*! \class promise
\brief For now, this is boost's promise. Will be replaced when C++'s promise catches up with boost's
*/
template<class T> class promise : public boost::promise<T>
{
public:
	promise() { }
	promise(boost::promise<T> &&o) : boost::promise<T>(std::move(o)) { }
	promise(promise &&o) : boost::promise<T>(std::move(o)) { }
	promise &operator=(boost::promise<T> &&o) { static_cast<boost::promise<T> &&>(*this)=std::move(o); return *this; }
	promise &operator=(promise &&o) { static_cast<boost::promise<T> &&>(*this)=std::move(o); return *this; }
};
/*! \brief For now, this is boost's exception_ptr. Will be replaced when C++'s promise catches up with boost's
*/
typedef boost::exception_ptr exception_ptr;
template<class T> inline exception_ptr make_exception_ptr(const T &e) { return boost::copy_exception(e); }

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
struct async_io_op;
struct async_path_op_req;
template<class T> struct async_data_op_req;

namespace detail {

	struct async_io_handle_posix;
	struct async_io_handle_windows;
	struct async_file_io_dispatcher_base_p;
	class async_file_io_dispatcher_compat;
	class async_file_io_dispatcher_windows;
	class async_file_io_dispatcher_linux;
	class async_file_io_dispatcher_qnx;
	class async_io_handle : public std::enable_shared_from_this<async_io_handle>
	{
		friend class async_file_io_dispatcher_base;
		friend struct async_io_handle_posix;
		friend struct async_io_handle_windows;
		friend class async_file_io_dispatcher_compat;
		friend class async_file_io_dispatcher_windows;
		friend class async_file_io_dispatcher_linux;
		friend class async_file_io_dispatcher_qnx;

		async_file_io_dispatcher_base *_parent;
		std::chrono::system_clock::time_point _opened;
		std::filesystem::path _path; // guaranteed canonical
	protected:
		std::atomic<off_t> bytesread, byteswritten, byteswrittenatlastfsync;
		async_io_handle(async_file_io_dispatcher_base *parent, const std::filesystem::path &path) : _parent(parent), _opened(std::chrono::system_clock::now()), _path(path), bytesread(0), byteswritten(0), byteswrittenatlastfsync(0) { }
	public:
		virtual ~async_io_handle() { }
		//! Returns the parent of this io handle
		async_file_io_dispatcher_base *parent() const { return _parent; }
		//! Returns when this handle was opened
		const std::chrono::system_clock::time_point &opened() const { return _opened; }
		//! Returns the path of this io handle
		const std::filesystem::path &path() const { return _path; }
		//! Returns how many bytes have been read since this handle was opened.
		off_t read_count() const { return bytesread; }
		//! Returns how many bytes have been written since this handle was opened.
		off_t write_count() const { return byteswritten; }
		//! Returns how many bytes have been written since this handle was last fsynced.
		off_t write_count_since_fsync() const { return byteswritten-byteswrittenatlastfsync; }
	};
	struct immediate_async_ops;
}


#define ASYNC_FILEIO_DECLARE_CLASS_ENUM_AS_BITFIELD(type) \
inline type operator&(type a, type b) \
{ \
	return static_cast<type>(static_cast<size_t>(a) & static_cast<size_t>(b)); \
} \
inline type operator|(type a, type b) \
{ \
	return static_cast<type>(static_cast<size_t>(a) | static_cast<size_t>(b)); \
} \
inline type operator~(type a) \
{ \
	return static_cast<type>(~static_cast<size_t>(a)); \
} \
inline bool operator!(type a) \
{ \
	return 0==static_cast<size_t>(a); \
}
/*! \enum open_flags
\brief Bitwise file and directory open flags
*/
enum class file_flags : size_t
{
	None=0,				//!< No flags set
	Read=1,				//!< Read access
	Write=2,			//!< Write access
	Append=4,			//!< Append only
	Truncate=8,			//!< Truncate existing file to zero
	Create=16,			//!< Open and create if doesn't exist
	CreateOnlyIfNotExist=32, //!< Create and open only if doesn't exist
	AutoFlush=64,		//!< Automatically initiate an asynchronous flush just before file close, and fuse both operations so both must complete for close to complete.
	WillBeSequentiallyAccessed=128, //!< Will be exclusively either read or written sequentially. If you're exclusively writing sequentially, \em strongly consider turning on OSDirect too.

	OSDirect=(1<<16),	//!< Bypass the OS file buffers (only really useful for writing large files. Note you must 4Kb align everything if this is on)
	OSSync=(1<<17)		//!< Ask the OS to not complete until the data is on the physical storage. Best used only with Direct, otherwise use AutoFlush.

};
ASYNC_FILEIO_DECLARE_CLASS_ENUM_AS_BITFIELD(file_flags)
enum class async_op_flags : size_t
{
	None=0,					//!< No flags set
	DetachedFuture=1,		//!< The specified completion routine may choose to not complete immediately
	ImmediateCompletion=2	//!< Call chained completion immediately instead of scheduling for later. Make SURE your completion can not block!
};
ASYNC_FILEIO_DECLARE_CLASS_ENUM_AS_BITFIELD(async_op_flags)


/*! \class async_file_io_dispatcher_base
\brief Abstract base class for dispatching file i/o asynchronously
*/
class TRIPLEGIT_ASYNC_FILE_IO_API async_file_io_dispatcher_base : public std::enable_shared_from_this<async_file_io_dispatcher_base>
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

	typedef std::pair<bool, std::shared_ptr<detail::async_io_handle>> completion_returntype;
	typedef completion_returntype completion_t(size_t, std::shared_ptr<detail::async_io_handle>);
	//! Invoke the specified function when each of the supplied operations complete
	std::vector<async_io_op> completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, std::function<completion_t>>> &callbacks);
	//! Invoke the specified function when the supplied operation completes
	inline async_io_op completion(const async_io_op &req, const std::pair<async_op_flags, std::function<completion_t>> &callback);
	//! Invoke the specified function when the supplied operation completes
	template<class R> inline async_io_op function(const async_io_op &req, std::function<R()> callback);
	//! Invoke the specified function when the supplied operation completes
	template<class C, class... Args> inline async_io_op call(const async_io_op &req, C callback, Args... args);

	//! Asynchronously creates directories
	virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)=0;
	//! Asynchronously creates a directory
	inline async_io_op dir(const async_path_op_req &req);
	//! Asynchronously deletes directories
	virtual std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)=0;
	//! Asynchronously deletes a directory
	inline async_io_op rmdir(const async_path_op_req &req);
	//! Asynchronously opens or creates files
	virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)=0;
	//! Asynchronously opens or creates a file
	inline async_io_op file(const async_path_op_req &req);
	//! Asynchronously deletes files
	virtual std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)=0;
	//! Asynchronously deletes files
	inline async_io_op rmfile(const async_path_op_req &req);
	//! Asynchronously synchronises items with physical storage once they complete
	virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)=0;
	//! Asynchronously synchronises an item with physical storage once it completes
	inline async_io_op sync(const async_io_op &req);
	//! Asynchronously closes connections to items once they complete
	virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)=0;
	//! Asynchronously closes the connection to an item once it completes
	inline async_io_op close(const async_io_op &req);

	//! Asynchronously reads data from items
	virtual std::vector<async_io_op> read(const std::vector<async_data_op_req<void>> &ops)=0;
	//! Asynchronously reads data from an item
	inline async_io_op read(const async_data_op_req<void> &req);
	//! Asynchronously reads data from items
	template<class T> inline std::vector<async_io_op> read(const std::vector<async_data_op_req<T>> &ops);
	//! Asynchronously writes data to items
	virtual std::vector<async_io_op> write(const std::vector<async_data_op_req<const void>> &ops)=0;
	//! Asynchronously writes data to an item
	inline async_io_op write(const async_data_op_req<const void> &req);
	//! Asynchronously writes data to items
	template<class T> inline std::vector<async_io_op> write(const std::vector<async_data_op_req<T>> &ops);
protected:
	void complete_async_op(size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr e=exception_ptr());
	completion_returntype invoke_user_completion(size_t id, std::shared_ptr<detail::async_io_handle> h, std::function<completion_t> callback);
	template<class F, class... Args> std::shared_ptr<detail::async_io_handle> invoke_async_op_completions(size_t id, std::shared_ptr<detail::async_io_handle> h, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args);
	template<class F, class... Args> async_io_op chain_async_op(detail::immediate_async_ops &immediates, int optype, const async_io_op &precondition, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args);
	template<class F, class T> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<T> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, T));
	template<class F> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_path_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_path_op_req));
	template<class F, class T> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_data_op_req<T>> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_data_op_req<T>));
};
/*! \brief Instatiates the best available async_file_io_dispatcher implementation for this system.

Note that the number of threads in the threadpool supplied is the maximum non-async op queue depth (e.g. file opens, closes etc.).
For fast SSDs, there isn't much gain after eight-sixteen threads, so the process threadpool is set to eight by default.
For slow hard drives, or worse, SANs, a queue depth of 64 or higher might deliver significant benefits.
*/
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

namespace detail
{
	struct when_all_count_completed_state
	{
		std::atomic<size_t> togo;
		std::vector<std::shared_ptr<detail::async_io_handle>> out;
		promise<std::vector<std::shared_ptr<detail::async_io_handle>>> done;
		when_all_count_completed_state(size_t outsize) : togo(outsize), out(outsize) { }
	};
	inline async_file_io_dispatcher_base::completion_returntype when_all_count_completed_nothrow(size_t, std::shared_ptr<detail::async_io_handle> h, std::shared_ptr<detail::when_all_count_completed_state> state, size_t idx)
	{
		state->out[idx]=h; // This might look thread unsafe, but each idx is unique
		if(!--state->togo)
			state->done.set_value(state->out);
		return std::make_pair(true, h);
	}
	inline async_file_io_dispatcher_base::completion_returntype when_all_count_completed(size_t, std::shared_ptr<detail::async_io_handle> h, std::shared_ptr<detail::when_all_count_completed_state> state, size_t idx)
	{
		state->out[idx]=h; // This might look thread unsafe, but each idx is unique
		if(!--state->togo)
		{
			bool done=false;
			try
			{
				for(auto &i : state->out)
					i.get();
			}
#ifdef _MSC_VER
			catch(const std::exception &)
#else
			catch(...)
#endif
			{
				exception_ptr e(async_io::make_exception_ptr(std::current_exception()));
				state->done.set_exception(e);
				done=true;
			}
			if(!done)
				state->done.set_value(state->out);
		}
		return std::make_pair(true, h);
	}
}

//! \brief Convenience overload for a vector of async_io_op. Does not retrieve exceptions.
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::nothrow_t, std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
	if(first==last)
		return future<std::vector<std::shared_ptr<detail::async_io_handle>>>();
	std::vector<async_io_op> inputs(first, last);
	std::shared_ptr<detail::when_all_count_completed_state> state(new detail::when_all_count_completed_state(inputs.size()));
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
	callbacks.reserve(inputs.size());
	size_t idx=0;
	for(auto &i : inputs)
		callbacks.push_back(std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&detail::when_all_count_completed_nothrow, std::placeholders::_1, std::placeholders::_2, state, idx++)));
	inputs.front().parent->completion(inputs, callbacks);
	return state->done.get_future();
}
//! \brief Convenience overload for a vector of async_io_op. Retrieves exceptions.
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
	if(first==last)
		return future<std::vector<std::shared_ptr<detail::async_io_handle>>>();
	std::vector<async_io_op> inputs(first, last);
	std::shared_ptr<detail::when_all_count_completed_state> state(new detail::when_all_count_completed_state(inputs.size()));
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
	callbacks.reserve(inputs.size());
	size_t idx=0;
	for(auto &i : inputs)
		callbacks.push_back(std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&detail::when_all_count_completed, std::placeholders::_1, std::placeholders::_2, state, idx++)));
	inputs.front().parent->completion(inputs, callbacks);
	return state->done.get_future();
}
//! \brief Convenience overload for a list of async_io_op.  Does not retrieve exceptions.
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::nothrow_t _, std::initializer_list<async_io_op> _ops)
{
	std::vector<async_io_op> ops;
	ops.reserve(_ops.size());
	for(auto &&i : _ops)
		ops.push_back(std::move(i));
	return when_all(_, ops.begin(), ops.end());
}
//! \brief Convenience overload for a list of async_io_op. Retrieves exceptions.
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::initializer_list<async_io_op> _ops)
{
	std::vector<async_io_op> ops;
	ops.reserve(_ops.size());
	for(auto &&i : _ops)
		ops.push_back(std::move(i));
	return when_all(ops.begin(), ops.end());
}

/*! \struct async_path_op_req
\brief A convenience bundle of path and flags, with optional precondition
*/
struct async_path_op_req
{
	std::filesystem::path path;
	file_flags flags;
	async_io_op precondition;
	//! Fails is path is not absolute
	async_path_op_req(std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags) { if(!path.is_absolute()) throw std::runtime_error("Non-absolute path"); }
	//! Fails is path is not absolute
	async_path_op_req(async_io_op _precondition, std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags), precondition(_precondition) { if(!path.is_absolute()) throw std::runtime_error("Non-absolute path"); }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(std::string _path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags) { }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(async_io_op _precondition, std::string _path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags), precondition(_precondition) { }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(const char *_path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags) { }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(async_io_op _precondition, const char *_path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags), precondition(_precondition) { }
};

/*! \struct async_data_op_req
\brief A convenience bundle of precondition, data and where. Data \b MUST stay around until the operation completes.
*/
template<> struct async_data_op_req<void> // For reading
{
	async_io_op precondition;
	std::vector<boost::asio::mutable_buffer> buffers;
	off_t where;
	async_data_op_req(async_io_op _precondition, void *_buffer, size_t _length, off_t _where) : precondition(_precondition), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::mutable_buffer(_buffer, _length)); }
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::mutable_buffer> _buffers, off_t _where) : precondition(_precondition), buffers(_buffers), where(_where) { }
};
template<> struct async_data_op_req<const void> // For writing
{
	async_io_op precondition;
	std::vector<boost::asio::const_buffer> buffers;
	off_t where;
	async_data_op_req(async_io_op _precondition, const void *_buffer, size_t _length, off_t _where) : precondition(_precondition), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::const_buffer(_buffer, _length)); }
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::const_buffer> _buffers, off_t _where) : precondition(_precondition), buffers(_buffers), where(_where) { }
};
//! \brief A specialisation for any pointer to type T
template<class T> struct async_data_op_req : public async_data_op_req<void>
{
	async_data_op_req(async_io_op _precondition, T *_buffer, size_t _length, off_t _where) : async_data_op_req<void>(_precondition, static_cast<void *>(_buffer), _length, _where) { }
};
template<class T> struct async_data_op_req<const T> : public async_data_op_req<const void>
{
	async_data_op_req(async_io_op _precondition, const T *_buffer, size_t _length, off_t _where) : async_data_op_req<const void>(_precondition, static_cast<const void *>(_buffer), _length, _where) { }
};
//! \brief A specialisation for any std::vector<T, A>
template<class T, class A> struct async_data_op_req<std::vector<T, A>> : public async_data_op_req<void>
{
	async_data_op_req(async_io_op _precondition, std::vector<T, A> &v, off_t _where) : async_data_op_req<void>(_precondition, static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
template<class T, class A> struct async_data_op_req<const std::vector<T, A>> : public async_data_op_req<const void>
{
	async_data_op_req(async_io_op _precondition, const std::vector<T, A> &v, off_t _where) : async_data_op_req<const void>(_precondition, static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A specialisation for any std::array<T, N>
template<class T, size_t N> struct async_data_op_req<std::array<T, N>> : public async_data_op_req<void>
{
	async_data_op_req(async_io_op _precondition, std::array<T, N> &v, off_t _where) : async_data_op_req<void>(_precondition, static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
template<class T, size_t N> struct async_data_op_req<const std::array<T, N>> : public async_data_op_req<const void>
{
	async_data_op_req(async_io_op _precondition, const std::array<T, N> &v, off_t _where) : async_data_op_req<const void>(_precondition, static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A specialisation for any std::basic_string<C, T, A>
template<class C, class T, class A> struct async_data_op_req<std::basic_string<C, T, A>> : public async_data_op_req<void>
{
	async_data_op_req(async_io_op _precondition, std::basic_string<C, T, A> &v, off_t _where) : async_data_op_req<void>(_precondition, static_cast<void *>(&v.front()), v.size()*sizeof(A), _where) { }
};
template<class C, class T, class A> struct async_data_op_req<const std::basic_string<C, T, A>> : public async_data_op_req<const void>
{
	async_data_op_req(async_io_op _precondition, const std::basic_string<C, T, A> &v, off_t _where) : async_data_op_req<const void>(_precondition, static_cast<const void *>(&v.front()), v.size()*sizeof(A), _where) { }
};

namespace detail {
	template<bool isconst> struct void_type_selector { typedef void type; };
	template<> struct void_type_selector<true> { typedef const void type; };
	template<class T> struct async_file_io_dispatcher_rwconverter
	{
		typedef async_data_op_req<typename void_type_selector<std::is_const<T>::value>::type> return_type;
		const std::vector<return_type> &operator()(const std::vector<async_data_op_req<T>> &ops)
		{
			typedef async_data_op_req<T> reqT;
			static_assert(std::is_convertible<reqT, return_type>::value, "async_data_op_req<T> is not convertible to async_data_op_req<[const] void>");
			static_assert(sizeof(return_type)==sizeof(reqT), "async_data_op_req<T> does not have the same size as async_data_op_req<[const] void>");
			return reinterpret_cast<const std::vector<return_type> &>(ops);
		}
	};
}

inline async_io_op async_file_io_dispatcher_base::completion(const async_io_op &req, const std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>> &callback)
{
	std::vector<async_io_op> r;
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> i;
	r.reserve(1); i.reserve(1);
	r.push_back(req);
	i.push_back(callback);
	return completion(r, i).front();
}
template<class R> inline async_io_op async_file_io_dispatcher_base::function(const async_io_op &req, std::function<R()> callback)
{
	auto invoker=[](size_t, std::shared_ptr<detail::async_io_handle> _, std::function<R()> callback){
		callback();
		return std::make_pair(true, _);
	};
	return completion(req, std::make_pair(async_op_flags::None, std::bind(invoker, std::placeholders::_1, std::placeholders::_2, callback)));
}
template<class C, class... Args> inline async_io_op async_file_io_dispatcher_base::call(const async_io_op &req, C callback, Args... args)
{
	auto invoker=[callback, args...](size_t, std::shared_ptr<detail::async_io_handle> _){
		callback(args...);
		return std::make_pair(true, _);
	};
	return completion(req, std::make_pair(async_op_flags::None, std::bind(invoker, std::placeholders::_1, std::placeholders::_2)));
}
inline async_io_op async_file_io_dispatcher_base::dir(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return dir(i).front();
}
inline async_io_op async_file_io_dispatcher_base::rmdir(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return rmdir(i).front();
}
inline async_io_op async_file_io_dispatcher_base::file(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return file(i).front();
}
inline async_io_op async_file_io_dispatcher_base::rmfile(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return rmfile(i).front();
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
inline async_io_op async_file_io_dispatcher_base::read(const async_data_op_req<void> &req)
{
	std::vector<async_data_op_req<void>> i;
	i.reserve(1);
	i.push_back(req);
	return read(i).front();
}
inline async_io_op async_file_io_dispatcher_base::write(const async_data_op_req<const void> &req)
{
	std::vector<async_data_op_req<const void>> i;
	i.reserve(1);
	i.push_back(req);
	return write(i).front();
}
template<class T> inline std::vector<async_io_op> async_file_io_dispatcher_base::read(const std::vector<async_data_op_req<T>> &ops)
{
	return read(detail::async_file_io_dispatcher_rwconverter<T>()(ops));
}
template<class T> inline std::vector<async_io_op> async_file_io_dispatcher_base::write(const std::vector<async_data_op_req<T>> &ops)
{
	return write(detail::async_file_io_dispatcher_rwconverter<T>()(ops));
}


} } // namespace

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
