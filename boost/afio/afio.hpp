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
// VS2010 needs D_VARIADIC_MAX set to at least six
#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 && (!defined(_VARIADIC_MAX) || _VARIADIC_MAX < 6)
#error _VARIADIC_MAX needs to be set to at least six to compile Boost.AFIO
#endif

#include "config.hpp"
#include "detail/Utility.hpp"
#include <type_traits>
#ifndef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
#include <initializer_list>
#endif
#include <exception>
#include <algorithm> // Boost.ASIO needs std::min and std::max
#include <cstdint>

//#define BOOST_THREAD_VERSION 4
//#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
//#define BOOST_THREAD_DONT_PROVIDE_FUTURE
//#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK


#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 // Dinkumware without <atomic>
#include "boost/atomic.hpp"
#include "boost/chrono.hpp"
#define BOOST_AFIO_USE_BOOST_ATOMIC
#define BOOST_AFIO_USE_BOOST_CHRONO
#else
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#endif

#include "boost/asio.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"
#include "boost/foreach.hpp"
#include "detail/Preprocessor_variadic.hpp"
#include <boost/detail/scoped_enum_emulation.hpp>

// Map in C++11 stuff if available
#if (defined(__GLIBCXX__) && __GLIBCXX__<=20120920 /* <= GCC 4.7 */) || (defined(BOOST_MSVC) && BOOST_MSVC < 1700)
#include "boost/exception_ptr.hpp"
#include "boost/thread/recursive_mutex.hpp"
namespace boost { namespace afio {
typedef boost::thread thread;
inline boost::thread::id get_this_thread_id() { return boost::this_thread::get_id(); }
inline boost::exception_ptr current_exception() { return boost::current_exception(); }
#define BOOST_AFIO_THROW(x) boost::throw_exception(boost::enable_current_exception(x))
#define BOOST_AFIO_RETHROW throw
typedef boost::recursive_mutex recursive_mutex;
} }
#else
namespace boost { namespace afio {
typedef std::thread thread;
inline std::thread::id get_this_thread_id() { return std::this_thread::get_id(); }
inline std::exception_ptr current_exception() { return std::current_exception(); }
#define BOOST_AFIO_THROW(x) throw x
#define BOOST_AFIO_RETHROW throw
typedef std::recursive_mutex recursive_mutex;
} }
#endif

#if BOOST_VERSION<105400
#error Boosts before v1.54 have a memory corruption bug in boost::packaged_task<> when built under C++11 which makes this library useless. Get a newer Boost!
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
	M(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW : B<T>(std::move(o)) { } \
	M(const M &o) : B<T>(o) { } \
	M(M &&o) BOOST_NOEXCEPT_OR_NOTHROW : B<T>(std::move(o)) { } \
	M &operator=(const B<T> &o) { static_cast<B<T> &&>(*this)=o; return *this; } \
	M &operator=(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
	M &operator=(const M &o) { static_cast<B<T> &&>(*this)=o; return *this; } \
	M &operator=(M &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
};
#define BOOST_AFIO_FORWARD_STL_IMPL_NC(M, B) \
template<class T> class M : public B<T> \
{ \
public: \
	M() { } \
	M(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW : B<T>(std::move(o)) { } \
	M(M &&o) BOOST_NOEXCEPT_OR_NOTHROW : B<T>(std::move(o)) { } \
	M &operator=(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
	M &operator=(M &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
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
using boost::future_status;
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
	packaged_task(Base &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(std::move(o)) { }
	packaged_task(packaged_task &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(static_cast<Base &&>(o)) { }
	template<class T> packaged_task(T &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(std::forward<T>(o)) { }
	packaged_task &operator=(Base &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<Base &&>(*this)=std::move(o); return *this; }
	packaged_task &operator=(packaged_task &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<Base &&>(*this)=std::move(o); return *this; }
};

// Map in an atomic implementation
template <class T> 
class atomic  
#ifdef BOOST_AFIO_USE_BOOST_ATOMIC
	: public boost::atomic<T>
{
	typedef boost::atomic<T> Base;
#else
	: public std::atomic<T>
{
	typedef std::atomic<T> Base;
#endif

public:
	atomic(): Base() {}
	BOOST_CONSTEXPR atomic(T v) BOOST_NOEXCEPT : Base(std::forward<T>(v)) {}
	
#ifdef BOOST_NO_CXX11_DELETED_FUNCTIONS
//private:
    atomic(const Base &) /* =delete */ ;
#else
    atomic(const Base &) = delete;
#endif
};//end boost::afio::atomic

// Map in a chrono implementation
namespace chrono { namespace detail {
	template<typename T> struct ratioToBase { typedef T type; };
} }
template <std::intmax_t N, std::intmax_t D = 1> class ratio
#ifdef BOOST_AFIO_USE_BOOST_CHRONO
	: public boost::ratio<N, D>
{
	typedef boost::ratio<N, D> Base;
#else
	: public std::ratio<N, D>
{
	typedef std::ratio<N, D> Base;
#endif
	template<typename T> friend struct chrono::detail::ratioToBase;
public:
	static BOOST_CONSTEXPR_OR_CONST std::intmax_t num=N;
	static BOOST_CONSTEXPR_OR_CONST std::intmax_t den=D;
};
namespace chrono {
	namespace detail
	{
		template<std::intmax_t N, std::intmax_t D> struct ratioToBase<ratio<N, D>> { typedef typename ratio<N, D>::Base type; };
		template<typename T> struct durationToBase { typedef T type; };
	}
	template<class Rep, class Period = ratio<1> > class duration
#ifdef BOOST_AFIO_USE_BOOST_CHRONO
		: public boost::chrono::duration<Rep, typename detail::ratioToBase<Period>::type>
	{
		typedef boost::chrono::duration<Rep, typename detail::ratioToBase<Period>::type> Base;
#else
		: public std::chrono::duration<Rep, typename detail::ratioToBase<Period>::type>
	{
		typedef std::chrono::duration<Rep, typename detail::ratioToBase<Period>::type> Base;
#endif
		template<typename T> friend struct detail::durationToBase;
	public:
		BOOST_CONSTEXPR duration() : Base() { }
		duration(const Base &o) : Base(o) { }
		template<class Rep2> BOOST_CONSTEXPR explicit duration(const Rep2 &r) : Base(r) { }
#ifdef BOOST_AFIO_USE_BOOST_CHRONO
		template<class Rep2, class Period2> BOOST_CONSTEXPR duration(const boost::chrono::duration<Rep2,Period2> &d) : Base(d) { }
#else
		template<class Rep2, class Period2> BOOST_CONSTEXPR duration(const std::chrono::duration<Rep2,Period2> &d) : Base(d) { }
#endif
		// This fellow needs to be convertible to boost::chrono::duration too.
		boost::chrono::duration<Rep, boost::ratio<Period::num, Period::den>> toBoost() const { return boost::chrono::duration<Rep, boost::ratio<Period::num, Period::den>>(Base::count()); }
	};
	namespace detail
	{
		template<class Rep, class Period> struct durationToBase<duration<Rep, Period>> { typedef typename duration<Rep, Period>::Base type; };
	}
#ifdef BOOST_AFIO_USE_BOOST_CHRONO
	using boost::chrono::system_clock;
	using boost::chrono::high_resolution_clock;
	template <class ToDuration, class Rep, class Period> BOOST_CONSTEXPR ToDuration duration_cast(const boost::chrono::duration<Rep,Period> &d)
	{
		return boost::chrono::duration_cast<typename detail::durationToBase<ToDuration>::type, Rep, typename detail::ratioToBase<Period>::type>(d);
	}
#else
	using std::chrono::system_clock;
	using std::chrono::high_resolution_clock;
	template <class ToDuration, class Rep, class Period> BOOST_CONSTEXPR ToDuration duration_cast(const std::chrono::duration<Rep,Period> &d)
	{
		return std::chrono::duration_cast<typename detail::durationToBase<ToDuration>::type, Rep, Period>(d);
	}
#endif
}

/*! \class thread_source
\brief A source of thread workers
*/
class thread_source {
private:
	template<class R> static void invoke_packagedtask(std::shared_ptr<packaged_task<R()>> t) { (*t)(); }
protected:
	boost::asio::io_service service;
	boost::asio::io_service::work working;
	thread_source() : working(service)
	{
	}
public:
	//! Returns the underlying io_service
	boost::asio::io_service &io_service() { return service; }
	//! Sends some callable entity to the thread pool for execution
	template<class F> future<typename std::result_of<F()>::type> enqueue(F f)
	{
		typedef typename std::result_of<F()>::type R;
		// Somewhat annoyingly, io_service.post() needs its parameter to be copy constructible,
		// and packaged_task is only move constructible, so ...
		auto task=std::make_shared<packaged_task<R()>>(std::move(f));
		service.post(std::bind(&thread_source::invoke_packagedtask<R>, task));
		return task->get_future();
	}
};

/*! \class std_thread_pool
\brief A very simple thread pool based on std::thread or boost::thread
*/
class std_thread_pool : public thread_source {
	class worker
	{
		std_thread_pool *pool;
	public:
		explicit worker(std_thread_pool *p) : pool(p) { }
		void operator()()
		{
			pool->service.run();
		}
	};
	friend class worker;

	std::vector< std::unique_ptr<thread> > workers;
public:
	/*! Constructs a thread pool of \em no workers
    \param no The number of worker threads to create
    */
	explicit std_thread_pool(size_t no)
	{
		workers.reserve(no);
		for(size_t n=0; n<no; n++)
			workers.push_back(std::unique_ptr<thread>(new thread(worker(this))));
	}
    //! Destroys the thread pool, waiting for worker threads to exit beforehand.
	~std_thread_pool()
	{
		service.stop();
		BOOST_FOREACH(auto &i, workers)
        {	i->join();}
	}
};
/*! \brief Returns the process threadpool

On first use, this instantiates a default std_thread_pool running eight threads.
\ingroup process_threadpool
*/
extern BOOST_AFIO_DECL std_thread_pool &process_threadpool();

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
	// VS2010 uses a non-namespace qualified move() in its headers, which generates an ambiguous resolution error
#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 // <= VS2010
	auto futures=std::make_shared<std::vector<future_type>>(boost::make_move_iterator(first), boost::make_move_iterator(last));
#else
	auto futures=std::make_shared<std::vector<future_type>>(std::make_move_iterator(first), std::make_move_iterator(last));
#endif
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
	// VS2010 uses a non-namespace qualified move() in its headers, which generates an ambiguous resolution error
#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 // <= VS2010
	auto futures=std::make_shared<std::vector<future_type>>(boost::make_move_iterator(first), boost::make_move_iterator(last));
#else
	auto futures=std::make_shared<std::vector<future_type>>(std::make_move_iterator(first), std::make_move_iterator(last));
#endif
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

#define BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(type) \
inline BOOST_CONSTEXPR type operator&(type a, type b) \
{ \
	return static_cast<type>(static_cast<size_t>(a) & static_cast<size_t>(b)); \
} \
inline BOOST_CONSTEXPR type operator|(type a, type b) \
{ \
	return static_cast<type>(static_cast<size_t>(a) | static_cast<size_t>(b)); \
} \
inline BOOST_CONSTEXPR type operator~(type a) \
{ \
	return static_cast<type>(~static_cast<size_t>(a)); \
} \
inline BOOST_CONSTEXPR bool operator!(type a) \
{ \
	return 0==static_cast<size_t>(a); \
}

/*! \enum file_flags
\brief Bitwise file and directory open flags
\ingroup file_flags
*/
#ifdef DOXYGEN_NO_CLASS_ENUMS
enum file_flags
#elif defined(BOOST_NO_CXX11_SCOPED_ENUMS)
BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(file_flags, size_t)
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
	WillBeSequentiallyAccessed=128, //!< Will be exclusively either read or written sequentially. If you're exclusively writing sequentially, \em strongly consider turning on OSDirect too.
	FastDirectoryEnumeration=256, //!< Hold a file handle open to the containing directory of each open file for fast directory enumeration (POSIX only).

	OSDirect=(1<<16),	//!< Bypass the OS file buffers (only really useful for writing large files. Note you must 4Kb align everything if this is on)

	AlwaysSync=(1<<24),		//!< Ask the OS to not complete until the data is on the physical storage. Best used only with OSDirect, otherwise use SyncOnClose.
	SyncOnClose=(1<<25),	//!< Automatically initiate an asynchronous flush just before file close, and fuse both operations so both must complete for close to complete.
	EnforceDependencyWriteOrder=(1<<26) //!< Ensure that data writes to files reach physical storage in the same order as the op dependencies close files. Does NOT enforce ordering of individual data writes, ONLY all file writes accumulated before a file close.
}
#ifdef BOOST_NO_CXX11_SCOPED_ENUMS
BOOST_SCOPED_ENUM_DECLARE_END(file_flags)
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(file_flags::enum_type)
#else
;
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(file_flags)
#endif

/*! \enum async_op_flags
\brief Bitwise async_op_flags flags
\ingroup async_op_flags
*/
#ifdef DOXYGEN_NO_CLASS_ENUMS
enum async_op_flags
#elif defined(BOOST_NO_CXX11_SCOPED_ENUMS)
BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(async_op_flags, size_t)
#else
enum class async_op_flags : size_t
#endif
{
	None=0,					//!< No flags set
	DetachedFuture=1,		//!< The specified completion routine may choose to not complete immediately
	ImmediateCompletion=2	//!< Call chained completion immediately instead of scheduling for later. Make SURE your completion can not block!
}
#ifdef BOOST_NO_CXX11_SCOPED_ENUMS
BOOST_SCOPED_ENUM_DECLARE_END(async_op_flags)
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(async_op_flags::enum_type)
#else
;
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(async_op_flags)
#endif

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
		chrono::system_clock::time_point _opened;
		std::filesystem::path _path; // guaranteed canonical
		file_flags _flags;
	protected:
		boost::afio::atomic<off_t> bytesread, byteswritten, byteswrittenatlastfsync;
		async_io_handle(async_file_io_dispatcher_base *parent, const std::filesystem::path &path, file_flags flags) : _parent(parent), _opened(chrono::system_clock::now()), _path(path), _flags(flags), bytesread(0), byteswritten(0), byteswrittenatlastfsync(0) { }
	public:
		virtual ~async_io_handle() { }
		//! Returns the parent of this io handle
		async_file_io_dispatcher_base *parent() const { return _parent; }
		//! Returns the native handle of this io handle. On POSIX, you can cast this to a fd using `(int)(size_t) native_handle()`. On Windows it's a simple `(HANDLE) native_handle()`.
		virtual void *native_handle() const=0;
		//! Returns when this handle was opened
		const chrono::system_clock::time_point &opened() const { return _opened; }
		//! Returns the path of this io handle
		const std::filesystem::path &path() const { return _path; }
		//! Returns the final flags used when this handle was opened
		file_flags flags() const { return _flags; }
		//! Returns how many bytes have been read since this handle was opened.
		off_t read_count() const { return bytesread; }
		//! Returns how many bytes have been written since this handle was opened.
		off_t write_count() const { return byteswritten; }
		//! Returns how many bytes have been written since this handle was last fsynced.
		off_t write_count_since_fsync() const { return byteswritten-byteswrittenatlastfsync; }
	};
	struct immediate_async_ops;
	template<bool for_writing> class async_data_op_req_impl;
}


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
[include generated/group_async_file_io_dispatcher_base__misc.qbk]
}
*/
class BOOST_AFIO_DECL async_file_io_dispatcher_base : public std::enable_shared_from_this<async_file_io_dispatcher_base>
{
	//friend BOOST_AFIO_DECL std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_source &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);
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
	async_file_io_dispatcher_base(thread_source &threadpool, file_flags flagsforce, file_flags flagsmask);
public:
	//! Destroys the dispatcher, blocking inefficiently if any ops are still in flight.
	virtual ~async_file_io_dispatcher_base();

	//! Returns the thread source used by this dispatcher
	thread_source &threadsource() const;
	//! Returns file flags as would be used after forcing and masking bits passed during construction
	file_flags fileflags(file_flags flags) const;
	//! Returns the current wait queue depth of this dispatcher
	size_t wait_queue_depth() const;
	//! Returns the number of open items in this dispatcher
	size_t count() const;

    //! The type returned by a completion handler \ingroup async_file_io_dispatcher_base__completion
	typedef std::pair<bool, std::shared_ptr<detail::async_io_handle>> completion_returntype;
    //! The type of a completion handler \ingroup async_file_io_dispatcher_base__completion
	typedef completion_returntype completion_t(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *);
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

    
    
    
#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
     
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
	template<class C, class... Args> inline std::pair<future<typename boost::result_of<C(Args...)>::type>, async_io_op> call(const async_io_op &req, C callback, Args... args);

#else   //define a version compatable with c++03

#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class C                                                                               \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                               \
    inline std::pair<future<typename boost::result_of<C(BOOST_PP_ENUM_PARAMS(N, A))>::type>, async_io_op>  \
    call (const async_io_op &req, C callback BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_BINARY_PARAMS(N, A, a));     

  
#define BOOST_PP_LOCAL_LIMITS     (1, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()

#endif





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

	\direct_io_note
    \return A batch of op handles.
	\tparam "class T" Any type.
    \param ops A batch of async_data_op_req<T> structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if reading data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	virtual std::vector<async_io_op> read(const std::vector<detail::async_data_op_req_impl<false>> &ops)=0;
	template<class T> inline std::vector<async_io_op> read(const std::vector<async_data_op_req<T>> &ops);
#else
	template<class T> virtual std::vector<async_io_op> read(const std::vector<async_data_op_req<T>> &ops)=0;
#endif
	/*! \brief Schedule an asynchronous data read after a preceding operation.

	\direct_io_note
	\return An op handle.
	\tparam "class T" Any type.
	\param req An async_data_op_req<T> structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if reading data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	inline async_io_op read(const detail::async_data_op_req_impl<false> &req);
#else
	template<class T> inline async_io_op read(const async_data_op_req<T> &req);
#endif
	/*! \brief Schedule a batch of asynchronous data writes after preceding operations.

	\direct_io_note
	\return A batch of op handles.
	\tparam "class T" Any type.
	\param ops A batch of async_data_op_req<const T> structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if writing data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	virtual std::vector<async_io_op> write(const std::vector<detail::async_data_op_req_impl<true>> &ops)=0;
	template<class T> inline std::vector<async_io_op> write(const std::vector<async_data_op_req<T>> &ops);
#else
	template<class T> virtual std::vector<async_io_op> write(const std::vector<async_data_op_req<const T>> &ops)=0;
#endif
	/*! \brief Schedule an asynchronous data write after a preceding operation.

	\direct_io_note
	\return An op handle.
	\tparam "class T" Any type.
	\param req An async_data_op_req<const T> structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if writing data is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	inline async_io_op write(const detail::async_data_op_req_impl<true> &req);
#else
	template<class T> inline async_io_op write(const async_data_op_req<const T> &req);
#endif

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

	/*! \brief Returns the page size of this architecture which is useful for calculating direct i/o multiples.
	\return The page size of this architecture.
	\ingroup async_file_io_dispatcher_base__misc
	\complexity{Whatever the system API takes (one would hope constant time).}
	\exceptionmodel{Never throws any exception.}
	*/
	static size_t page_size() BOOST_NOEXCEPT_OR_NOTHROW;
protected:
	void complete_async_op(size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr e=exception_ptr());
	completion_returntype invoke_user_completion(size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr *e, std::function<completion_t> callback);
	template<class F, class T> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_io_op> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *, T));
	template<class F> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_io_op> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *, async_io_op));
	template<class F> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_path_op_req> &container, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *, async_path_op_req));
	template<class F, bool iswrite> std::vector<async_io_op> chain_async_ops(int optype, const std::vector<detail::async_data_op_req_impl<iswrite>> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *, detail::async_data_op_req_impl<iswrite>));
	template<class T> async_file_io_dispatcher_base::completion_returntype dobarrier(size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr *, T);

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
    
    template<class F, class... Args> std::shared_ptr<detail::async_io_handle> invoke_async_op_completions(size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr *e, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *, Args...), Args... args);
    template<class F, class... Args> async_io_op chain_async_op(detail::immediate_async_ops &immediates, int optype, const async_io_op &precondition, async_op_flags flags, completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr *, Args...), Args... args);

#else

#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                                                               \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                               \
    std::shared_ptr<detail::async_io_handle>                                                        \
    invoke_async_op_completions                                                                     \
    (size_t id, std::shared_ptr<detail::async_io_handle> h, exception_ptr *,                        \
    completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr * \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, A))                                                                     \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_BINARY_PARAMS(N, A, a));     /* parameters end */     

#define BOOST_PP_LOCAL_LIMITS     (0, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()



#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                /* template start */                           \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                /* template end */                             \
    async_io_op                                      /* return type */                              \
    chain_async_op                                   /* function name */                            \
    (detail::immediate_async_ops &immediates, int optype,           /* parameters start */          \
    const async_io_op &precondition,async_op_flags flags,                                           \
    completion_returntype (F::*f)(size_t, std::shared_ptr<detail::async_io_handle>, exception_ptr * \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, A))                                                                     \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_BINARY_PARAMS(N, A, a));     /* parameters end */

  
#define BOOST_PP_LOCAL_LIMITS     (0, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()

#endif
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
extern BOOST_AFIO_DECL std::shared_ptr<async_file_io_dispatcher_base> make_async_file_io_dispatcher(thread_source &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);

/*! \struct async_io_op
\brief A reference to an asynchronous operation

The id field is always valid (and non-zero) if this reference is valid. The h field, being the shared state
between all references referring to the same op, only becomes a non-default shared_future when the op has
actually begun execution. You should therefore \b never try waiting via h->get() until you are absolutely sure
that the op has already started.
*/
struct async_io_op
{
	async_file_io_dispatcher_base *parent;				//!< The parent dispatcher
	size_t id;											//!< A unique id for this operation
	std::shared_ptr<shared_future<std::shared_ptr<detail::async_io_handle>>> h;	//!< A future handle to the item being operated upon

    //! \constr
	async_io_op() : parent(nullptr), id(0), h(std::make_shared<shared_future<std::shared_ptr<detail::async_io_handle>>>()) { }
    //! \cconstr
	async_io_op(const async_io_op &o) : parent(o.parent), id(o.id), h(o.h) { }
    //! \mconstr
	async_io_op(async_io_op &&o) BOOST_NOEXCEPT_OR_NOTHROW : parent(std::move(o.parent)), id(std::move(o.id)), h(std::move(o.h)) { }
    /*! Constructs an instance.
    \param _parent The dispatcher this op belongs to.
    \param _id The unique non-zero id of this op.
    \param _handle A shared_ptr to shared state between all instances of this reference.
    */
	async_io_op(async_file_io_dispatcher_base *_parent, size_t _id, std::shared_ptr<shared_future<std::shared_ptr<detail::async_io_handle>>> _handle) : parent(_parent), id(_id), h(std::move(_handle)) { _validate(); }
    /*! Constructs an instance.
    \param _parent The dispatcher this op belongs to.
    \param _id The unique non-zero id of this op.
    */
	async_io_op(async_file_io_dispatcher_base *_parent, size_t _id) : parent(_parent), id(_id), h(std::make_shared<shared_future<std::shared_ptr<detail::async_io_handle>>>()) { }
    //! \cassign
	async_io_op &operator=(const async_io_op &o) { parent=o.parent; id=o.id; h=o.h; return *this; }
    //! \massign
	async_io_op &operator=(async_io_op &&o) BOOST_NOEXCEPT_OR_NOTHROW { parent=std::move(o.parent); id=std::move(o.id); h=std::move(o.h); return *this; }
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
			BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
#endif
	}
};

namespace detail
{
	struct when_all_count_completed_state
	{
		std::vector<async_io_op> inputs;
		atomic<size_t> togo;
		std::vector<std::shared_ptr<detail::async_io_handle>> out;
		promise<std::vector<std::shared_ptr<detail::async_io_handle>>> done;
		when_all_count_completed_state(std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last) : inputs(first, last), togo(inputs.size()), out(inputs.size()) { }
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
			// If the last completion is throwing an exception, throw that, else scan inputs for exception states
			exception_ptr this_e(afio::make_exception_ptr(afio::current_exception()));
			if(this_e)
				state->done.set_exception(this_e);
			else
			{
				bool done=false;
				try
				{
					// Retrieve the result of each input, waiting for it if it is between decrementing the
					// atomic and signalling its future.
					BOOST_FOREACH(auto &i, state->inputs)
					{
						shared_future<std::shared_ptr<detail::async_io_handle>> *future=i.h.get();
						future->get();
					}
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
	auto state(std::make_shared<detail::when_all_count_completed_state>(first, last));
	std::vector<async_io_op> &inputs=state->inputs;
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
	callbacks.reserve(inputs.size());
	size_t idx=0;
	BOOST_FOREACH(auto &i, inputs)
    {	
        callbacks.push_back(std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&detail::when_all_count_completed_nothrow, std::placeholders::_1, std::placeholders::_2, state, idx++)));
    }
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
	auto state(std::make_shared<detail::when_all_count_completed_state>(first, last));
	std::vector<async_io_op> &inputs=state->inputs;
	std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
	callbacks.reserve(inputs.size());
	size_t idx=0;
	BOOST_FOREACH(auto &i, inputs)
    {	
        callbacks.push_back(std::make_pair(async_op_flags::ImmediateCompletion, std::bind(&detail::when_all_count_completed, std::placeholders::_1, std::placeholders::_2, state, idx++)));
    }
	inputs.front().parent->completion(inputs, callbacks);
	return state->done.get_future();
}
#ifndef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
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
	BOOST_FOREACH(auto &&i, _ops)
    {	ops.push_back(std::move(i));}
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
	BOOST_FOREACH(auto &&i, _ops)
    {	ops.push_back(std::move(i));}
	return when_all(ops.begin(), ops.end());
}
#endif
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
	async_path_op_req(std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags) { if(!path.is_absolute()) BOOST_AFIO_THROW(std::runtime_error("Non-absolute path")); }
	/*! \brief Constructs an instance.
    
    Note that this will fail if path is not absolute.
    
    \param _precondition The precondition for this operation.
    \param _path The filing system path to be used. Must be absolute.
    \param _flags The flags to be used.
    */
	async_path_op_req(async_io_op _precondition, std::filesystem::path _path, file_flags _flags=file_flags::None) : path(_path), flags(_flags), precondition(std::move(_precondition)) { _validate(); if(!path.is_absolute()) BOOST_AFIO_THROW(std::runtime_error("Non-absolute path")); }
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
			BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
#endif
	}
};

namespace detail
{
	template<bool for_writing> class async_data_op_req_impl;
	template<bool for_writing> struct async_data_op_req_impl_type_selector
	{
		typedef boost::asio::const_buffer boost_asio_buffer_type;
		typedef const void *void_type;
		typedef async_data_op_req_impl<false> conversion_type;
	};
	template<> struct async_data_op_req_impl_type_selector<false>
	{
		typedef boost::asio::mutable_buffer boost_asio_buffer_type;
		typedef void *void_type;
		typedef std::nullptr_t conversion_type;
	};
	//! \brief The implementation of all async_data_op_req specialisations. \tparam for_writing Whether this implementation is for writing data. \ingroup async_data_op_req
	template<bool for_writing> class async_data_op_req_impl
	{
		typedef typename async_data_op_req_impl_type_selector<for_writing>::boost_asio_buffer_type boost_asio_buffer_type;
		typedef typename async_data_op_req_impl_type_selector<for_writing>::void_type void_type;
		typedef typename async_data_op_req_impl_type_selector<for_writing>::conversion_type conversion_type;
	public:
		//! An optional precondition for this operation
		async_io_op precondition;
		//! A sequence of mutable Boost.ASIO buffers to read into
		std::vector<boost_asio_buffer_type> buffers;
		//! The offset from which to read
		off_t where;
		//! \constr
		async_data_op_req_impl() { }
		//! \cconstr
		async_data_op_req_impl(const async_data_op_req_impl &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
		//! \mconstr
		async_data_op_req_impl(async_data_op_req_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
		//! \cconstr
		async_data_op_req_impl(const conversion_type &o) : precondition(o.precondition), where(o.where) { buffers.reserve(o.buffers.capacity()); BOOST_FOREACH(auto &i, o.buffers){ buffers.push_back(i); } }
		//! \mconstr
		async_data_op_req_impl(conversion_type &&o) BOOST_NOEXCEPT_OR_NOTHROW : precondition(std::move(o.precondition)), where(std::move(o.where)) { buffers.reserve(o.buffers.capacity()); BOOST_FOREACH(auto &&i, o.buffers){ buffers.push_back(std::move(i)); } }
		//! \cassign
		async_data_op_req_impl &operator=(const async_data_op_req_impl &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
		//! \massign
		async_data_op_req_impl &operator=(async_data_op_req_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
		//! \async_data_op_req1 \param _length The number of bytes to transfer
		async_data_op_req_impl(async_io_op _precondition, void_type v, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost_asio_buffer_type(v, _length)); _validate(); }
		//! \async_data_op_req2 \tparam "class T" boost_asio_buffer_type
		template<class T> async_data_op_req_impl(async_io_op _precondition, std::vector<T> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(std::move(_buffers)), where(_where) { _validate(); }
		//! Validates contents for correctness \return True if contents are correct
		bool validate() const
		{
			if(!precondition.validate()) return false;
			if(buffers.empty()) return false;
			BOOST_FOREACH(auto &b, buffers)
			{
				if(!boost::asio::buffer_cast<const void *>(b) || !boost::asio::buffer_size(b)) return false;
				if(!!(precondition.parent->fileflags(file_flags::None)&file_flags::OSDirect))
				{
					if(((size_t) boost::asio::buffer_cast<const void *>(b) & 4095) || (boost::asio::buffer_size(b) & 4095)) return false;
				}
			}
			return true;
		}
	private:
		void _validate() const
		{
#if BOOST_AFIO_VALIDATE_INPUTS
			if(!validate())
				BOOST_AFIO_THROW(std::runtime_error("Inputs are invalid."));
#endif
		}
	};
}

/*! \struct async_data_op_req
\brief A convenience bundle of precondition, data and where for reading into a single `T *`. Data \b MUST stay around until the operation completes.
\tparam T Any readable type T
\ingroup async_data_op_req
*/
template<class T> struct async_data_op_req : public detail::async_data_op_req_impl<false>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of mutable Boost.ASIO buffers to read into
	std::vector<boost::asio::mutable_buffer> buffers;
	//! The offset from which to read
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
	//! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<false>(o) { }
	//! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<false>(std::move(o)) { }
	//! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<false>>(*this)=o; return *this; }
	//! \massign
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<false>>(*this)=std::move(o); return *this; }
	//! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, T *v, size_t _length, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), static_cast<void *>(v), _length, _where) { }
	};
	//! \brief A convenience bundle of precondition, data and where for writing from a single `const T *`. Data \b MUST stay around until the operation completes. \tparam "class T" Any readable type T \ingroup async_data_op_req
	template<class T> struct async_data_op_req<const T> : public detail::async_data_op_req_impl<true>
	{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
		//! A precondition containing an open file handle for this operation
		async_io_op precondition;
		//! A sequence of const Boost.ASIO buffers to write from
		std::vector<boost::asio::const_buffer> buffers;
		//! The offset at which to write
		off_t where;
#endif
		//! \constr
		async_data_op_req() { }
		//! \cconstr
		async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<true>(o) { }
		//! \mconstr
		async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
		//! \cconstr
		async_data_op_req(const async_data_op_req<T> &o) : detail::async_data_op_req_impl<true>(o) { }
		//! \mconstr
		async_data_op_req(async_data_op_req<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
		//! \cassign
		async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
		//! \massign
		async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
		//! \async_data_op_req1 \param _length The number of bytes to transfer
		async_data_op_req(async_io_op _precondition, const T *v, size_t _length, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), static_cast<const void *>(v), _length, _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `void *`. Data \b MUST stay around until the operation completes. \ingroup async_data_op_req
template<> struct async_data_op_req<void> : public detail::async_data_op_req_impl<false>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of mutable Boost.ASIO buffers to read into
	std::vector<boost::asio::mutable_buffer> buffers;
	//! The offset from which to read
	off_t where;
#endif
	//! \constr
	async_data_op_req() : detail::async_data_op_req_impl<false>() {}
	//! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<false>(o) {}
	//! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<false>(std::move(o)) {}
	//! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, void *v, size_t _length, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), v, _length, _where) { }
	//! \async_data_op_req2
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::mutable_buffer> _buffers, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), std::move(_buffers), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const void *`. Data \b MUST stay around until the operation completes. \ingroup async_data_op_req
template<> struct async_data_op_req<const void> : public detail::async_data_op_req_impl<true>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of const Boost.ASIO buffers to write from
	std::vector<boost::asio::const_buffer> buffers;
	//! The offset at which to write
	off_t where;
#endif
	//! \constr
	async_data_op_req() : detail::async_data_op_req_impl<true>() {}
	//! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<true>(o) {}
	//! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) {}
	//! \cconstr
	async_data_op_req(const async_data_op_req<void> &o) : detail::async_data_op_req_impl<true>(o) {}
	//! \mconstr
	async_data_op_req(async_data_op_req<void> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) {}
	//! \async_data_op_req1 \param _length The number of bytes to transfer
	async_data_op_req(async_io_op _precondition, const void *v, size_t _length, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), v, _length, _where) {}
	//! \async_data_op_req2
	async_data_op_req(async_io_op _precondition, std::vector<boost::asio::const_buffer> _buffers, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), _buffers, _where) {}
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::vector<T, A>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class T, class A> struct async_data_op_req<std::vector<T, A>> : public detail::async_data_op_req_impl<false>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of mutable Boost.ASIO buffers to read into
	std::vector<boost::asio::mutable_buffer> buffers;
	//! The offset from which to read
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<false>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<false>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<false>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<false>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, std::vector<T, A> &v, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const std::vector<T, A>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class T, class A> struct async_data_op_req<const std::vector<T, A>> : public detail::async_data_op_req_impl<true>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of const Boost.ASIO buffers to write from
	std::vector<boost::asio::const_buffer> buffers;
	//! The offset at which to write
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cconstr
	async_data_op_req(const async_data_op_req<std::vector<T, A>> &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<std::vector<T, A>> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, const std::vector<T, A> &v, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::array<T, N>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam N Any compile-time size \ingroup async_data_op_req
template<class T, size_t N> struct async_data_op_req<std::array<T, N>> : public detail::async_data_op_req_impl<false>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of mutable Boost.ASIO buffers to read into
	std::vector<boost::asio::mutable_buffer> buffers;
	//! The offset from which to read
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<false>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<false>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<false>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<false>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, std::array<T, N> &v, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const std::array<T, N>`. Data \b MUST stay around until the operation completes. \tparam "class T" Any type \tparam N Any compile-time size \ingroup async_data_op_req
template<class T, size_t N> struct async_data_op_req<const std::array<T, N>> : public detail::async_data_op_req_impl<true>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of const Boost.ASIO buffers to write from
	std::vector<boost::asio::const_buffer> buffers;
	//! The offset at which to write
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
    //! \massign
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
    //! \cconstr
	async_data_op_req(const async_data_op_req<std::array<T, N>> &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<std::array<T, N>> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, const std::array<T, N> &v, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(T), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::basic_string<C, T, A>`. Data \b MUST stay around until the operation completes. \tparam "class C" Any character type \tparam "class T" Character traits type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class C, class T, class A> struct async_data_op_req<std::basic_string<C, T, A>> : public detail::async_data_op_req_impl<false>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of mutable Boost.ASIO buffers to read into
	std::vector<boost::asio::mutable_buffer> buffers;
	//! The offset from which to read
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<false>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<false>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<false>>(*this)=o; return *this; }
    //! \mconstr
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<false>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, std::basic_string<C, T, A> &v, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), static_cast<void *>(&v.front()), v.size()*sizeof(A), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `const std::basic_string<C, T, A>`. Data \b MUST stay around until the operation completes.  \tparam "class C" Any character type \tparam "class T" Character traits type \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class C, class T, class A> struct async_data_op_req<const std::basic_string<C, T, A>> : public detail::async_data_op_req_impl<true>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
	//! A precondition containing an open file handle for this operation
	async_io_op precondition;
	//! A sequence of const Boost.ASIO buffers to write from
	std::vector<boost::asio::const_buffer> buffers;
	//! The offset at which to write
	off_t where;
#endif
	//! \constr
	async_data_op_req() { }
    //! \cconstr
	async_data_op_req(const async_data_op_req &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cconstr
	async_data_op_req(const async_data_op_req<std::basic_string<C, T, A>> &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
	async_data_op_req(async_data_op_req<std::basic_string<C, T, A>> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cassign
	async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
    //! \mconstr
	async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
	async_data_op_req(async_io_op _precondition, const std::basic_string<C, T, A> &v, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), static_cast<const void *>(&v.front()), v.size()*sizeof(A), _where) { }
};

namespace detail {
	template<bool iswrite, class T> struct async_file_io_dispatcher_rwconverter
	{
		typedef detail::async_data_op_req_impl<iswrite> return_type;
		const std::vector<return_type> &operator()(const std::vector<async_data_op_req<T>> &ops)
		{
			typedef async_data_op_req<T> reqT;
			static_assert(std::is_convertible<reqT, return_type>::value, "async_data_op_req<T> is not convertible to detail::async_data_op_req_impl<constness>");
			static_assert(sizeof(return_type)==sizeof(reqT), "async_data_op_req<T> does not have the same size as detail::async_data_op_req_impl<constness>");
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
namespace detail {
	template<class tasktype> std::pair<bool, std::shared_ptr<detail::async_io_handle>> doCall(size_t, std::shared_ptr<detail::async_io_handle> _, exception_ptr *, std::shared_ptr<tasktype> c)
	{
		(*c)();
		return std::make_pair(true, _);
	}
}
template<class R> inline std::pair<std::vector<future<R>>, std::vector<async_io_op>> async_file_io_dispatcher_base::call(const std::vector<async_io_op> &ops, const std::vector<std::function<R()>> &callables)
{
	typedef packaged_task<R()> tasktype;
	std::vector<future<R>> retfutures;
	std::vector<std::pair<async_op_flags, std::function<completion_t>>> callbacks;
	retfutures.reserve(callables.size());
	callbacks.reserve(callables.size());
    
	BOOST_FOREACH(auto &t, callables)
	{
		std::shared_ptr<tasktype> c(std::make_shared<tasktype>(std::function<R()>(t)));
		retfutures.push_back(c->get_future());
		callbacks.push_back(std::make_pair(async_op_flags::None, std::bind(&detail::doCall<tasktype>, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::move(c))));
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

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES

template<class C, class... Args> inline std::pair<future<typename boost::result_of<C(Args...)>::type>, async_io_op> async_file_io_dispatcher_base::call(const async_io_op &req, C callback, Args... args)
{
	typedef typename std::result_of<C(Args...)>::type rettype;
	return call(req, std::bind<rettype()>(callback, args...));
}

#else

#define BOOST_PP_LOCAL_MACRO(N)                                                                                         \
    template <class C                                                                                                   \
    BOOST_PP_COMMA_IF(N)                                                                                                \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                                                   \
    inline std::pair<future<typename boost::result_of<C(BOOST_PP_ENUM_PARAMS(N, A))>::type>, async_io_op>               \
    call (const async_io_op &req, C callback BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))                 \
    {                                                                                                                   \
        typedef typename std::result_of<C(BOOST_PP_ENUM_PARAMS(N, A))>::type rettype;                                   \
    	return call(req, std::bind<rettype()>(callback, BOOST_PP_ENUM_PARAMS(N, a)));                                   \
    }
  
#define BOOST_PP_LOCAL_LIMITS     (1, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()


#endif

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
#ifndef DOXYGEN_SHOULD_SKIP_THIS
inline async_io_op async_file_io_dispatcher_base::read(const detail::async_data_op_req_impl<false> &req)
{
	std::vector<detail::async_data_op_req_impl<false>> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(read(i).front());
}
inline async_io_op async_file_io_dispatcher_base::write(const detail::async_data_op_req_impl<true> &req)
{
	std::vector<detail::async_data_op_req_impl<true>> i;
	i.reserve(1);
	i.push_back(req);
	return std::move(write(i).front());
}
#endif
template<class T> inline std::vector<async_io_op> async_file_io_dispatcher_base::read(const std::vector<async_data_op_req<T>> &ops)
{
	return read(detail::async_file_io_dispatcher_rwconverter<false, T>()(ops));
}
template<class T> inline std::vector<async_io_op> async_file_io_dispatcher_base::write(const std::vector<async_data_op_req<T>> &ops)
{
	return write(detail::async_file_io_dispatcher_rwconverter<true, T>()(ops));
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
