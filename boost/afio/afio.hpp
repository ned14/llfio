/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef BOOST_AFIO_HPP
#define BOOST_AFIO_HPP

// Fix up mingw weirdness
#if !defined(WIN32) && defined(_WIN32)
#define WIN32 1
#endif
// Boost ASIO needs this
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif

#include <type_traits>
#include <initializer_list>
#include <thread>
#include <mutex>
#include <atomic>
#include <exception>
//#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
//#define BOOST_THREAD_DONT_PROVIDE_FUTURE
//#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#include "boost/asio.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"
#include "config.hpp"
#include "detail/Utility.hpp"

// Map in C++11 stuff if available
#if defined(__GLIBCXX__) && __GLIBCXX__<=20120920
#include "boost/exception_ptr.hpp"
#include "boost/thread/recursive_mutex.hpp"
namespace boost { namespace afio {
typedef boost::thread thread;
inline boost::thread::id get_this_thread_id() { return boost::this_thread::get_id(); }
inline boost::exception_ptr current_exception() { return boost::current_exception(); }
typedef boost::recursive_mutex recursive_mutex;
} }
#else
namespace boost { namespace afio {
typedef std::thread thread;
inline std::thread::id get_this_thread_id() { return std::this_thread::get_id(); }
inline std::exception_ptr current_exception() { return std::current_exception(); }
typedef std::recursive_mutex recursive_mutex;
} }
#endif

#if BOOST_VERSION<105400
#error Boosts before v1.54 have a memory corruption bug in boost::packaged_task<> when built under C++11 which makes this library useless. Get a newer Boost!
#endif


#ifndef BOOST_AFIO_DECL
#ifdef BOOST_AFIO_DLL_EXPORTS
#define BOOST_AFIO_DECL BOOST_SYMBOL_EXPORT
#else
/*! \brief Defines the API decoration for any exportable symbols
\ingroup macros
*/
#define BOOST_AFIO_DECL BOOST_SYMBOL_IMPORT
#endif
#endif


/*! \brief Validate inputs at the point of instantiation.

Turns on the checking of inputs for validity and throwing of exception conditions
at the point of instantiation rather than waiting until those inputs are sent
for dispatch. This, being very useful for debugging, defaults to 1 except when
`NDEBUG` is defined i.e. final release builds.
\ingroup macros
*/
#ifndef BOOST_AFIO_VALIDATE_INPUTS
#ifndef NDEBUG
#define BOOST_AFIO_VALIDATE_INPUTS 1
#else
#define BOOST_AFIO_VALIDATE_INPUTS 0
#endif
#endif

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4251) // type needs to have dll-interface to be used by clients of class
#endif

/*! \file afio.hpp
\brief Provides a batch asynchronous file i/o implementation based on Boost.ASIO

*/
// namespaces dont show up in documentation unless I also document the parent namespace
// this is ok for now, but will need to be fixed when we improve the docs.

//! \brief The namespace used by the Boost Libraries
namespace boost {
//! \brief The namespace containing the Boost.ASIO asynchronous file i/o implementation.
namespace afio {

// This isn't consistent on MSVC so hard code it
typedef unsigned long long off_t;

#define BOOST_AFIO_FORWARD_STL_IMPL(M, B) \
template<class T> class M : public B<T> \
{ \
public: \
	M() { } \
	M(const B<T> &o) : B<T>(o) { } \
	M(B<T> &&o) : B<T>(std::move(o)) { } \
	M(const M &o) : B<T>(o) { } \
	M(M &&o) : B<T>(std::move(o)) { } \
	M &operator=(const B<T> &o) { static_cast<B<T> &&>(*this)=o; return *this; } \
	M &operator=(B<T> &&o) { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
	M &operator=(const M &o) { static_cast<B<T> &&>(*this)=o; return *this; } \
	M &operator=(M &&o) { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
};
#define BOOST_AFIO_FORWARD_STL_IMPL_NC(M, B) \
template<class T> class M : public B<T> \
{ \
public: \
	M() { } \
	M(B<T> &&o) : B<T>(std::move(o)) { } \
	M(M &&o) : B<T>(std::move(o)) { } \
	M &operator=(B<T> &&o) { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
	M &operator=(M &&o) { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
};
/*! \class future
\brief For now, this is boost's future. Will be replaced when C++'s future catches up with boost's

when_all() and when_any() definitions borrowed from http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3428.pdf
*/
#if BOOST_THREAD_VERSION >=4
BOOST_AFIO_FORWARD_STL_IMPL_NC(future, boost::future)
#else
BOOST_AFIO_FORWARD_STL_IMPL_NC(future, boost::unique_future)
#endif
/*! \class shared_future
\brief For now, this is boost's shared_future. Will be replaced when C++'s shared_future catches up with boost's

when_all() and when_any() definitions borrowed from http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3428.pdf
*/
BOOST_AFIO_FORWARD_STL_IMPL(shared_future, boost::shared_future)
/*! \class promise
\brief For now, this is boost's promise. Will be replaced when C++'s promise catches up with boost's
*/
BOOST_AFIO_FORWARD_STL_IMPL(promise, boost::promise)
/*! \brief For now, this is boost's exception_ptr. Will be replaced when C++'s exception_ptr catches up with boost's
*/
typedef boost::exception_ptr exception_ptr;
template<class T> inline exception_ptr make_exception_ptr(const T &e) { return boost::copy_exception(e); }
using boost::rethrow_exception;
/*! \brief For now, this is an emulation of std::packaged_task based on boost's packaged_task.
We have to drop the Args... support because it segfaults MSVC Nov 2012 CTP.
*/
template<class> class packaged_task;
template<class R> class packaged_task<R()>
#if BOOST_THREAD_VERSION >=4
: public boost::packaged_task<R()>
{
	typedef boost::packaged_task<R()> Base;
#else
: public boost::packaged_task<R>
	{
		typedef boost::packaged_task<R> Base;
#endif
public:
	packaged_task() { }
	packaged_task(Base &&o) : Base(std::move(o)) { }
	packaged_task(packaged_task &&o) : Base(static_cast<Base &&>(o)) { }
	template<class T> packaged_task(T &&o) : Base(std::forward<T>(o)) { }
	packaged_task &operator=(Base &&o) { static_cast<Base &&>(*this)=std::move(o); return *this; }
	packaged_task &operator=(packaged_task &&o) { static_cast<Base &&>(*this)=std::move(o); return *this; }
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
	/*! Constructs a thread pool of \em no workers
    \param no The number of worker threads to create
    */
	explicit thread_pool(size_t no) : working(service)
	{
		workers.reserve(no);
		for(size_t n=0; n<no; n++)
			workers.push_back(std::unique_ptr<thread>(new thread(worker(this))));
	}
    //! Destroys the thread pool, waiting for worker threads to exit beforehand.
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
		auto task=std::make_shared<packaged_task<R()>>(std::move(f));
		service.post(std::bind([](std::shared_ptr<packaged_task<R()>> t) { (*t)(); }, task));
		return task->get_future();
	}
};
/*! \brief Returns the process threadpool

On first use, this instantiates a default thread_pool running eight threads.
\ingroup process_threadpool
*/
extern BOOST_AFIO_DECL thread_pool &process_threadpool();

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
/*! \brief Makes a future vector of results from all the supplied futures

This is an implementation of WG21 N3634's when_all() which uses process_threadpool()
to allow you to create a future which only becomes available when all the supplied
futures become available.

\return A future vector of the results of the input futures
\tparam "class InputIterator" A type modelling an iterator
\param first An iterator pointing to the first item to wait upon
\param last An iterator pointing to after the last future to wait upon
\ingroup when_all_futures
\qbk{distinguish, from ___WG21_N3634__ for range of futures}
\complexity{O(N)}
\exceptionmodel{The same as a future}
*/
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
/*! \brief Makes a future result from the first of the supplied futures to become available

This is an implementation of WG21 N3634's when_any() which uses process_threadpool()
to allow you to create a future which only becomes available when the first of the supplied
futures become available.

\return A future pair of the first future to become available and its result
\tparam "class InputIterator" A type modelling an iterator
\param first An iterator pointing to the first future to wait upon
\param last An iterator pointing to after the last future to wait upon
\ingroup when_all_futures
\qbk{distinguish, from ___WG21_N3634__ for range of futures}
\complexity{The same as boost::wait_for_any()}
\exceptionmodel{The same as a future}
*/
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
	/*! \brief The abstract base class encapsulating a platform-specific file handle
    */
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
		//! Returns the native handle of this io handle. On POSIX, you can cast this to a fd using `(int)(size_t) native_handle()`. On Windows it's a simple `(HANDLE) native_handle()`.
		virtual void *native_handle() const=0;
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


#define BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(type) \
inline constexpr type operator&(type a, type b) \
{ \
	return static_cast<type>(static_cast<size_t>(a) & static_cast<size_t>(b)); \
} \
inline constexpr type operator|(type a, type b) \
{ \
	return static_cast<type>(static_cast<size_t>(a) | static_cast<size_t>(b)); \
} \
inline constexpr type operator~(type a) \
{ \
	return static_cast<type>(~static_cast<size_t>(a)); \
} \
inline constexpr bool operator!(type a) \
{ \
	return 0==static_cast<size_t>(a); \
}
/*! \enum file_flags
\brief Bitwise file and directory open flags
\ingroup file_flags
*/
#ifdef DOXYGEN_NO_CLASS_ENUMS
enum file_flags
#else
enum class file_flags : size_t
#endif
{
	None=0,				//!< No flags set
	Read=1,				//!< Read access
	Write=2,			//!< Write access
	ReadWrite=3,		//!< Read and write access
	Append=4,			//!< Append only
	Truncate=8,			//!< Truncate existing file to zero
	Create=16,			//!< Open and create if doesn't exist
	CreateOnlyIfNotExist=32, //!< Create and open only if doesn't exist
	AutoFlush=64,		//!< Automatically initiate an asynchronous flush just before file close, and fuse both operations so both must complete for close to complete.
	WillBeSequentiallyAccessed=128, //!< Will be exclusively either read or written sequentially. If you're exclusively writing sequentially, \em strongly consider turning on OSDirect too.
	FastDirectoryEnumeration=256, //!< Hold a file handle open to the containing directory of each open file for fast directory enumeration (POSIX only).

	OSDirect=(1<<16),	//!< Bypass the OS file buffers (only really useful for writing large files. Note you must 4Kb align everything if this is on)
	OSSync=(1<<17)		//!< Ask the OS to not complete until the data is on the physical storage. Best used only with Direct, otherwise use AutoFlush.

};
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(file_flags)
/*! \enum async_op_flags
\brief Bitwise async_op_flags flags
\ingroup async_op_flags
*/
#ifdef DOXYGEN_NO_CLASS_ENUMS
enum async_op_flags
#else
enum class async_op_flags : size_t
#endif
{
	None=0,					//!< No flags set
	DetachedFuture=1,		//!< The specified completion routine may choose to not complete immediately
	ImmediateCompletion=2	//!< Call chained completion immediately instead of scheduling for later. Make SURE your completion can not block!
};
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(async_op_flags)


/*! \class async_file_io_dispatcher_base
\brief Abstract base class for dispatching file i/o asynchronously

This is a reference counted instance with platform-specific implementation optionally hidden in object code.
Construct an instance using the `boost::afio::async_file_io_dispatcher()` function.

\qbk{
[/ link afio.reference.functions.async_file_io_dispatcher `async_file_io_dispatcher()`]
[include generated/group_async_file_io_dispatcher_base__completion.qbk]
[include generated/group_async_file_io_dispatcher_base__call.qbk]
[include generated/group_async_file_io_dispatcher_base__filedirops.qbk]
[include generated/group_async_file_io_dispatcher_base__barrier.qbk]
}
*/
class BOOST_AFIO_DECL async_file_io_dispatcher_base : public std::enable_shared_from_this<async_file_io_dispatcher_base>
{
	//friend BOOST_AFIO_DECL std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);
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
	//! Destroys the dispatcher, blocking inefficiently if any ops are still in flight.
	virtual ~async_file_io_dispatcher_base();

	//! Returns the thread pool used by this dispatcher
	thread_pool &threadpool() const;
	//! Returns file flags as would be used after forcing and masking bits passed during construction
	file_flags fileflags(file_flags flags) const;
	//! Returns the current wait queue depth of this dispatcher
	size_t wait_queue_depth() const;
	//! Returns the number of open items in this dispatcher
	size_t count() const;

    //! The type returned by a completion handler \ingroup async_file_io_dispatcher_base__completion
	typedef std::pair<bool, std::shared_ptr<detail::async_io_handle>> completion_returntype;
    //! The type of a completion handler \ingroup async_file_io_dispatcher_base__completion
	typedef completion_returntype completion_t(size_t, std::shared_ptr<detail::async_io_handle>);
	/*! \brief Schedule a batch of asynchronous invocations of the specified functions when their supplied operations complete.
    \return A batch of op handles
    \param ops A batch of precondition op handles.
    \param callbacks A batch of pairs of op flags and bound completion handler functions of type `completion_t`
    \ingroup async_file_io_dispatcher_base__completion
    \qbk{distinguish, batch bound functions}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete.}
    \exceptionmodelstd
    \qexample{completion_example}
    */
	std::vector<async_io_op> completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> &callbacks);
	/*! \brief Schedule the asynchronous invocation of the specified single function when the supplied single operation completes.
    \return An op handle
    \param req A precondition op handle
    \param callback A pair of op flag and bound completion handler function of type `completion_t`
    \ingroup async_file_io_dispatcher_base__completion
    \qbk{distinguish, single bound function}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete.}
    \exceptionmodelstd
    \qexample{completion_example}
    */
	inline async_io_op completion(const async_io_op &req, const std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>> &callback);

	/*! \brief Schedule a batch of asynchronous invocations of the specified bound functions when their supplied preconditions complete.
    
    This is effectively a convenience wrapper for `completion()`. It creates a packaged_task matching the `completion_t`
    handler specification and calls the specified arbitrary callable, always returning completion on exit.
    
    \return A pair with a batch of futures returning the result of each of the callables and a batch of op handles.
    \tparam "class R" A compiler deduced return type of the bound functions.
    \param ops A batch of precondition op handles. If default constructed, a precondition is null.
    \param callables A batch of bound functions to call, returning R.
    \ingroup async_file_io_dispatcher_base__call
    \qbk{distinguish, batch bound functions}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete.}
    \exceptionmodelstd
    \qexample{call_example}
    */
	template<class R> inline std::pair<std::vector<future<R>>, std::vector<async_io_op>> call(const std::vector<async_io_op> &ops, const std::vector<std::function<R()>> &callables);
	/*! \brief Schedule a batch of asynchronous invocations of the specified bound functions when their supplied preconditions complete.

    This is effectively a convenience wrapper for `completion()`. It creates a packaged_task matching the `completion_t`
    handler specification and calls the specified arbitrary callable, always returning completion on exit. If you
    are seeing performance issues, using `completion()` directly will have much less overhead.
    
    \return A pair with a batch of futures returning the result of each of the callables and a batch of op handles.
    \tparam "class R" A compiler deduced return type of the bound functions.
    \param callables A batch of bound functions to call, returning R.
    \ingroup async_file_io_dispatcher_base__call
    \qbk{distinguish, batch bound functions without preconditions}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete.}
    \exceptionmodelstd
    \qexample{call_example}
    */
	template<class R> std::pair<std::vector<future<R>>, std::vector<async_io_op>> call(const std::vector<std::function<R()>> &callables) { return call(std::vector<async_io_op>(), callables); }
	/*! \brief Schedule an asynchronous invocation of the specified bound function when its supplied precondition completes.

    This is effectively a convenience wrapper for `completion()`. It creates a packaged_task matching the `completion_t`
    handler specification and calls the specified arbitrary callable, always returning completion on exit. If you
    are seeing performance issues, using `completion()` directly will have much less overhead.
    
    \return A pair with a future returning the result of the callable and an op handle.
    \tparam "class R" A compiler deduced return type of the bound functions.
    \param req A precondition op handle. If default constructed, the precondition is null.
    \param callback A bound functions to call, returning R.
    \ingroup async_file_io_dispatcher__call
    \qbk{distinguish, single bound function}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete.}
    \exceptionmodelstd
    \qexample{call_example}
    */
	template<class R> inline std::pair<future<R>, async_io_op> call(const async_io_op &req, std::function<R()> callback);
	/*! \brief Schedule an asynchronous invocation of the specified unbound callable when its supplied precondition completes.
    Note that this function essentially calls `std::bind()` on the callable and the args and passes it to the other call() overload taking a `std::function<>`.
    You should therefore use `std::ref()` etc. as appropriate.

    This is effectively a convenience wrapper for `completion()`. It creates a packaged_task matching the `completion_t`
    handler specification and calls the specified arbitrary callable, always returning completion on exit. If you
    are seeing performance issues, using `completion()` directly will have much less overhead.
    
    \return A pair with a future returning the result of the callable and an op handle.
    \tparam "class C" Any callable type.
    \tparam Args Any sequence of argument types.
    \param req A precondition op handle. If default constructed, the precondition is null.
    \param callback An unbound callable to call.
    \param args An arbitrary sequence of arguments to bind to the callable.
    \ingroup async_file_io_dispatcher_base__call
    \qbk{distinguish, single unbound callable}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete.}
    \exceptionmodelstd
    \qexample{call_example}
    */
	template<class C, class... Args> inline std::pair<future<typename std::result_of<C(Args...)>::type>, async_io_op> call(const async_io_op &req, C callback, Args... args);

	/*! \brief Schedule a batch of asynchronous directory creations and opens after optional preconditions.
    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if directory creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	virtual std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs)=0;
	/*! \brief Schedule an asynchronous directory creation and open after an optional precondition.
    \return An op handle.
    \param req An `async_path_op_req` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if directory creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	inline async_io_op dir(const async_path_op_req &req);
	/*! \brief Schedule a batch of asynchronous directory deletions after optional preconditions.
    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if directory deletion is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	virtual std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs)=0;
	/*! \brief Schedule an asynchronous directory deletion after an optional precondition.
    \return An op handle.
    \param req An `async_path_op_req` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if directory deletion is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	inline async_io_op rmdir(const async_path_op_req &req);
	/*! \brief Schedule a batch of asynchronous file creations and opens after optional preconditions.
    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if file creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	virtual std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs)=0;
	/*! \brief Schedule an asynchronous file creation and open after an optional precondition.
    \return An op handle.
    \param req An `async_path_op_req` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if file creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	inline async_io_op file(const async_path_op_req &req);
	/*! \brief Schedule a batch of asynchronous file deletions after optional preconditions.
    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if file deletion is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	virtual std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs)=0;
	/*! \brief Schedule an asynchronous file deletion after an optional precondition.
    \return An op handle.
    \param req An `async_path_op_req` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if file deletion is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	inline async_io_op rmfile(const async_path_op_req &req);
	/*! \brief Schedule a batch of asynchronous content synchronisations with physical storage after preceding operations.
    \return A batch of op handles.
    \param ops A batch of op handles.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if content synchronisation is constant time (which is extremely unlikely).}
    \exceptionmodelstd
    \qexample{sync_example}
    */
	virtual std::vector<async_io_op> sync(const std::vector<async_io_op> &ops)=0;
	/*! \brief Schedule an asynchronous content synchronisation with physical storage after a preceding operation.
    \return An op handle.
    \param req An op handle.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if content synchronisation is constant time (which is extremely unlikely).}
    \exceptionmodelstd
    \qexample{sync_example}
    */
	inline async_io_op sync(const async_io_op &req);
	/*! \brief Schedule a batch of asynchronous file or directory handle closes after preceding operations.
    \return A batch of op handles.
    \param ops A batch of op handles.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if closing handles is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	virtual std::vector<async_io_op> close(const std::vector<async_io_op> &ops)=0;
	/*! \brief Schedule an asynchronous file or directory handle close after a preceding operation.
    \return An op handle.
    \param req An op handle.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if closing handles is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
	inline async_io_op close(const async_io_op &req);

	/*! \brief Schedule a batch of asynchronous data reads after preceding operations.
    \return A batch of op handles.
    \param ops A batch of `async_data_op_req<void>` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if reading data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
	virtual std::vector<async_io_op> read(const std::vector<async_data_op_req<void>> &ops)=0;
	/*! \brief Schedule an asynchronous data read after a preceding operation.
    \return An op handle.
    \param req An `async_data_op_req<void>` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if reading data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
	inline async_io_op read(const async_data_op_req<void> &req);
	/*! \brief Schedule a batch of asynchronous data reads after preceding operations.
    \return A batch of op handles.
    \tparam "class T" Any arbitrary type.
    \param ops A batch of `async_data_op_req<T>` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if reading data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
	template<class T> inline std::vector<async_io_op> read(const std::vector<async_data_op_req<T>> &ops);
	/*! \brief Schedule a batch of asynchronous data writes after preceding operations.
    \return A batch of op handles.
    \param ops A batch of `async_data_op_req<const void>` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if writing data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
	virtual std::vector<async_io_op> write(const std::vector<async_data_op_req<const void>> &ops)=0;
	/*! \brief Schedule an asynchronous data write after a preceding operation.
    \return An op handle.
    \param req An `async_data_op_req<const void>` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if writing data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
	inline async_io_op write(const async_data_op_req<const void> &req);
	/*! \brief Schedule a batch of asynchronous data writes after preceding operations.
    \return A batch of op handles.
    \tparam "class T" Any arbitrary type.
    \param ops A batch of `async_data_op_req<T>` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if writing data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
	template<class T> inline std::vector<async_io_op> write(const std::vector<async_data_op_req<T>> &ops);

	/*! \brief Schedule a batch of asynchronous file length truncations after preceding operations.
    \return A batch of op handles.
    \param ops A batch of op handles.
    \param sizes A batch of new lengths.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if truncating file lengths is constant time.}
    \exceptionmodelstd
    \qexample{truncate_example}
    */
	virtual std::vector<async_io_op> truncate(const std::vector<async_io_op> &ops, const std::vector<off_t> &sizes)=0;
	/*! \brief Schedule an asynchronous data write after a preceding operation.
    \return An op handle.
    \param op An op handle.
    \param newsize The new size for the file.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if truncating file lengths is constant time.}
    \exceptionmodelstd
    \qexample{truncate_example}
    */
	inline async_io_op truncate(const async_io_op &op, off_t newsize);
	/*! \brief Schedule an asynchronous synchronisation of preceding operations.
    
    If you perform many asynchronous operations of unequal duration but wish to schedule one of more operations
    to occur only after \b all of those operations have completed, this is the correct function to use. The returned
    batch of ops exactly match the input batch of ops (including their exception states), but they will only
    complete when the last of the input batch of ops completes.
    
    \note If an input op is in an exceptioned state at the point of entry into this function, this function
    will propagate the exception there and then. \em Only error states which occur \em after this function
    has been scheduled are propagated into the output set of ops.
    
    \return A batch of op handles.
    \param ops A batch of op handles.
    \ingroup async_file_io_dispatcher_base__barrier
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N) to complete.}
    \exceptionmodel{See detailed description above.}
    \qexample{barrier_example}
    */
	std::vector<async_io_op> barrier(const std::vector<async_io_op> &ops);
protected:
	void complete_async_op(size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr e=exception_ptr());
	completion_returntype invoke_user_completion(size_t id, std::shared_ptr<detail::async_io_handle> h, std::function<completion_t> callback);
	template<class F, class... Args> std::shared_ptr<detail::async_io_handle> invoke_async_op_completions(size_t id, std::shared_ptr<detail::async_io_handle> h, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args);
	template<class F, class... Args> async_io_op chain_async_op(detail::immediate_async_ops &immediates, int optype, const async_io_op &precondition, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, Args...), Args... args);
	template<class F, class T> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_io_op> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, T));
	template<class F> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_io_op> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_io_op));
	template<class F> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_path_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_path_op_req));
	template<class F, class T> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_data_op_req<T>> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, async_data_op_req<T>));
	template<class T> async_file_io_dispatcher_base::completion_returntype dobarrier(size_t id, std::shared_ptr<detail::async_io_handle> h, T);
};
/*! \brief Instatiates the best available async_file_io_dispatcher implementation for this system.

Note that the number of threads in the threadpool supplied is the maximum non-async op queue depth (e.g. file opens, closes etc.).
For fast SSDs, there isn't much gain after eight-sixteen threads, so the process threadpool is set to eight by default.
For slow hard drives, or worse, SANs, a queue depth of 64 or higher might deliver significant benefits.

\return A shared_ptr to the best available async_file_io_dispatcher implementation for this system.
\param threadpool The threadpool instance to use for asynchronous dispatch.
\param flagsforce The flags to bitwise OR with any opened file flags. Used to force on certain flags.
\param flagsmask The flags to bitwise AND with any opened file flags. Used to force off certain flags.
\ingroup async_file_io_dispatcher
\qbk{
[heading Example]
[call_example]
}
*/
extern BOOST_AFIO_DECL std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);

/*! \struct async_io_op
\brief A reference to an asynchronous operation

The id field is always valid (and non-zero) if this reference is valid. The h field, being the shared state
between all references referring to the same op, only becomes a non-default shared_future when the op has
actually begun execution. You should therefore \b never try waiting via h->get() until you are absolutely sure
that the op has already started.
*/
struct async_io_op
{
	std::shared_ptr<async_file_io_dispatcher_base> parent; //!< The parent dispatcher
	size_t id;											//!< A unique id for this operation
	std::shared_ptr<shared_future<std::shared_ptr<detail::async_io_handle>>> h;	//!< A future handle to the item being operated upon

    //! \constr
	async_io_op() : id(0), h(std::make_shared<shared_future<std::shared_ptr<detail::async_io_handle>>>()) { }
    //! \cconstr
	async_io_op(const async_io_op &o) : parent(o.parent), id(o.id), h(o.h) { }
    //! \mconstr
	async_io_op(async_io_op &&o) : parent(std::move(o.parent)), id(std::move(o.id)), h(std::move(o.h)) { }
    /*! Constructs an instance.
    \param _parent The dispatcher this op belongs to.
    \param _id The unique non-zero id of this op.
    \param _handle A shared_ptr to shared state between all instances of this reference.
    */
	async_io_op(std::shared_ptr<async_file_io_dispatcher_base> _parent, size_t _id, std::shared_ptr<shared_future<std::shared_ptr<detail::async_io_handle>>> _handle) : parent(_parent), id(_id), h(std::move(_handle)) { _validate(); }
    /*! Constructs an instance.
    \param _parent The dispatcher this op belongs to.
    \param _id The unique non-zero id of this op.
    */
	async_io_op(std::shared_ptr<async_file_io_dispatcher_base> _parent, size_t _id) : parent(_parent), id(_id), h(std::make_shared<shared_future<std::shared_ptr<detail::async_io_handle>>>()) { }
    //! \cassign
	async_io_op &operator=(const async_io_op &o) { parent=o.parent; id=o.id; h=o.h; return *this; }
    //! \massign
	async_io_op &operator=(async_io_op &&o) { parent=std::move(o.parent); id=std::move(o.id); h=std::move(o.h); return *this; }
	//! Validates contents
	bool validate() const
	{
		if(!parent || !id) return false;
		// If h is valid and ready and contains an exception, throw it now
		if(h->valid() && h->is_ready() /*h->wait_for(seconds(0))==future_status::ready*/)
			h->get();
		return true;
	}
private:
	void _validate() const
	{
#if BOOST_AFIO_VALIDATE_INPUTS
		if(!validate())
			throw std::runtime_error("Inputs are invalid.");
#endif
	}
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
			catch(...)
			{
				exception_ptr e(afio::make_exception_ptr(afio::current_exception()));
				state->done.set_exception(e);
				done=true;
			}
			if(!done)
				state->done.set_value(state->out);
		}
		return std::make_pair(true, h);
	}
}

/*! \brief Returns a result when all the supplied ops complete. Does not propagate exception states.

\return A future vector of shared_ptr's to detail::async_io_handle.
\param _ An instance of std::nothrow_t.
\param first A vector iterator pointing to the first async_io_op to wait upon.
\param last A vector iterator pointing after the last async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, vector batch of ops not exception propagating}
\complexity{O(N) to dispatch. O(N/threadpool) to complete, but at least one cache line is contended between threads.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::nothrow_t _, std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
	if(first==last)
		return future<std::vector<std::shared_ptr<detail::async_io_handle>>>();
	std::vector<async_io_op> inputs(first, last);
	auto state(std::make_shared<detail::when_all_count_completed_state>(inputs.size()));
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
	callbacks.reserve(inputs.size());
	size_t idx=0;
	for(auto &i : inputs)
		callbacks.push_back(std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&detail::when_all_count_completed_nothrow, std::placeholders::_1, std::placeholders::_2, state, idx++)));
	inputs.front().parent->completion(inputs, callbacks);
	return state->done.get_future();
}
/*! \brief Returns a result when all the supplied ops complete. Propagates exception states.

\return A future vector of shared_ptr's to detail::async_io_handle.
\param first A vector iterator pointing to the first async_io_op to wait upon.
\param last A vector iterator pointing after the last async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, vector batch of ops exception propagating}
\complexity{O(N) to dispatch. O(N/threadpool) to complete, but at least one cache line is contended between threads.}
\exceptionmodel{Propagating}
*/
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
	if(first==last)
		return future<std::vector<std::shared_ptr<detail::async_io_handle>>>();
	std::vector<async_io_op> inputs(first, last);
	auto state(std::make_shared<detail::when_all_count_completed_state>(inputs.size()));
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
	callbacks.reserve(inputs.size());
	size_t idx=0;
	for(auto &i : inputs)
		callbacks.push_back(std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&detail::when_all_count_completed, std::placeholders::_1, std::placeholders::_2, state, idx++)));
	inputs.front().parent->completion(inputs, callbacks);
	return state->done.get_future();
}
/*! \brief Returns a result when all the supplied ops complete. Does not propagate exception states.

\return A future vector of shared_ptr's to detail::async_io_handle.
\param _ An instance of std::nothrow_t.
\param _ops A std::initializer_list<> of async_io_op's to wait upon.
\ingroup when_all_ops
\qbk{distinguish, initializer_list batch of ops not exception propagating}
\complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete, but at least one cache line is heavily contended between threads.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::nothrow_t _, std::initializer_list<async_io_op> _ops)
{
	std::vector<async_io_op> ops;
	ops.reserve(_ops.size());
	for(auto &&i : _ops)
		ops.push_back(std::move(i));
	return when_all(_, ops.begin(), ops.end());
}
/*! \brief Returns a result when all the supplied ops complete. Propagates exception states.

\return A future vector of shared_ptr's to detail::async_io_handle.
\param _ops A std::initializer_list<> of async_io_op's to wait upon.
\ingroup when_all_ops
\qbk{distinguish, initializer_list batch of ops exception propagating}
\complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete, but at least one cache line is heavily contended between threads.}
\exceptionmodel{Propagating}
*/
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::initializer_list<async_io_op> _ops)
{
	std::vector<async_io_op> ops;
	ops.reserve(_ops.size());
	for(auto &&i : _ops)
		ops.push_back(std::move(i));
	return when_all(ops.begin(), ops.end());
}
/*! \brief Returns a result when the supplied op completes. Does not propagate exception states.

\return A future vector of shared_ptr's to detail::async_io_handle.
\param _ An instance of std::nothrow_t.
\param op An async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, convenience single op not exception propagating}
\complexity{O(1) to dispatch. O(1) to complete.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::nothrow_t _, async_io_op op)
{
	std::vector<async_io_op> ops(1, op);
	return when_all(_, ops.begin(), ops.end());
}
/*! \brief Returns a result when the supplied op completes. Propagates exception states.

\return A future vector of shared_ptr's to detail::async_io_handle.
\param op An async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, convenience single op exception propagating}
\complexity{O(1) to dispatch. O(1) to complete.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(async_io_op op)
{
	std::vector<async_io_op> ops(1, op);
	return when_all(ops.begin(), ops.end());
}

/*! \struct async_path_op_req
\brief A convenience bundle of path and flags, with optional precondition
*/
struct async_path_op_req
{
	std::filesystem::path path; //!< The filing system path to be used for this operation
	file_flags flags;           //!< The flags to be used for this operation (note they can be overriden by flags passed during dispatcher construction).
	async_io_op precondition;   //!< An optional precondition for this operation
    //! \constr
	async_path_op_req() : flags(file_flags::None) { }
	/*! \brief Constructs an instance.
    
    Note that this will fail if path is not absolute.
    
    \param _path The filing system path to be used. Must be absolute.
    \param _flags The flags to be used.
    */
	async_path_op_req(std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags) { if(!path.is_absolute()) throw std::runtime_error("Non-absolute path"); }
	/*! \brief Constructs an instance.
    
    Note that this will fail if path is not absolute.
    
    \param _precondition The precondition for this operation.
    \param _path The filing system path to be used. Must be absolute.
    \param _flags The flags to be used.
    */
	async_path_op_req(async_io_op _precondition, std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags), precondition(std::move(_precondition)) { _validate(); if(!path.is_absolute()) throw std::runtime_error("Non-absolute path"); }
	/*! \brief Constructs an instance.
    \param _path The filing system leaf or path to be used. make_preferred() and absolute() will be used to convert the string into an absolute path.
    \param _flags The flags to be used.
    */
	async_path_op_req(std::string _path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags) { _validate(); }
	/*! \brief Constructs an instance.
    \param _precondition The precondition for this operation.
    \param _path The filing system leaf or path to be used. make_preferred() and absolute() will be used to convert the string into an absolute path.
    \param _flags The flags to be used.
    */
	async_path_op_req(async_io_op _precondition, std::string _path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags), precondition(std::move(_precondition)) { _validate(); }
	/*! \brief Constructs an instance.
    \param _path The filing system leaf or path to be used. make_preferred() and absolute() will be used to convert the string into an absolute path.
    \param _flags The flags to be used.
    */
	async_path_op_req(const char *_path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags) { _validate(); }
	/*! \brief Constructs an instance.
    \param _precondition The precondition for this operation.
    \param _path The filing system leaf or path to be used. make_preferred() and absolute() will be used to convert the string into an absolute path.
    \param _flags The flags to be used.
    */
	async_path_op_req(async_io_op _precondition, const char *_path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags), precondition(std::move(_precondition)) { _validate(); }
	//! Validates contents
	bool validate() const
	{
		if(path.empty()) return false;
		return !precondition.id || precondition.validate();
	}
private:
	void _validate() const
	{
#if BOOST_AFIO_VALIDATE_INPUTS
		if(!validate())
			throw std::runtime_error("Inputs are invalid.");
#endif
	}
};

//! \brief A convenience bundle of precondition, data and where for reading into a void *. Data \b MUST stay around until the operation completes. \ingroup async_data_op_req
template<> struct async_data_op_req<void> // For reading
{
    //! An optional precondition for this operation
	async_io_op precondition;
	//! A sequence of mutable Boost.ASIO buffers to read into
	std::vector<boost::asio::mutable_buffer> buffers;
	//! The offset from which to read
	off_t where;
	//! \constr
	async_data_op_req() { }
	//! \cconstr
	async_data_op_req(const async_data_op_req &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
	//! \mconstr
	async_data_op_req(async_data_op_req &&o) : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
	//! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
	//! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
	//! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, void *v, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::mutable_buffer(v, _length)); _validate(); }
	//! \async_data_op_req2
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::mutable_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(_buffers), where(_where) { _validate(); }
	//! Validates contents for correctness \return True if contents are correct
	bool validate() const
	{
		if(!precondition.validate()) return false;
		if(buffers.empty()) return false;
		for(auto &b : buffers)
		{
			if(!boost::asio::buffer_cast<const void *>(b) || !boost::asio::buffer_size(b)) return false;
			if(!!(precondition.parent->fileflags(file_flags::None)&file_flags::OSDirect))
			{
				if(((size_t)boost::asio::buffer_cast<const void *>(b) & 4095) || (boost::asio::buffer_size(b) & 4095)) return false;
			}
		}
		return true;
	}
private:
	void _validate() const
	{
#if BOOST_AFIO_VALIDATE_INPUTS
		if(!validate())
			throw std::runtime_error("Inputs are invalid.");
#endif
	}
};
//! \brief A convenience bundle of precondition, data and where for writing from a const void *. Data \b MUST stay around until the operation completes. \ingroup async_data_op_req
template<> struct async_data_op_req<const void> // For writing
{
    //! An optional precondition for this operation
	async_io_op precondition;
	//! A sequence of const Boost.ASIO buffers to write from
	std::vector<boost::asio::const_buffer> buffers;
	//! The offset into which to write
	off_t where;
	//! \constr
	async_data_op_req() { }
	//! \cconstr
	async_data_op_req(const async_data_op_req &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
	//! \mconstr
	async_data_op_req(async_data_op_req &&o) : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
	//! \cconstr
	async_data_op_req(const async_data_op_req<void> &o) : precondition(o.precondition), where(o.where) { buffers.reserve(o.buffers.capacity()); for(auto &i: o.buffers) buffers.push_back(i); }
	//! \mconstr
	async_data_op_req(async_data_op_req<void> &&o) : precondition(std::move(o.precondition)), where(o.where) { buffers.reserve(o.buffers.capacity()); for(auto &&i: o.buffers) buffers.push_back(std::move(i)); }
	//! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
	//! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
	//! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, const void *v, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::const_buffer(v, _length)); _validate(); }
	//! \async_data_op_req2
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::const_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(_buffers), where(_where) { _validate(); }
	//! Validates contents for correctness \return True if contents are correct
	bool validate() const
	{
		if(!precondition.validate()) return false;
		if(buffers.empty()) return false;
		for(auto &b : buffers)
		{
			if(!boost::asio::buffer_cast<const void *>(b) || !boost::asio::buffer_size(b)) return false;
			if(!!(precondition.parent->fileflags(file_flags::None)&file_flags::OSDirect))
			{
				if(((size_t)boost::asio::buffer_cast<const void *>(b) & 4095) || (boost::asio::buffer_size(b) & 4095)) return false;
			}
		}
		return true;
	}
private:
	void _validate() const
	{
#if BOOST_AFIO_VALIDATE_INPUTS
		if(!validate())
			throw std::runtime_error("Inputs are invalid.");
#endif
	}
};
//! \brief A convenience bundle of precondition, data and where for reading into a single T *. Data \b MUST stay around until the operation completes. \tparam "class T" Any writable type T \ingroup async_data_op_req
template<class T> struct async_data_op_req : public async_data_op_req<void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, T *v, size_t _length, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(v), _length, _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a single const T *. Data \b MUST stay around until the operation completes. \tparam "class T" Any readable type T \ingroup async_data_op_req
template<class T> struct async_data_op_req<const T> : public async_data_op_req<const void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cconstr
	async_data_op_req(const async_data_op_req<T> &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<T> &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, const T *v, size_t _length, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(v), _length, _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::vector<T, A>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class T, class A> struct async_data_op_req<std::vector<T, A>> : public async_data_op_req<void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, std::vector<T, A> &v, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const std::vector<T, A>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class T, class A> struct async_data_op_req<const std::vector<T, A>> : public async_data_op_req<const void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cconstr
	async_data_op_req(const async_data_op_req<std::vector<T, A>> &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<std::vector<T, A>> &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, const std::vector<T, A> &v, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::array<T, N>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam N Any compile-time size \ingroup async_data_op_req
template<class T, size_t N> struct async_data_op_req<std::array<T, N>> : public async_data_op_req<void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, std::array<T, N> &v, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const std::array<T, N>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam N Any compile-time size \ingroup async_data_op_req
template<class T, size_t N> struct async_data_op_req<const std::array<T, N>> : public async_data_op_req<const void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
    //! \cconstr
	async_data_op_req(const async_data_op_req<std::array<T, N>> &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<std::array<T, N>> &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, const std::array<T, N> &v, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::basic_string<C, T, A>`. Data \b MUST stay around until the operation completes. \tparam "class C" Any character type \tparam "class T" Character traits type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class C, class T, class A> struct async_data_op_req<std::basic_string<C, T, A>> : public async_data_op_req<void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
    //! \mconstr
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, std::basic_string<C, T, A> &v, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(A), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const std::basic_string<C, T, A>`. Data \b MUST stay around until the operation completes.  \tparam "class C" Any character type \tparam "class T" Character traits type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class C, class T, class A> struct async_data_op_req<const std::basic_string<C, T, A>> : public async_data_op_req<const void>
{
    //! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cconstr
	async_data_op_req(const async_data_op_req<std::basic_string<C, T, A>> &o) : async_data_op_req<const void>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<std::basic_string<C, T, A>> &&o) : async_data_op_req<const void>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
    //! \mconstr
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, const std::basic_string<C, T, A> &v, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(A), _where) { }
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
	return std::move(completion(r, i).front());
}
template<class R> inline std::pair<std::vector<future<R>>, std::vector<async_io_op>> async_file_io_dispatcher_base::call(const std::vector<async_io_op> &ops, const std::vector<std::function<R()>> &callables)
{
	typedef packaged_task<R()> tasktype;
	std::vector<future<R>> retfutures;
	std::vector<std::pair<async_op_flags, std::function<completion_t>>> callbacks;
	retfutures.reserve(callables.size());
	callbacks.reserve(callables.size());
	auto f=[](size_t, std::shared_ptr<detail::async_io_handle> _, std::shared_ptr<tasktype> c) {
		(*c)();
		return std::make_pair(true, _);
	};
	for(auto &t : callables)
	{
		std::shared_ptr<tasktype> c(std::make_shared<tasktype>(std::function<R()>(t)));
		retfutures.push_back(c->get_future());
		callbacks.push_back(std::make_pair(async_op_flags::None, std::bind(f, std::placeholders::_1, std::placeholders::_2, std::move(c))));
	}
	return std::make_pair(std::move(retfutures), completion(ops, callbacks));
}
template<class R> inline std::pair<future<R>, async_io_op> async_file_io_dispatcher_base::call(const async_io_op &req, std::function<R()> callback)
{
	std::vector<async_io_op> i;
	std::vector<std::function<R()>> c;
	i.reserve(1); c.reserve(1);
	i.push_back(req);
	c.push_back(std::move(callback));
	std::pair<std::vector<future<R>>, std::vector<async_io_op>> ret(call(i, c));
	return std::make_pair(std::move(ret.first.front()), ret.second.front());
}
template<class C, class... Args> inline std::pair<future<typename std::result_of<C(Args...)>::type>, async_io_op> async_file_io_dispatcher_base::call(const async_io_op &req, C callback, Args... args)
{
	typedef typename std::result_of<C(Args...)>::type rettype;
	return call(req, std::bind<rettype()>(callback, args...));
}
inline async_io_op async_file_io_dispatcher_base::dir(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(dir(i).front());
}
inline async_io_op async_file_io_dispatcher_base::rmdir(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(rmdir(i).front());
}
inline async_io_op async_file_io_dispatcher_base::file(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(file(i).front());
}
inline async_io_op async_file_io_dispatcher_base::rmfile(const async_path_op_req &req)
{
	std::vector<async_path_op_req> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(rmfile(i).front());
}
inline async_io_op async_file_io_dispatcher_base::sync(const async_io_op &req)
{
	std::vector<async_io_op> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(sync(i).front());
}
inline async_io_op async_file_io_dispatcher_base::close(const async_io_op &req)
{
	std::vector<async_io_op> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(close(i).front());
}
inline async_io_op async_file_io_dispatcher_base::read(const async_data_op_req<void> &req)
{
	std::vector<async_data_op_req<void>> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(read(i).front());
}
inline async_io_op async_file_io_dispatcher_base::write(const async_data_op_req<const void> &req)
{
	std::vector<async_data_op_req<const void>> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(write(i).front());
}
template<class T> inline std::vector<async_io_op> async_file_io_dispatcher_base::read(const std::vector<async_data_op_req<T>> &ops)
{
	return read(detail::async_file_io_dispatcher_rwconverter<T>()(ops));
}
template<class T> inline std::vector<async_io_op> async_file_io_dispatcher_base::write(const std::vector<async_data_op_req<T>> &ops)
{
	return write(detail::async_file_io_dispatcher_rwconverter<T>()(ops));
}
inline async_io_op async_file_io_dispatcher_base::truncate(const async_io_op &op, off_t newsize)
{
	std::vector<async_io_op> o;
	std::vector<off_t> i;
	o.reserve(1);
	o.push_back(op);
	i.reserve(1);
	i.push_back(newsize);
	return std::move(truncate(o, i).front());
}


} } // namespace boost

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

#endif
