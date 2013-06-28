/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef BOOST_AFIO_HPP
#define BOOST_AFIO_HPP

//#include "../NiallsCPP11Utilities/NiallsCPP11Utilities.hpp"
#include "../NiallsCPP11Utilities/std_filesystem.hpp"
#include <type_traits>
#include <initializer_list>
#include <thread>
#include <atomic>
#include <exception>
#if !defined(_WIN32_WINNT) && defined(WIN32)
#define _WIN32_WINNT 0x0501
#endif
//#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
//#define BOOST_THREAD_DONT_PROVIDE_FUTURE
//#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#include "boost/asio.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"
#include "config.hpp"
#include "detail/Utility.hpp"

#if BOOST_VERSION<105300
#error I absolutely need Boost v1.53 or higher to compile (I need lock free containers).
#endif
#if BOOST_VERSION<105400
#ifdef BOOST_MSVC
#pragma message(__FILE__ ": WARNING: Boost v1.53 has a memory corruption bug in boost::packaged_task<> when built under C++11 which makes this library useless. Get a newer Boost!")
#else
#warning Boost v1.53 has a memory corruption bug in boost::packaged_task<> when built under C++11 which makes this library useless. Get a newer Boost!
#endif
#endif


/*
#ifndef BOOST_AFIO_DECL
#ifdef BOOST_AFIO_DLL_EXPORTS
#define BOOST_AFIO_DECL BOOST_SYMBOL_EXPORT
#else
#define BOOST_AFIO_DECL BOOST_SYMBOL_IMPORT
#endif
#endif
*/

//! \def BOOST_AFIO_VALIDATE_INPUTS Validate inputs at the point of instantiation
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

#ifdef __GNUC__
typedef boost::thread thread;
#else
typedef std::thread thread;
#endif

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
using std::current_exception;
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
		auto task=std::make_shared<packaged_task<R()>>(std::move(f));
		service.post(std::bind([](std::shared_ptr<packaged_task<R()>> t) { (*t)(); }, task));
		return task->get_future();
	}
};
//! Returns the process threadpool
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
	//! \brief May occasionally be useful to access to discover information about an open handle
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
		//! Returns the native handle of this io handle
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
/*! \enum open_flags
\brief Bitwise file and directory open flags
*/
enum class file_flags : size_t
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
	FastDirectoryEnumeration=256, //! Hold a file handle open to the containing directory of each open file (POSIX only).

	OSDirect=(1<<16),	//!< Bypass the OS file buffers (only really useful for writing large files. Note you must 4Kb align everything if this is on)
	OSSync=(1<<17)		//!< Ask the OS to not complete until the data is on the physical storage. Best used only with Direct, otherwise use AutoFlush.

};
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(file_flags)
enum class async_op_flags : size_t
{
	None=0,					//!< No flags set
	DetachedFuture=1,		//!< The specified completion routine may choose to not complete immediately
	ImmediateCompletion=2	//!< Call chained completion immediately instead of scheduling for later. Make SURE your completion can not block!
};
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(async_op_flags)


/*! \class async_file_io_dispatcher_base
\brief Abstract base class for dispatching file i/o asynchronously
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

	//! Invoke the specified callable when the supplied operation completes
	template<class R> inline std::pair<std::vector<future<R>>, std::vector<async_io_op>> call(const std::vector<async_io_op> &ops, const std::vector<std::function<R()>> &callables);
	//! Invoke the specified callable when the supplied operation completes
	template<class R> std::pair<std::vector<future<R>>, std::vector<async_io_op>> call(const std::vector<std::function<R()>> &callables) { return call(std::vector<async_io_op>(), callables); }
	//! Invoke the specified callable when the supplied operation completes
	template<class R> inline std::pair<future<R>, async_io_op> call(const async_io_op &req, std::function<R()> callback);
	//! Invoke the specified callable when the supplied operation completes
	template<class C, class... Args> inline std::pair<future<typename std::result_of<C(Args...)>::type>, async_io_op> call(const async_io_op &req, C callback, Args... args);

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

	//! Truncates the lengths of items
	virtual std::vector<async_io_op> truncate(const std::vector<async_io_op> &ops, const std::vector<off_t> &sizes)=0;
	//! Truncates the length of an item
	inline async_io_op truncate(const async_io_op &op, off_t newsize);
	//! Completes each of the supplied ops when and only when the last of the supplied ops completes
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

\em flagsforce is ORed with any opened file flags. The NOT of \em flags mask is ANDed with any opened flags.

Note that the number of threads in the threadpool supplied is the maximum non-async op queue depth (e.g. file opens, closes etc.).
For fast SSDs, there isn't much gain after eight-sixteen threads, so the process threadpool is set to eight by default.
For slow hard drives, or worse, SANs, a queue depth of 64 or higher might deliver significant benefits.
*/
extern BOOST_AFIO_DECL std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);

/*! \struct async_io_op
\brief A reference to an async operation
*/
struct async_io_op
{
	std::shared_ptr<async_file_io_dispatcher_base> parent; //!< The parent dispatcher
	size_t id;											//!< A unique id for this operation
	std::shared_ptr<shared_future<std::shared_ptr<detail::async_io_handle>>> h;	//!< A future handle to the item being operated upon

	async_io_op() : id(0), h(std::make_shared<shared_future<std::shared_ptr<detail::async_io_handle>>>()) { }
	async_io_op(const async_io_op &o) : parent(o.parent), id(o.id), h(o.h) { }
	async_io_op(async_io_op &&o) : parent(std::move(o.parent)), id(std::move(o.id)), h(std::move(o.h)) { }
	async_io_op(std::shared_ptr<async_file_io_dispatcher_base> _parent, size_t _id, std::shared_ptr<shared_future<std::shared_ptr<detail::async_io_handle>>> _handle) : parent(_parent), id(_id), h(std::move(_handle)) { _validate(); }
	async_io_op(std::shared_ptr<async_file_io_dispatcher_base> _parent, size_t _id) : parent(_parent), id(_id), h(std::make_shared<shared_future<std::shared_ptr<detail::async_io_handle>>>()) { }
	async_io_op &operator=(const async_io_op &o) { parent=o.parent; id=o.id; h=o.h; return *this; }
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
				exception_ptr e(afio::make_exception_ptr(current_exception()));
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
	auto state(std::make_shared<detail::when_all_count_completed_state>(inputs.size()));
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
	auto state(std::make_shared<detail::when_all_count_completed_state>(inputs.size()));
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
//! \brief Convenience overload for a single async_io_op.  Does not retrieve exceptions.
inline future<std::vector<std::shared_ptr<detail::async_io_handle>>> when_all(std::nothrow_t _, async_io_op op)
{
	std::vector<async_io_op> ops(1, op);
	return when_all(_, ops.begin(), ops.end());
}
//! \brief Convenience overload for a single async_io_op.  Retrieves exceptions.
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
	std::filesystem::path path;
	file_flags flags;
	async_io_op precondition;
	async_path_op_req() : flags(file_flags::None) { }
	//! Fails is path is not absolute
	async_path_op_req(std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags) { if(!path.is_absolute()) throw std::runtime_error("Non-absolute path"); }
	//! Fails is path is not absolute
	async_path_op_req(async_io_op _precondition, std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags), precondition(std::move(_precondition)) { _validate(); if(!path.is_absolute()) throw std::runtime_error("Non-absolute path"); }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(std::string _path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags) { _validate(); }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(async_io_op _precondition, std::string _path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags), precondition(std::move(_precondition)) { _validate(); }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
	async_path_op_req(const char *_path, file_flags _flags=file_flags::None) : path(std::filesystem::absolute(std::filesystem::path(_path).make_preferred())), flags(_flags) { _validate(); }
	//! Constructs on the basis of a string. make_preferred() and absolute() are called in this case.
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

/*! \struct async_data_op_req
\brief A convenience bundle of precondition, data and where. Data \b MUST stay around until the operation completes.
*/
template<> struct async_data_op_req<void> // For reading
{
	async_io_op precondition;
	std::vector<boost::asio::mutable_buffer> buffers;
	off_t where;
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
	async_data_op_req(async_data_op_req &&o) : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
	async_data_op_req(async_io_op _precondition, void *_buffer, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::mutable_buffer(_buffer, _length)); _validate(); }
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::mutable_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(_buffers), where(_where) { _validate(); }
	//! Validates contents
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
template<> struct async_data_op_req<const void> // For writing
{
	async_io_op precondition;
	std::vector<boost::asio::const_buffer> buffers;
	off_t where;
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
	async_data_op_req(async_data_op_req &&o) : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
	async_data_op_req(const async_data_op_req<void> &o) : precondition(o.precondition), where(o.where) { buffers.reserve(o.buffers.capacity()); for(auto &i: o.buffers) buffers.push_back(i); }
	async_data_op_req(async_data_op_req<void> &&o) : precondition(std::move(o.precondition)), where(o.where) { buffers.reserve(o.buffers.capacity()); for(auto &&i: o.buffers) buffers.push_back(std::move(i)); }
	async_data_op_req &operator=(const async_data_op_req &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
	async_data_op_req(async_io_op _precondition, const void *_buffer, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::const_buffer(_buffer, _length)); _validate(); }
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::const_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(_buffers), where(_where) { _validate(); }
	//! Validates contents
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
//! \brief A specialisation for any pointer to type T
template<class T> struct async_data_op_req : public async_data_op_req<void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
	async_data_op_req(async_io_op _precondition, T *_buffer, size_t _length, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(_buffer), _length, _where) { }
};
template<class T> struct async_data_op_req<const T> : public async_data_op_req<const void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req(const async_data_op_req<T> &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req<T> &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
	async_data_op_req(async_io_op _precondition, const T *_buffer, size_t _length, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(_buffer), _length, _where) { }
};
//! \brief A specialisation for any std::vector<T, A>
template<class T, class A> struct async_data_op_req<std::vector<T, A>> : public async_data_op_req<void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
	async_data_op_req(async_io_op _precondition, std::vector<T, A> &v, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
template<class T, class A> struct async_data_op_req<const std::vector<T, A>> : public async_data_op_req<const void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req(const async_data_op_req<std::vector<T, A>> &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req<std::vector<T, A>> &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
	async_data_op_req(async_io_op _precondition, const std::vector<T, A> &v, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A specialisation for any std::array<T, N>
template<class T, size_t N> struct async_data_op_req<std::array<T, N>> : public async_data_op_req<void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
	async_data_op_req(async_io_op _precondition, std::array<T, N> &v, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
template<class T, size_t N> struct async_data_op_req<const std::array<T, N>> : public async_data_op_req<const void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
	async_data_op_req(const async_data_op_req<std::array<T, N>> &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req<std::array<T, N>> &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req(async_io_op _precondition, const std::array<T, N> &v, off_t _where) : async_data_op_req<const void>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A specialisation for any std::basic_string<C, T, A>
template<class C, class T, class A> struct async_data_op_req<std::basic_string<C, T, A>> : public async_data_op_req<void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<void>>(*this)=std::move(o); return *this; }
	async_data_op_req(async_io_op _precondition, std::basic_string<C, T, A> &v, off_t _where) : async_data_op_req<void>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(A), _where) { }
};
template<class C, class T, class A> struct async_data_op_req<const std::basic_string<C, T, A>> : public async_data_op_req<const void>
{
	async_data_op_req() { }
	async_data_op_req(const async_data_op_req &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req(const async_data_op_req<std::basic_string<C, T, A>> &o) : async_data_op_req<const void>(o) { }
	async_data_op_req(async_data_op_req<std::basic_string<C, T, A>> &&o) : async_data_op_req<const void>(std::move(o)) { }
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<async_data_op_req<const void>>(*this)=o; return *this; }
	async_data_op_req &operator=(async_data_op_req &&o) { static_cast<async_data_op_req<const void>>(*this)=std::move(o); return *this; }
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
