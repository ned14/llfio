/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013-2014 Niall Douglas http://www.nedprod.com/
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
// VS2010 needs D_VARIADIC_MAX set to at least seven
#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 && (!defined(_VARIADIC_MAX) || _VARIADIC_MAX < 7)
#error _VARIADIC_MAX needs to be set to at least seven to compile Boost.AFIO
#endif

#include "config.hpp"
#include "boost/asio.hpp"
#include "boost/foreach.hpp"
#include "detail/Preprocessor_variadic.hpp"
#include "boost/detail/scoped_enum_emulation.hpp"
#include "detail/Utility.hpp"
#include <type_traits>
#include <boost/functional/hash.hpp>
#ifndef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
#include <initializer_list>
#endif
#include <exception>
#include <algorithm> // Boost.ASIO needs std::min and std::max

#if BOOST_VERSION<105500
#error Boosts before v1.55 are missing boost::future<>::get_exception_ptr() critical for good AFIO performance. Get a newer Boost!
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


namespace detail
{
    template<class R> class enqueued_task_impl
    {
    protected:
        struct Private
        {
            std::function<R()> task;
            promise<R> r;
            bool autoset;
            atomic<int> done;
            Private(std::function<R()> _task) : task(std::move(_task)), autoset(true), done(0) { }
        };
        std::shared_ptr<Private> p;
    public:
        //! Default constructor
        enqueued_task_impl() { }
        //! Constructs an enqueued task calling \em c
        enqueued_task_impl(std::function<R()> c) : p(std::make_shared<Private>(std::move(c))) { }
        //! Returns true if valid
        bool valid() const BOOST_NOEXCEPT_OR_NOTHROW{ return p.get()!=nullptr; }
            //! Swaps contents with another instance
        void swap(enqueued_task_impl &o) BOOST_NOEXCEPT_OR_NOTHROW{ p.swap(o.p); }
            //! Resets the contents
        void reset() { p.reset(); }
        //! Returns the future corresponding to the future return value of the task
        future<R> get_future() { return p->r.get_future(); }
        //! Sets the future corresponding to the future return value of the task.
        void set_future_exception(exception_ptr e)
        {
            int _=0;
            if(!p->done.compare_exchange_strong(_, 1))
                return;
            p->r.set_exception(e);
        }
        //! Disables the task setting the future return value.
        void disable_auto_set_future(bool v=true) { p->autoset=!v; }
    };
}

template<class R> class enqueued_task;
/*! \class enqueued_task<R()>
\brief Effectively our own custom std::packaged_task<>, with copy semantics and letting us early set value to significantly improve performance

Unlike `std::packaged_task<>`, this custom variant is copyable though each copy always refers to the same
internal state. Early future value setting is possible, with any subsequent value setting including that
by the function being executed being ignored. Note that this behaviour opens the potential to lose exception
state - if you set the future value early and then an exception is later thrown, the exception is swallowed.
*/
// Can't have args in callable type as that segfaults VS2010
template<class R> class enqueued_task<R()> : public detail::enqueued_task_impl<R>
{
    typedef detail::enqueued_task_impl<R> Base;
public:
    //! Default constructor
    enqueued_task() { }
    //! Constructs an enqueued task calling \em c
    enqueued_task(std::function<R()> c) : Base(std::move(c)) { }
    //! Sets the future corresponding to the future return value of the task.
    template<class T> void set_future_value(T v)
    {
        int _=0;
        if(!Base::p->done.compare_exchange_strong(_, 1))
            return;
        Base::p->r.set_value(v);
    }
    //! Invokes the callable, setting the future to the value it returns
    void operator()()
    {
        try
        {
            auto v(Base::p->task());
            if(Base::p->autoset) set_future_value(v);
        }
        catch(...)
        {
            auto e(afio::make_exception_ptr(afio::current_exception()));
            if(Base::p->autoset) Base::set_future_exception(e);
        }
    }
};
template<> class enqueued_task<void()> : public detail::enqueued_task_impl<void>
{
    typedef detail::enqueued_task_impl<void> Base;
public:
    //! Default constructor
    enqueued_task() { }
    //! Constructs an enqueued task calling \em c
    enqueued_task(std::function<void()> c) : Base(std::move(c)) { }
    //! Sets the future corresponding to the future return value of the task.
    void set_future_value()
    {
        int _=0;
        if(!Base::p->done.compare_exchange_strong(_, 1))
            return;
        Base::p->r.set_value();
    }
    //! Invokes the callable, setting the future to the value it returns
    void operator()()
    {
        try
        {
            Base::p->task();
            if(Base::p->autoset) set_future_value();
        }
        catch(...)
        {
            auto e(afio::make_exception_ptr(afio::current_exception()));
            if(Base::p->autoset) Base::set_future_exception(e);
        }
    }
};
/*! \class thread_source
\brief Abstract base class for a source of thread workers

Note that in Boost 1.54, and possibly later versions, `boost::asio::io_service` on Windows appears to dislike being
destructed during static data deinit, hence why this inherits from `std::enable_shared_from_this<>` in order that it
may be reference count deleted before static data deinit occurs.
*/
class thread_source : public std::enable_shared_from_this<thread_source>
{
protected:
    boost::asio::io_service &service;
    thread_source(boost::asio::io_service &_service) : service(_service) { }
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~thread_source() { }
public:
    //! Returns the underlying io_service
    boost::asio::io_service &io_service() { return service; }
    //! Sends some callable entity to the thread pool for execution \tparam "class F" Any callable type with signature R(void) \param out An enqueued task for the enqueued callable \param f Any instance of a callable type \param autosetfuture Whether the enqueued_task will set its future on return of the callable
    template<class F> void enqueue(enqueued_task<typename std::result_of<F()>::type()> &out, F f, bool autosetfuture=true)
    {
        typedef typename std::result_of<F()>::type R;
        out=std::move(enqueued_task<R()>(std::move(f)));
        out.disable_auto_set_future(!autosetfuture);
        service.post(out);
    }
    //! Sends some callable entity to the thread pool for execution \return An enqueued task for the enqueued callable \tparam "class F" Any callable type with signature R(void) \param f Any instance of a callable type
    template<class F> future<typename std::result_of<F()>::type> enqueue(F f)
    {
        typedef typename std::result_of<F()>::type R;
        enqueued_task<R()> out(std::move(f));
        auto ret(out.get_future());
        service.post(out);
        return std::move(ret);
    }
};

/*! \class std_thread_pool
\brief A very simple thread pool based on std::thread or boost::thread

This instantiates a `boost::asio::io_service` and a latchable `boost::asio::io_service::work` to keep any threads working until the instance is destructed.
*/
class std_thread_pool : public thread_source {
    class worker
    {
        std_thread_pool *pool;
    public:
        explicit worker(std_thread_pool *p) : pool(p) { }
        void operator()()
        {
            detail::set_threadname("boost::afio::std_thread_pool worker");
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
            // VS2010 segfaults if you ever do a try { throw; } catch(...) without an exception ever having been thrown :(
            try
            {
                throw detail::vs2010_lack_of_decent_current_exception_support_hack_t();
            }
            catch(...)
            {
                detail::vs2010_lack_of_decent_current_exception_support_hack()=boost::current_exception();
#endif
                try
                {
                    pool->service.run();
                }
                catch(...)
                {
                    std::cerr << "WARNING: ASIO exits via exception which shouldn't happen. Exception was " << boost::current_exception_diagnostic_information(true) << std::endl;
                }
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
            }
#endif
        }
    };
    friend class worker;

    boost::asio::io_service service;
    std::unique_ptr<boost::asio::io_service::work> working;
    std::vector< std::unique_ptr<thread> > workers;
public:
    /*! \brief Constructs a thread pool of \em no workers
    \param no The number of worker threads to create
    */
    explicit std_thread_pool(size_t no) : thread_source(service), working(make_unique<boost::asio::io_service::work>(service))
    {
        add_workers(no);
    }
    //! Adds more workers to the thread pool \param no The number of worker threads to add
    void add_workers(size_t no)
    {
        workers.reserve(workers.size()+no);
        for(size_t n=0; n<no; n++)
            workers.push_back(make_unique<thread>(worker(this)));
    }
    //! Destroys the thread pool, waiting for worker threads to exit beforehand.
    void destroy()
    {
        if(!service.stopped())
        {
            // Tell the threads there is no more work to do
            working.reset();
            BOOST_FOREACH(auto &i, workers) { i->join(); }
            workers.clear();
            // For some reason ASIO occasionally thinks there is still more work to do
            if(!service.stopped())
                service.run();
            service.stop();
            service.reset();
        }
    }
    ~std_thread_pool()
    {
        destroy();
    }
};
/*! \brief Returns the process threadpool

On first use, this instantiates a default std_thread_pool running `BOOST_AFIO_MAX_NON_ASYNC_QUEUE_DEPTH` threads which will remain until its shared count reaches zero.
\ingroup process_threadpool
*/
BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC std::shared_ptr<std_thread_pool> process_threadpool();

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
    return process_threadpool()->enqueue(std::move(waitforall));
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
    return process_threadpool()->enqueue(std::move(waitforall));
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
struct async_enumerate_op_req;

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
    None=0,             //!< No flags set
    Read=1,             //!< Read access
    Write=2,            //!< Write access
    ReadWrite=3,        //!< Read and write access
    Append=4,           //!< Append only
    Truncate=8,         //!< Truncate existing file to zero
    Create=16,          //!< Open and create if doesn't exist
    CreateOnlyIfNotExist=32, //!< Create and open only if doesn't exist

    WillBeSequentiallyAccessed=128, //!< Will be exclusively either read or written sequentially. If you're exclusively writing sequentially, \em strongly consider turning on OSDirect too.
    WillBeRandomlyAccessed=256, //!< Will be randomly accessed, so don't bother with read-ahead. If you're using this, \em strongly consider turning on OSDirect too.

    FastDirectoryEnumeration=(1<<10), //!< Hold a file handle open to the containing directory of each open file for fast directory enumeration.
    UniqueDirectoryHandle=(1<<11), //!< Return a unique directory handle rather than a shared directory handle

    OSDirect=(1<<16),   //!< Bypass the OS file buffers (only really useful for writing large files, or a lot of random reads and writes. Note you must 4Kb align everything if this is on)
    OSMMap=(1<<17),     //!< Memory map files (for reads only).

    AlwaysSync=(1<<24),     //!< Ask the OS to not complete until the data is on the physical storage. Best used only with OSDirect, otherwise use SyncOnClose.
    SyncOnClose=(1<<25),    //!< Automatically initiate an asynchronous flush just before file close, and fuse both operations so both must complete for close to complete.
    EnforceDependencyWriteOrder=(1<<26), //!< Ensure that data writes to files reach physical storage in the same order as the op dependencies close files. Does NOT enforce ordering of individual data writes, ONLY all file writes accumulated before a file close.

    int_opening_link=(1<<29), //!< Internal use only. Don't use.
    int_opening_dir=(1<<30) //!< Internal use only. Don't use.
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
    none=0,                 //!< No flags set
    immediate=1             //!< Call chained completion immediately instead of scheduling for later. Make SURE your completion can not block!
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
    struct immediate_async_ops;
    template<bool for_writing> class async_data_op_req_impl;
}

class async_io_handle;

/*! \enum metadata_flags
\brief Bitflags for availability of metadata from `struct stat_t`
\ingroup metadata_flags
*/
#ifdef DOXYGEN_NO_CLASS_ENUMS
enum metadata_flags
#elif defined(BOOST_NO_CXX11_SCOPED_ENUMS)
BOOST_SCOPED_ENUM_UT_DECLARE_BEGIN(metadata_flags, size_t)
#else
enum class metadata_flags : size_t
#endif
{
    None=0,
    dev=1<<0,
    ino=1<<1,
    type=1<<2,
    perms=1<<3,
    nlink=1<<4,
    uid=1<<5,
    gid=1<<6,
    rdev=1<<7,
    atim=1<<8,
    mtim=1<<9,
    ctim=1<<10,
    size=1<<11,
    allocated=1<<12,
    blocks=1<<13,
    blksize=1<<14,
    flags=1<<15,
    gen=1<<16,
    birthtim=1<<17,
    All=(size_t)-1       //!< Return the maximum possible metadata.
}
#ifdef BOOST_NO_CXX11_SCOPED_ENUMS
BOOST_SCOPED_ENUM_DECLARE_END(metadata_flags)
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(metadata_flags::enum_type)
#else
;
BOOST_AFIO_DECLARE_CLASS_ENUM_AS_BITFIELD(metadata_flags)
#endif
/*! \struct stat_t
\brief Metadata about a directory entry

This structure looks somewhat like a `struct stat`, and indeed it was derived from BSD's `struct stat`.
However there are a number of changes to better interoperate with modern practice, specifically:
(i) inode value containers are forced to 64 bits.
(ii) Timestamps use C++11's `std::chrono::system_clock::time_point` or Boost equivalent. The resolution
of these may or may not equal what a `struct timespec` can do depending on your STL.
(iii) The type of a file, which is available on Windows and on POSIX without needing an additional
syscall, is provided by `st_type` which is one of the values from `std::filesystem::file_type`.
(iv) As type is now separate from permissions, there is no longer a `st_mode`, instead being a
`st_perms` which is solely the permissions bits. If you want to test permission bits in `st_perms`
but don't want to include platform specific headers, note that `std::filesystem::perms` contains
definitions of the POSIX permissions flags.
*/
struct stat_t
{
#ifndef WIN32
    uint64_t        st_dev;                       /*!< inode of device containing file (POSIX only) */
#endif
    uint64_t        st_ino;                       /*!< inode of file                   (Windows, POSIX) */
    std::filesystem::file_type st_type;           /*!< type of file                    (Windows, POSIX) */
#ifndef WIN32
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    uint16_t        st_perms;
#else
    std::filesystem::perms st_perms;              /*!< bitfield perms of file          (POSIX only) */
#endif
#endif
    int16_t         st_nlink;                     /*!< number of hard links            (Windows, POSIX) */
#ifndef WIN32
    int16_t         st_uid;                       /*!< user ID of the file             (POSIX only) */
    int16_t         st_gid;                       /*!< group ID of the file            (POSIX only) */
    dev_t           st_rdev;                      /*!< id of file if special           (POSIX only) */
#endif
    chrono::system_clock::time_point st_atim;     /*!< time of last access             (Windows, POSIX) */
    chrono::system_clock::time_point st_mtim;     /*!< time of last data modification  (Windows, POSIX) */
    chrono::system_clock::time_point st_ctim;     /*!< time of last status change      (Windows, POSIX) */
    off_t           st_size;                      /*!< file size, in bytes             (Windows, POSIX) */
    off_t           st_allocated;                 /*!< bytes allocated for file        (Windows, POSIX) */
    off_t           st_blocks;                    /*!< number of blocks allocated      (Windows, POSIX) */
    uint16_t        st_blksize;                   /*!< block size used by this device  (Windows, POSIX) */
    uint32_t        st_flags;                     /*!< user defined flags for file     (FreeBSD, OS X, zero otherwise) */
    uint32_t        st_gen;                       /*!< file generation number          (FreeBSD, OS X, zero otherwise)*/
    chrono::system_clock::time_point st_birthtim; /*!< time of file creation           (Windows, FreeBSD, OS X, zero otherwise) */

    //! Constructs a UNINITIALIZED instance i.e. full of random garbage
    stat_t() { }
    //! Constructs a zeroed instance
    stat_t(std::nullptr_t) :
#ifndef WIN32
        st_dev(0),
#endif
        st_ino(0),
        st_type(std::filesystem::file_type::type_unknown),
#ifndef WIN32
        st_perms(0),
#endif
        st_nlink(0),
#ifndef WIN32
        st_uid(0), st_gid(0), st_rdev(0),
#endif
        st_size(0), st_allocated(0), st_blocks(0), st_blksize(0), st_flags(0), st_gen(0) { }
};

/*! \brief The abstract base class for an entry in a directory with lazily filled metadata.

Note that `directory_entry_hash` will hash one of these for you, and a `std::hash<directory_entry>` specialisation
is defined for you so you ought to be able to use directory_entry directly in an `unordered_map<>`.
*/
class BOOST_AFIO_DECL directory_entry
{
    friend class detail::async_file_io_dispatcher_compat;
    friend class detail::async_file_io_dispatcher_windows;
    friend class detail::async_file_io_dispatcher_linux;
    friend class detail::async_file_io_dispatcher_qnx;

    std::filesystem::path leafname;
    stat_t stat;
    metadata_flags have_metadata;
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void _int_fetch(metadata_flags wanted, std::shared_ptr<async_io_handle> dirh);
public:
    //! \constr
    directory_entry() : stat(nullptr), have_metadata(metadata_flags::None) { }
    //! \constr
    directory_entry(std::filesystem::path _leafname, stat_t __stat, metadata_flags _have_metadata) : leafname(_leafname), stat(__stat), have_metadata(_have_metadata) { }
#ifndef BOOST_NO_CXX11_DEFAULTED_FUNCTIONS
    directory_entry(const directory_entry &) = default;
    directory_entry &operator=(const directory_entry &) = default;
#endif
    directory_entry(directory_entry &&o) : leafname(std::move(o.leafname)), stat(std::move(o.stat)), have_metadata(std::move(o.have_metadata)) { }
    directory_entry &operator=(directory_entry &&o)
    {
        leafname=std::move(o.leafname);
        stat=std::move(o.stat);
        have_metadata=std::move(o.have_metadata);
        return *this;
    }

    bool operator==(const directory_entry& rhs) const BOOST_NOEXCEPT_OR_NOTHROW { return leafname == rhs.leafname; }
    bool operator!=(const directory_entry& rhs) const BOOST_NOEXCEPT_OR_NOTHROW { return leafname != rhs.leafname; }
    bool operator< (const directory_entry& rhs) const BOOST_NOEXCEPT_OR_NOTHROW { return leafname < rhs.leafname; }
    bool operator<=(const directory_entry& rhs) const BOOST_NOEXCEPT_OR_NOTHROW { return leafname <= rhs.leafname; }
    bool operator> (const directory_entry& rhs) const BOOST_NOEXCEPT_OR_NOTHROW { return leafname > rhs.leafname; }
    bool operator>=(const directory_entry& rhs) const BOOST_NOEXCEPT_OR_NOTHROW { return leafname >= rhs.leafname; }
    //! \return The name of the directory entry
    std::filesystem::path name() const BOOST_NOEXCEPT_OR_NOTHROW { return leafname; }
    //! \return A bitfield of what metadata is ready right now
    metadata_flags metadata_ready() const BOOST_NOEXCEPT_OR_NOTHROW { return have_metadata; }
    /*! \brief Fetches the specified metadata, returning that newly available. This is a blocking call if wanted metadata is not yet ready.
    \return The metadata now available in this directory entry.
    \param dirh An open handle to the entry's containing directory. You can get this from an op ref using when_all(dirop).get().front().
    \param wanted A bitfield of the metadata to fetch. This does not replace existing metadata.
    */
    metadata_flags fetch_metadata(std::shared_ptr<async_io_handle> dirh, metadata_flags wanted)
    {
        metadata_flags tofetch;
        wanted=wanted&metadata_supported();
        tofetch=wanted&~have_metadata;
        if(!!tofetch) _int_fetch(tofetch, dirh);
        return have_metadata;
    }
    /*! \brief Returns a copy of the internal `stat_t` structure. This is a blocking call if wanted metadata is not yet ready.
    \return A copy of the internal `stat_t` structure.
    \param dirh An open handle to the entry's containing directory. You can get this from an op ref using when_all(dirop).get().front().
    \param wanted A bitfield of the metadata to fetch. This does not replace existing metadata.
    */
    stat_t fetch_lstat(std::shared_ptr<async_io_handle> dirh, metadata_flags wanted=directory_entry::metadata_fastpath())
    {
        fetch_metadata(dirh, wanted);
        return stat;
    }
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#define BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(field) \
decltype(stat_t().st_##field) st_##field() const { if(!(have_metadata&metadata_flags::field)) { BOOST_AFIO_THROW(std::runtime_error("Field st_" #field " not present.")); } return stat.st_##field; } \
decltype(stat_t().st_##field) st_##field(std::shared_ptr<async_io_handle> dirh) { if(!(have_metadata&metadata_flags::field)) { _int_fetch(metadata_flags::field, dirh); } return stat.st_##field; }
#else
#define BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(field) \
decltype(stat_t().st_##field) st_##field(std::shared_ptr<async_io_handle> dirh=std::shared_ptr<async_io_handle>()) { if(!(have_metadata&metadata_flags::field)) { _int_fetch(metadata_flags::field, dirh); } return stat.st_##field; }
#endif
#ifndef WIN32
    //! Returns st_dev \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(dev)
#endif
    //! Returns st_ino \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(ino)
    //! Returns st_type \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(type)
#ifndef WIN32
    //! Returns st_perms \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(perms)
#endif
    //! Returns st_nlink \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(nlink)
#ifndef WIN32
    //! Returns st_uid \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(uid)
    //! Returns st_gid \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(gid)
    //! Returns st_rdev \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(rdev)
#endif
    //! Returns st_atim \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(atim)
    //! Returns st_mtim \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(mtim)
    //! Returns st_ctim \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(ctim)
    //! Returns st_size \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(size)
    //! Returns st_allocated \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(allocated)
    //! Returns st_blocks \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(blocks)
    //! Returns st_blksize \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(blksize)
    //! Returns st_flags \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(flags)
    //! Returns st_gen \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(gen)
    //! Returns st_birthtim \param dirh An optional open handle to the entry's containing directory if fetching missing metadata is desired (an exception is thrown otherwise). You can get this from an op ref using when_all(dirop).get().front().
    BOOST_AFIO_DIRECTORY_ENTRY_ACCESS_METHOD(birthtim)

    //! A bitfield of what metadata is available on this platform. This doesn't mean all is available for every filing system.
    static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC metadata_flags metadata_supported() BOOST_NOEXCEPT_OR_NOTHROW;
    //! A bitfield of what metadata is fast on this platform. This doesn't mean all is available for every filing system.
    static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC metadata_flags metadata_fastpath() BOOST_NOEXCEPT_OR_NOTHROW;
    //! The maximum number of entries which is "usual" to fetch at once i.e. what your libc does.
    static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t compatibility_maximum() BOOST_NOEXCEPT_OR_NOTHROW;
};

/*! \brief A hasher for directory_entry, hashing inode and birth time (if available on this platform).
*/
struct directory_entry_hash
{
public:
    size_t operator()(const directory_entry &p) const
    {
        size_t seed = (size_t) 0x9ddfea08eb382d69ULL;
        boost::hash_combine(seed, p.st_ino());
        if(!!(directory_entry::metadata_supported() & metadata_flags::birthtim))
            boost::hash_combine(seed, p.st_birthtim().time_since_epoch().count());
        return seed;
    }
};

/*! \brief The abstract base class encapsulating a platform-specific file handle
*/
class async_io_handle : public std::enable_shared_from_this<async_io_handle>
{
    friend class async_file_io_dispatcher_base;
    friend struct async_io_handle_posix;
    friend struct async_io_handle_windows;
    friend class detail::async_file_io_dispatcher_compat;
    friend class detail::async_file_io_dispatcher_windows;
    friend class detail::async_file_io_dispatcher_linux;
    friend class detail::async_file_io_dispatcher_qnx;

    async_file_io_dispatcher_base *_parent;
    std::shared_ptr<async_io_handle> dirh;
    chrono::system_clock::time_point _opened;
    std::filesystem::path _path; // guaranteed canonical
    file_flags _flags;
protected:
    boost::afio::atomic<off_t> bytesread, byteswritten, byteswrittenatlastfsync;
    async_io_handle(async_file_io_dispatcher_base *parent, std::shared_ptr<async_io_handle> _dirh, const std::filesystem::path &path, file_flags flags) : _parent(parent), dirh(std::move(_dirh)), _opened(chrono::system_clock::now()), _path(path), _flags(flags), bytesread(0), byteswritten(0), byteswrittenatlastfsync(0) { }
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void close() BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
public:
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~async_io_handle() { }
    //! Returns the parent of this io handle
    async_file_io_dispatcher_base *parent() const { return _parent; }
    //! Returns a handle to the directory containing this handle. Only works if `file_flags::FastDirectoryEnumeration` was specified when this handle was opened.
    std::shared_ptr<async_io_handle> container() const { return dirh; }
    //! Returns the native handle of this io handle. On POSIX, you can cast this to a fd using `(int)(size_t) native_handle()`. On Windows it's a simple `(HANDLE) native_handle()`.
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void *native_handle() const BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    //! Returns when this handle was opened
    const chrono::system_clock::time_point &opened() const { return _opened; }
    //! Returns the path of this io handle
    const std::filesystem::path &path() const { return _path; }
    //! Returns the final flags used when this handle was opened
    file_flags flags() const { return _flags; }
    //! True if this handle was opened as a file
    bool opened_as_file() const { return !(_flags&file_flags::int_opening_dir) && !(_flags&file_flags::int_opening_link); }
    //! True if this handle was opened as a directory
    bool opened_as_dir() const { return !!(_flags&file_flags::int_opening_dir); }
    //! True if this handle was opened as a symlink
    bool opened_as_symlink() const { return !!(_flags&file_flags::int_opening_link); }
    //! Returns how many bytes have been read since this handle was opened.
    off_t read_count() const { return bytesread; }
    //! Returns how many bytes have been written since this handle was opened.
    off_t write_count() const { return byteswritten; }
    //! Returns how many bytes have been written since this handle was last fsynced.
    off_t write_count_since_fsync() const { return byteswritten-byteswrittenatlastfsync; }
    //! Returns a mostly filled directory_entry for the file or directory referenced by this handle. Use `metadata_flags::All` if you want it as complete as your platform allows, even at the cost of severe performance loss.
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC directory_entry direntry(metadata_flags wanted=directory_entry::metadata_fastpath()) const BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    //! Returns a mostly filled stat_t structure for the file or directory referenced by this handle. Use `metadata_flags::All` if you want it as complete as your platform allows, even at the cost of severe performance loss.
    stat_t lstat(metadata_flags wanted=directory_entry::metadata_fastpath()) const
    {
        directory_entry de(direntry(wanted));
        return de.fetch_lstat(std::shared_ptr<async_io_handle>() /* actually unneeded */, wanted);
    }
    //! Returns the target path of this handle if it is a symbolic link
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::filesystem::path target() const BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    //! Tries to map the file into memory. Currently only works if handle is read-only.
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC void *try_mapfile() BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
};

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
// This is a result_of filter to work around the weird mix of brittle decltype(), SFINAE incapable
// std::result_of and variadic template overload resolution rules in VS2013. Works on other compilers
// too of course, it simply prefilters out the call() overloads not matching the variadic overload.
namespace detail
{
#if 0
    template<class C, class... Args> struct vs2013_variadic_overload_resolution_workaround;
    // Match callable
    template<class R, class... OArgs, class... Args> struct vs2013_variadic_overload_resolution_workaround<R (*)(OArgs...), Args...>
    {
        typedef typename std::result_of<R(*)(Args...)>::type type;
    };
    // Match callable
    template<class R, class T, class... OArgs, class... Args> struct vs2013_variadic_overload_resolution_workaround<R (T::*)(OArgs...) const, Args...>
    {
        typedef typename std::result_of<R (T::*)(Args...) const>::type type;
    };
    // Match callable
    template<class R, class T, class... OArgs, class... Args> struct vs2013_variadic_overload_resolution_workaround<R (T::*const)(OArgs...) const, Args...>
    {
        typedef typename std::result_of<R (T::*const)(Args...) const>::type type;
    };
#else
    /*
    call(const std::vector<async_io_op> &ops             , const std::vector<std::function<R()>> &callables              );
    call(const std::vector<std::function<R()>> &callables                                                                );
    call(const async_io_op &req                          , std::function<R()> callback                                   );
    call(const async_io_op &req                          , C callback                                      , Args... args);
    */
    template<class C, class... Args> struct vs2013_variadic_overload_resolution_workaround
    {
        typedef typename std::result_of<C(Args...)>::type type;
    };
    // Disable C being a const std::vector<std::function<R()>> &callables
    template<class T, class... Args> struct vs2013_variadic_overload_resolution_workaround<std::vector<T>, Args...>;
#endif
}
#endif

/*! \class async_file_io_dispatcher_base
\brief Abstract base class for dispatching file i/o asynchronously

This is a reference counted instance with platform-specific implementation optionally hidden in object code.
Construct an instance using the `boost::afio::make_async_file_io_dispatcher()` function.

\qbk{
[/ link afio.reference.functions.async_file_io_dispatcher `async_file_io_dispatcher()`]
[include generated/group_async_file_io_dispatcher_base__completion.qbk]
[include generated/group_async_file_io_dispatcher_base__call.qbk]
[include generated/group_async_file_io_dispatcher_base__filedirops.qbk]
[include generated/group_async_file_io_dispatcher_base__barrier.qbk]
[include generated/group_async_file_io_dispatcher_base__enumerate.qbk]
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
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_add_io_handle(void *key, std::shared_ptr<async_io_handle> h);
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void int_del_io_handle(void *key);
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_io_op int_op_from_scheduled_id(size_t id) const;
protected:
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_file_io_dispatcher_base(std::shared_ptr<thread_source> threadpool, file_flags flagsforce, file_flags flagsmask);
    std::pair<bool, std::shared_ptr<async_io_handle>> doadopt(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, std::shared_ptr<async_io_handle> h)
    {
        return std::make_pair(true, h);
    }
public:
    //! Destroys the dispatcher, blocking inefficiently if any ops are still in flight.
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC ~async_file_io_dispatcher_base();

    //! Returns the thread source used by this dispatcher
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::shared_ptr<thread_source> threadsource() const;
    //! Returns file flags as would be used after forcing and masking bits passed during construction
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC file_flags fileflags(file_flags flags) const;
    //! Returns the current wait queue depth of this dispatcher
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t wait_queue_depth() const;
    //! Returns the number of open items in this dispatcher
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t fd_count() const;
    /*! \brief Returns an op ref for a given \b currently scheduled op id, throwing an exception if id not scheduled at the point of call.
    Can be used to retrieve exception state from some op id, or one's own shared future.
    \return An async_io_op with the same shared future as all op refs with this id.
    \param id The unique integer id for the op.
    */
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_io_op op_from_scheduled_id(size_t id) const;

    //! The type returned by a completion handler \ingroup async_file_io_dispatcher_base__completion
    typedef std::pair<bool, std::shared_ptr<async_io_handle>> completion_returntype;
    //! The type of a completion handler \ingroup async_file_io_dispatcher_base__completion
    typedef completion_returntype completion_t(size_t, std::shared_ptr<async_io_handle>, exception_ptr *);
#ifndef DOXYGEN_SHOULD_SKIP_THIS
#if defined(BOOST_AFIO_ENABLE_BENCHMARKING_COMPLETION) || BOOST_AFIO_HEADERS_ONLY==0 // Only really used for benchmarking
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *>> &callbacks);
    inline async_io_op completion(const async_io_op &req, const std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *> &callback);
#endif
#endif
    /*! \brief Schedule a batch of asynchronous invocations of the specified functions when their supplied operations complete.
    \return A batch of op handles
    \param ops A batch of precondition op handles.
    \param callbacks A batch of pairs of op flags and bound completion handler functions of type `completion_t`
    \ingroup async_file_io_dispatcher_base__completion
    \qbk{distinguish, batch bound functions}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete.}
    \exceptionmodelstd
    \qexample{completion_example1}
    */
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> completion(const std::vector<async_io_op> &ops, const std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> &callbacks);
    /*! \brief Schedule the asynchronous invocation of the specified single function when the supplied single operation completes.
    \return An op handle
    \param req A precondition op handle
    \param callback A pair of op flag and bound completion handler function of type `completion_t`
    \ingroup async_file_io_dispatcher_base__completion
    \qbk{distinguish, single bound function}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete.}
    \exceptionmodelstd
    \qexample{completion_example1}
    */
    inline async_io_op completion(const async_io_op &req, const std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>> &callback);

    /*! \brief Schedule a batch of asynchronous invocations of the specified bound functions when their supplied preconditions complete.
    
    This is effectively a convenience wrapper for `completion()`. It creates an enqueued_task matching the `completion_t`
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

    This is effectively a convenience wrapper for `completion()`. It creates an enqueued_task matching the `completion_t`
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

    This is effectively a convenience wrapper for `completion()`. It creates an enqueued_task matching the `completion_t`
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

    
    
    
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
     
    /*! \brief Schedule an asynchronous invocation of the specified unbound callable when its supplied precondition completes.
    Note that this function essentially calls `std::bind()` on the callable and the args and passes it to the other call() overload taking a `std::function<>`.
    You should therefore use `std::ref()` etc. as appropriate.

    This is effectively a convenience wrapper for `completion()`. It creates an enqueued_task matching the `completion_t`
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
#ifndef DOXYGEN_SHOULD_SKIP_THIS
    template<class C, class... Args> inline std::pair<future<typename detail::vs2013_variadic_overload_resolution_workaround<C, Args...>::type>, async_io_op> call(const async_io_op &req, C callback, Args... args);
#else
    template<class C, class... Args> inline std::pair<future<typename std::result_of<C(Args...)>::type>, async_io_op> call(const async_io_op &req, C callback, Args... args);
#endif

#else   //define a version compatable with c++03

#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class C                                                                               \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                               \
    inline std::pair<future<typename std::result_of<C(BOOST_PP_ENUM_PARAMS(N, A))>::type>, async_io_op>  \
    call (const async_io_op &req, C callback BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_BINARY_PARAMS(N, A, a));     

  
#define BOOST_PP_LOCAL_LIMITS     (1, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()

#endif





    /*! \brief Schedule a batch of third party handle adoptions.

    This function enables you to adopt third party custom async_io_handle derivatives
    as ops into the scheduler. Think of it as if you were calling file(), except the
    op returns the supplied handle and otherwise does nothing.

    \return A batch of op handles.
    \param hs A batch of handles to adopt.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete.}
    \exceptionmodelstd
    \qexample{adopt_example}
    */
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> adopt(const std::vector<std::shared_ptr<async_io_handle>> &hs);
    /*! \brief Schedule an adoption of a third party handle.

    This function enables you to adopt third party custom async_io_handle derivatives
    as ops into the scheduler. Think of it as if you were calling file(), except the
    op returns the supplied handle and otherwise does nothing.

    \return An op handle.
    \param h A handle to adopt.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if directory creation is constant time.}
    \exceptionmodelstd
    \qexample{adopt_example}
    */
    inline async_io_op adopt(std::shared_ptr<async_io_handle> h);
    /*! \brief Schedule a batch of asynchronous directory creations and opens after optional preconditions.

    Note that if there is already a handle open to the directory requested, that will be returned instead of
    a new handle unless file_flags::UniqueDirectoryHandle is specified.

    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if directory creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> dir(const std::vector<async_path_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    /*! \brief Schedule an asynchronous directory creation and open after an optional precondition.

    Note that if there is already a handle open to the directory requested, that will be returned instead of
    a new handle unless file_flags::UniqueDirectoryHandle is specified.

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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> rmdir(const std::vector<async_path_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> file(const std::vector<async_path_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> rmfile(const std::vector<async_path_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
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
    /*! \brief Schedule a batch of asynchronous symlink creations and opens after a precondition.

    Note that if creating, the target for the symlink is the precondition. On Windows directories are symlinked using a reparse
    point instead of a symlink due to the default lack of the <tt>SeCreateSymbolicLinkPrivilege</tt> for non-Administrative
    users.

    Note that currently on Windows non-directory symbolic links are not supported. If there is demand for this we may add support.

    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if symlink creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> symlink(const std::vector<async_path_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    /*! \brief Schedule an asynchronous symlink creation and open after a precondition.

    Note that if creating, the target for the symlink is the precondition. On Windows directories are symlinked using a reparse
    point instead of a symlink due to the default lack of the <tt>SeCreateSymbolicLinkPrivilege</tt> for non-Administrative
    users.

    Note that currently on Windows non-directory symbolic links are not supported. If there is demand for this we may add support.

    \return An op handle.
    \param req An `async_path_op_req` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if symlink creation is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
    inline async_io_op symlink(const async_path_op_req &req);
    /*! \brief Schedule a batch of asynchronous symlink deletions after optional preconditions.
    \return A batch of op handles.
    \param reqs A batch of `async_path_op_req` structures.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if symlink deletion is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> rmsymlink(const std::vector<async_path_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    /*! \brief Schedule an asynchronous symlink deletion after an optional precondition.
    \return An op handle.
    \param req An `async_path_op_req` structure.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if symlink deletion is constant time.}
    \exceptionmodelstd
    \qexample{filedir_example}
    */
    inline async_io_op rmsymlink(const async_path_op_req &req);
    /*! \brief Schedule a batch of asynchronous content synchronisations with physical storage after preceding operations.
    \return A batch of op handles.
    \param ops A batch of op handles.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool) to complete if content synchronisation is constant time (which is extremely unlikely).}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> sync(const std::vector<async_io_op> &ops) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    /*! \brief Schedule an asynchronous content synchronisation with physical storage after a preceding operation.
    \return An op handle.
    \param req An op handle.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if content synchronisation is constant time (which is extremely unlikely).}
    \exceptionmodelstd
    \qexample{readwrite_example}
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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> close(const std::vector<async_io_op> &ops) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
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

    /*! \brief Schedule a batch of asynchronous data reads after preceding operations, where
    offset and total data read must not exceed the present file size.

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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> read(const std::vector<detail::async_data_op_req_impl<false>> &ops) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    template<class T> inline std::vector<async_io_op> read(const std::vector<async_data_op_req<T>> &ops);
#else
    template<class T> BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> read(const std::vector<async_data_op_req<T>> &ops) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
#endif
    /*! \brief Schedule an asynchronous data read after a preceding operation, where
    offset and total data read must not exceed the present file size.

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
    /*! \brief Schedule a batch of asynchronous data writes after preceding operations, where
    offset and total data written must not exceed the present file size.

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
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> write(const std::vector<detail::async_data_op_req_impl<true>> &ops) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    template<class T> inline std::vector<async_io_op> write(const std::vector<async_data_op_req<T>> &ops);
#else
    template<class T> BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> write(const std::vector<async_data_op_req<const T>> &ops) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
#endif
    /*! \brief Schedule an asynchronous data write after a preceding operation, where
    offset and total data written must not exceed the present file size.

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
    \qexample{readwrite_example}
    */
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::vector<async_io_op> truncate(const std::vector<async_io_op> &ops, const std::vector<off_t> &sizes) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    /*! \brief Schedule an asynchronous file length truncation after a preceding operation.
    \return An op handle.
    \param op An op handle.
    \param newsize The new size for the file.
    \ingroup async_file_io_dispatcher_base__filedirops
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(1) to complete if truncating file lengths is constant time.}
    \exceptionmodelstd
    \qexample{readwrite_example}
    */
    inline async_io_op truncate(const async_io_op &op, off_t newsize);
    /*! \brief Schedule a batch of asynchronous directory enumerations after a preceding operations.

    By default dir() returns shared handles i.e. dir("foo") and dir("foo") will return the exact same
    handle, and therefore enumerating not all of the entries at once is a race condition. The solution is
    to either set maxitems to a value large enough to guarantee a directory will be enumerated in a single
    shot, or to open a separate directory handle using the file_flags::UniqueDirectoryHandle flag.

    Note that setting maxitems=1 will often cause a buffer space exhaustion, causing a second syscall
    with an enlarged buffer. This is because AFIO cannot know if the allocated buffer can hold all of
    the filename being retrieved, so it may have to retry. Put another way, setting maxitems=1 will
    give you the worst performance possible, whereas maxitems=2 will probably only return one item most of the time.

    \return A batch of future vectors of directory entries with boolean returning false if done.
    \param reqs A batch of enumeration requests.
    \ingroup async_file_io_dispatcher_base__enumerate
    \qbk{distinguish, batch}
    \complexity{Amortised O(N) to dispatch. Amortised O(N/threadpool*M) to complete where M is the average number of entries in each directory.}
    \exceptionmodelstd
    \qexample{enumerate_example}
    */
    BOOST_AFIO_HEADERS_ONLY_VIRTUAL_SPEC std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> enumerate(const std::vector<async_enumerate_op_req> &reqs) BOOST_AFIO_HEADERS_ONLY_VIRTUAL_UNDEFINED_SPEC
    /*! \brief Schedule an asynchronous directory enumeration after a preceding operation.

    By default dir() returns shared handles i.e. dir("foo") and dir("foo") will return the exact same
    handle, and therefore enumerating not all of the entries at once is a race condition. The solution is
    to either set maxitems to a value large enough to guarantee a directory will be enumerated in a single
    shot, or to open a separate directory handle using the file_flags::UniqueDirectoryHandle flag.

    Note that setting maxitems=1 will often cause a buffer space exhaustion, causing a second syscall
    with an enlarged buffer. This is because AFIO cannot know if the allocated buffer can hold all of
    the filename being retrieved, so it may have to retry. Put another way, setting maxitems=1 will
    give you the worst performance possible, whereas maxitems=2 will probably only return one item most of the time.
    
    \return A future vector of directory entries with a boolean returning false if done.
    \param req An enumeration request.
    \ingroup async_file_io_dispatcher_base__enumerate
    \qbk{distinguish, single}
    \complexity{Amortised O(1) to dispatch. Amortised O(M) to complete where M is the average number of entries in each directory.}
    \exceptionmodelstd
    \qexample{enumerate_example}
    */
    inline std::pair<future<std::pair<std::vector<directory_entry>, bool>>, async_io_op> enumerate(const async_enumerate_op_req &req);

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
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> barrier(const std::vector<async_io_op> &ops);

    /*! \brief Returns the page size of this architecture which is useful for calculating direct i/o multiples.
    \return The page size of this architecture.
    \ingroup async_file_io_dispatcher_base__misc
    \complexity{Whatever the system API takes (one would hope constant time).}
    \exceptionmodel{Never throws any exception.}
    */
    static BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC size_t page_size() BOOST_NOEXCEPT_OR_NOTHROW;
        
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC void complete_async_op(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr e=exception_ptr());
protected:
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC completion_returntype invoke_user_completion_fast(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, completion_t *callback);
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC completion_returntype invoke_user_completion_slow(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::function<completion_t> callback);
    template<class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_io_op> &preconditions, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, T));
    template<class F, class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> chain_async_ops(int optype, const std::vector<T> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, T));
    template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_io_op> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_io_op));
    template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> chain_async_ops(int optype, const std::vector<async_path_op_req> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_path_op_req));
    template<class F, bool iswrite> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::vector<async_io_op> chain_async_ops(int optype, const std::vector<detail::async_data_op_req_impl<iswrite>> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, detail::async_data_op_req_impl<iswrite>));
    template<class F> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::pair<std::vector<future<std::pair<std::vector<directory_entry>, bool>>>, std::vector<async_io_op>> chain_async_ops(int optype, const std::vector<async_enumerate_op_req> &container, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, async_enumerate_op_req, std::shared_ptr<promise<std::pair<std::vector<directory_entry>, bool>>>));
    template<class T> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_file_io_dispatcher_base::completion_returntype dobarrier(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *, T);

#ifndef BOOST_NO_CXX11_VARIADIC_TEMPLATES
    
    template<class F, class... Args> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC std::shared_ptr<async_io_handle> invoke_async_op_completions(size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *e, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, Args...), Args... args);
    template<class F, class... Args> BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC async_io_op chain_async_op(exception_ptr *he, detail::immediate_async_ops &immediates, int optype, const async_io_op &precondition, async_op_flags flags, completion_returntype(F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr *, Args...), Args... args);

#else

#define BOOST_PP_LOCAL_MACRO(N)                                                                     \
    template <class F                                                                               \
    BOOST_PP_COMMA_IF(N)                                                                            \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                               \
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC                                                            \
    std::shared_ptr<async_io_handle>                                                        \
    invoke_async_op_completions                                                                     \
    (size_t id, std::shared_ptr<async_io_handle> h, exception_ptr *,                        \
    completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr * \
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
    BOOST_AFIO_HEADERS_ONLY_MEMFUNC_SPEC                                                            \
    async_io_op                                      /* return type */                              \
    chain_async_op                                   /* function name */                            \
    (exception_ptr *he, detail::immediate_async_ops &immediates, int optype,           /* parameters start */          \
    const async_io_op &precondition,async_op_flags flags,                                           \
    completion_returntype (F::*f)(size_t, std::shared_ptr<async_io_handle>, exception_ptr * \
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
BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC std::shared_ptr<async_file_io_dispatcher_base> make_async_file_io_dispatcher(std::shared_ptr<thread_source> threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);

/*! \struct async_io_op
\brief A reference to an asynchronous operation

The id field is always valid (and non-zero) if this reference is valid. The h field, being the shared state
between all references referring to the same op, only becomes a non-default shared_future when the op has
actually begun execution. You should therefore \b never try waiting via h->get() until you are absolutely sure
that the op has already started, instead do when_all(op).get().front().
*/
struct async_io_op
{
    async_file_io_dispatcher_base *parent;              //!< The parent dispatcher
    size_t id;                                          //!< A unique id for this operation
    std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>> h; //!< A future handle to the item being operated upon

    //! \constr
    async_io_op() : parent(nullptr), id(0), h(std::make_shared<shared_future<std::shared_ptr<async_io_handle>>>()) { }
    //! \cconstr
    async_io_op(const async_io_op &o) : parent(o.parent), id(o.id), h(o.h) { }
    //! \mconstr
    async_io_op(async_io_op &&o) BOOST_NOEXCEPT_OR_NOTHROW : parent(std::move(o.parent)), id(std::move(o.id)), h(std::move(o.h)) { }
    /*! Constructs an instance.
    \param _parent The dispatcher this op belongs to.
    \param _id The unique non-zero id of this op.
    \param _handle A shared_ptr to shared state between all instances of this reference.
    */
    async_io_op(async_file_io_dispatcher_base *_parent, size_t _id, std::shared_ptr<shared_future<std::shared_ptr<async_io_handle>>> _handle) : parent(_parent), id(_id), h(std::move(_handle)) { _validate(); }
    /*! Constructs an instance.
    \param _parent The dispatcher this op belongs to.
    \param _id The unique non-zero id of this op.
    */
    async_io_op(async_file_io_dispatcher_base *_parent, size_t _id) : parent(_parent), id(_id), h(std::make_shared<shared_future<std::shared_ptr<async_io_handle>>>()) { }
    //! \cassign
    async_io_op &operator=(const async_io_op &o) { parent=o.parent; id=o.id; h=o.h; return *this; }
    //! \massign
    async_io_op &operator=(async_io_op &&o) BOOST_NOEXCEPT_OR_NOTHROW { parent=std::move(o.parent); id=std::move(o.id); h=std::move(o.h); return *this; }
    //! Validates contents
    bool validate(bool check_handle=true) const
    {
        if(!parent || !id) return false;
        // If h is valid and ready and contains an exception, throw it now
        if(h->valid() && h->is_ready() /*h->wait_for(seconds(0))==future_status::ready*/)
        {
            if(!check_handle)
                h->get();
            else
                if(!h->get().get())
                    return false;
        }
        return true;
    }
private:
    void _validate() const
    {
#if BOOST_AFIO_VALIDATE_INPUTS
        if(!validate())
            BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
#endif
    }
};

namespace detail
{
    struct when_all_count_completed_state
    {
        std::vector<async_io_op> inputs;
        atomic<size_t> togo;
        std::vector<std::shared_ptr<async_io_handle>> out;
        promise<std::vector<std::shared_ptr<async_io_handle>>> done;
        when_all_count_completed_state(std::vector<async_io_op> _inputs) : inputs(std::move(_inputs)), togo(inputs.size()), out(inputs.size()) { }
        when_all_count_completed_state(std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last) : inputs(first, last), togo(inputs.size()), out(inputs.size()) { }
    };
    inline async_file_io_dispatcher_base::completion_returntype when_all_count_completed_nothrow(size_t, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::shared_ptr<detail::when_all_count_completed_state> state, size_t idx)
    {
        state->out[idx]=h; // This might look thread unsafe, but each idx is unique
        if(!--state->togo)
            state->done.set_value(state->out);
        return std::make_pair(true, h);
    }
    inline async_file_io_dispatcher_base::completion_returntype when_all_count_completed(size_t, std::shared_ptr<async_io_handle> h, exception_ptr *e, std::shared_ptr<detail::when_all_count_completed_state> state, size_t idx)
    {
        state->out[idx]=h; // This might look thread unsafe, but each idx is unique
        if(!--state->togo)
        {
            bool done=false;
            // Retrieve the result of each input, waiting for it if it is between decrementing the
            // atomic and signalling its future.
            BOOST_FOREACH(auto &i, state->inputs)
            {
                shared_future<std::shared_ptr<async_io_handle>> &future=*i.h;
                auto e(get_exception_ptr(future));
                if(e)
                {
                    state->done.set_exception(e);
                    done=true;
                    break;
                }
            }
            if(!done)
                state->done.set_value(state->out);
        }
        return std::make_pair(true, h);
    }
    inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::nothrow_t _, std::shared_ptr<detail::when_all_count_completed_state> state)
    {
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
        // VS2010 segfaults if you ever do a try { throw; } catch(...) without an exception ever having been thrown :(
        exception_ptr e;
        try
        {
            throw detail::vs2010_lack_of_decent_current_exception_support_hack_t();
        }
        catch(...)
        {
            detail::vs2010_lack_of_decent_current_exception_support_hack()=boost::current_exception();
            try
            {
#endif
                std::vector<async_io_op> &inputs=state->inputs;
#if BOOST_AFIO_VALIDATE_INPUTS
                BOOST_FOREACH(auto &i, inputs)
                {
                    if(!i.validate(false))
                        BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
                }
#endif
                std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
                callbacks.reserve(inputs.size());
                size_t idx=0;
                BOOST_FOREACH(auto &i, inputs)
                {
                    callbacks.push_back(std::make_pair(async_op_flags::immediate/*safe if nothrow*/, std::bind(&detail::when_all_count_completed_nothrow, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, state, idx++)));
                }
                inputs.front().parent->completion(inputs, callbacks);
                return state->done.get_future();
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
            }
            catch(...)
            {
                e=afio::make_exception_ptr(afio::current_exception());
            }
        }
        if(e)
            rethrow_exception(e);
        else
            BOOST_AFIO_THROW_FATAL(std::runtime_error("Exception pointer null despite exiting via exception handling code path."));
        return future<std::vector<std::shared_ptr<async_io_handle>>>();
#endif
    }
    inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::shared_ptr<detail::when_all_count_completed_state> state)
    {
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
        // VS2010 segfaults if you ever do a try { throw; } catch(...) without an exception ever having been thrown :(
        exception_ptr e;
        try
        {
            throw detail::vs2010_lack_of_decent_current_exception_support_hack_t();
        }
        catch(...)
        {
            detail::vs2010_lack_of_decent_current_exception_support_hack()=boost::current_exception();
            try
            {
#endif
                std::vector<async_io_op> &inputs=state->inputs;
#if BOOST_AFIO_VALIDATE_INPUTS
                BOOST_FOREACH(auto &i, inputs)
                {
                    if(!i.validate(false))
                        BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
                }
#endif
                std::vector<std::pair<async_op_flags, std::function<async_file_io_dispatcher_base::completion_t>>> callbacks;
                callbacks.reserve(inputs.size());
                size_t idx=0;
                BOOST_FOREACH(auto &i, inputs)
                {
                    callbacks.push_back(std::make_pair(async_op_flags::none/*can't be immediate as may try to retrieve exception state of own precondition*/, std::bind(&detail::when_all_count_completed, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, state, idx++)));
                }
                inputs.front().parent->completion(inputs, callbacks);
                return state->done.get_future();
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
            }
            catch(...)
            {
                e=afio::make_exception_ptr(afio::current_exception());
            }
        }
        if(e)
            rethrow_exception(e);
        else
            BOOST_AFIO_THROW_FATAL(std::runtime_error("Exception pointer null despite exiting via exception handling code path."));
        return future<std::vector<std::shared_ptr<async_io_handle>>>();
#endif
    }
}

/*! \brief Returns a result when all the supplied ops complete. Does not propagate exception states.

\return A future vector of shared_ptr's to async_io_handle.
\param _ An instance of std::nothrow_t.
\param first A vector iterator pointing to the first async_io_op to wait upon.
\param last A vector iterator pointing after the last async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, iterator batch of ops not exception propagating}
\complexity{O(N) to dispatch. O(N/threadpool) to complete, but at least one cache line is contended between threads.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::nothrow_t _, std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
    if(first==last)
        return future<std::vector<std::shared_ptr<async_io_handle>>>();
    auto state(std::make_shared<detail::when_all_count_completed_state>(first, last));
    return detail::when_all(_, state);
}
/*! \brief Returns a result when all the supplied ops complete. Does not propagate exception states.

\return A future vector of shared_ptr's to async_io_handle.
\param _ An instance of std::nothrow_t.
\param ops A vector of the async_io_ops to wait upon.
\ingroup when_all_ops
\qbk{distinguish, vector batch of ops not exception propagating}
\complexity{O(N) to dispatch. O(N/threadpool) to complete, but at least one cache line is contended between threads.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::nothrow_t _, std::vector<async_io_op> ops)
{
    if(ops.empty())
        return future<std::vector<std::shared_ptr<async_io_handle>>>();
    auto state(std::make_shared<detail::when_all_count_completed_state>(std::move(ops)));
    return detail::when_all(_, state);
}
/*! \brief Returns a result when all the supplied ops complete. Propagates exception states.

\return A future vector of shared_ptr's to async_io_handle.
\param first A vector iterator pointing to the first async_io_op to wait upon.
\param last A vector iterator pointing after the last async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, iterator batch of ops exception propagating}
\complexity{O(N) to dispatch. O(N/threadpool) to complete, but at least one cache line is contended between threads.}
\exceptionmodel{Propagating}
*/
inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::vector<async_io_op>::iterator first, std::vector<async_io_op>::iterator last)
{
    if(first==last)
        return future<std::vector<std::shared_ptr<async_io_handle>>>();
    auto state(std::make_shared<detail::when_all_count_completed_state>(first, last));
    return detail::when_all(state);
}
/*! \brief Returns a result when all the supplied ops complete. Propagates exception states.

\return A future vector of shared_ptr's to async_io_handle.
\param ops A vector of the async_io_ops to wait upon.
\ingroup when_all_ops
\qbk{distinguish, vector batch of ops exception propagating}
\complexity{O(N) to dispatch. O(N/threadpool) to complete, but at least one cache line is contended between threads.}
\exceptionmodel{Propagating}
*/
inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::vector<async_io_op> ops)
{
    if(ops.empty())
        return future<std::vector<std::shared_ptr<async_io_handle>>>();
    auto state(std::make_shared<detail::when_all_count_completed_state>(std::move(ops)));
    return detail::when_all(state);
}
/*! \brief Returns a result when the supplied op completes. Does not propagate exception states.

\return A future vector of shared_ptr's to async_io_handle.
\param _ An instance of std::nothrow_t.
\param op An async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, convenience single op not exception propagating}
\complexity{O(1) to dispatch. O(1) to complete.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(std::nothrow_t _, async_io_op op)
{
    std::vector<async_io_op> ops(1, op);
    return when_all(_, ops);
}
/*! \brief Returns a result when the supplied op completes. Propagates exception states.

\return A future vector of shared_ptr's to async_io_handle.
\param op An async_io_op to wait upon.
\ingroup when_all_ops
\qbk{distinguish, convenience single op exception propagating}
\complexity{O(1) to dispatch. O(1) to complete.}
\exceptionmodel{Non propagating}
*/
inline future<std::vector<std::shared_ptr<async_io_handle>>> when_all(async_io_op op)
{
    std::vector<async_io_op> ops(1, op);
    return when_all(ops);
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
            BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
#endif
    }
};

namespace detail
{
    //! \brief The implementation of all async_data_op_req specialisations. \tparam for_writing Whether this implementation is for writing data. \ingroup async_data_op_req
    template<bool for_writing> class async_data_op_req_impl;
    template<> class async_data_op_req_impl<false>
    {
    public:
        //! An optional precondition for this operation
        async_io_op precondition;
        //! A sequence of mutable Boost.ASIO buffers to read into
        std::vector<boost::asio::mutable_buffer> buffers;
        //! The offset from which to read
        off_t where;
        //! \constr
        async_data_op_req_impl() { }
        //! \cconstr
        async_data_op_req_impl(const async_data_op_req_impl &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
        //! \mconstr
        async_data_op_req_impl(async_data_op_req_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
        //! \cassign
        async_data_op_req_impl &operator=(const async_data_op_req_impl &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
        //! \massign
        async_data_op_req_impl &operator=(async_data_op_req_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
        //! \async_data_op_req1 \param _length The number of bytes to transfer
        async_data_op_req_impl(async_io_op _precondition, void *v, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::mutable_buffer(v, _length)); _validate(); }
        //! \async_data_op_req2
        async_data_op_req_impl(async_io_op _precondition, std::vector<boost::asio::mutable_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(std::move(_buffers)), where(_where) { _validate(); }
        //! \async_data_op_req2 \tparam N Number of boost::asio::mutable_buffer
        template<size_t N> async_data_op_req_impl(async_io_op _precondition, std::array<boost::asio::mutable_buffer, N> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(std::make_move_iterator(_buffers.begin()), std::make_move_iterator(_buffers.end())), where(_where) { _validate(); }
        //! \async_data_op_req3
        async_data_op_req_impl(async_io_op _precondition, boost::asio::mutable_buffer _buffer, off_t _where) : precondition(std::move(_precondition)), buffers(1, std::move(_buffer)), where(_where) { _validate(); }
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
                BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
#endif
        }
    };
    template<> class async_data_op_req_impl<true>
    {
    public:
        //! An optional precondition for this operation
        async_io_op precondition;
        //! A sequence of mutable Boost.ASIO buffers to read into
        std::vector<boost::asio::const_buffer> buffers;
        //! The offset from which to read
        off_t where;
        //! \constr
        async_data_op_req_impl() { }
        //! \cconstr
        async_data_op_req_impl(const async_data_op_req_impl &o) : precondition(o.precondition), buffers(o.buffers), where(o.where) { }
        //! \mconstr
        async_data_op_req_impl(async_data_op_req_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW : precondition(std::move(o.precondition)), buffers(std::move(o.buffers)), where(std::move(o.where)) { }
        //! \cconstr
        async_data_op_req_impl(const async_data_op_req_impl<false> &o) : precondition(o.precondition), where(o.where) { buffers.reserve(o.buffers.capacity()); BOOST_FOREACH(auto &i, o.buffers){ buffers.push_back(i); } }
        //! \mconstr
        async_data_op_req_impl(async_data_op_req_impl<false> &&o) BOOST_NOEXCEPT_OR_NOTHROW : precondition(std::move(o.precondition)), where(std::move(o.where)) { buffers.reserve(o.buffers.capacity()); BOOST_FOREACH(auto &&i, o.buffers){ buffers.push_back(std::move(i)); } }
        //! \cassign
        async_data_op_req_impl &operator=(const async_data_op_req_impl &o) { precondition=o.precondition; buffers=o.buffers; where=o.where; return *this; }
        //! \massign
        async_data_op_req_impl &operator=(async_data_op_req_impl &&o) BOOST_NOEXCEPT_OR_NOTHROW { precondition=std::move(o.precondition); buffers=std::move(o.buffers); where=std::move(o.where); return *this; }
        //! \async_data_op_req1 \param _length The number of bytes to transfer
        async_data_op_req_impl(async_io_op _precondition, const void *v, size_t _length, off_t _where) : precondition(std::move(_precondition)), where(_where) { buffers.reserve(1); buffers.push_back(boost::asio::const_buffer(v, _length)); _validate(); }
        //! \async_data_op_req2
        async_data_op_req_impl(async_io_op _precondition, std::vector<boost::asio::const_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(std::move(_buffers)), where(_where) { _validate(); }
        //! \async_data_op_req2
        async_data_op_req_impl(async_io_op _precondition, std::vector<boost::asio::mutable_buffer> _buffers, off_t _where) : precondition(std::move(_precondition)), where(_where)
        {
            buffers.reserve(_buffers.capacity());
            BOOST_FOREACH(auto &&i, _buffers)
                buffers.push_back(std::move(i));
            _validate();
        }
        //! \async_data_op_req2 \tparam N Number of boost::asio::const_buffer
        template<size_t N> async_data_op_req_impl(async_io_op _precondition, std::array<boost::asio::const_buffer, N> _buffers, off_t _where) : precondition(std::move(_precondition)), buffers(std::make_move_iterator(_buffers.begin()), std::make_move_iterator(_buffers.end())), where(_where) { _validate(); }
        //! \async_data_op_req2 \tparam N Number of boost::asio::mutable_buffer
        template<size_t N> async_data_op_req_impl(async_io_op _precondition, std::array<boost::asio::mutable_buffer, N> _buffers, off_t _where) : precondition(std::move(_precondition)), where(_where)
        {
            buffers.reserve(_buffers.size());
            BOOST_FOREACH(auto &&i, _buffers)
                buffers.push_back(std::move(i));
            _validate();
        }
        //! \async_data_op_req3
        async_data_op_req_impl(async_io_op _precondition, boost::asio::const_buffer _buffer, off_t _where) : precondition(std::move(_precondition)), buffers(1, std::move(_buffer)), where(_where) { _validate(); }
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
                BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
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
    async_data_op_req()
    {
        static_assert(std::is_trivial<T>::value, "async_data_op_req<T> has not been specialised for this non-trivial type, which suggests you are trying to read or write a complex C++ type! Either add a custom specialisation, or directly instantiate an async_data_op_req with a void * and size_t length to some serialised representation.");
    }
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
    //! \async_data_op_req1 \tparam N The size of the array
    template<size_t N> async_data_op_req(async_io_op _precondition, T (&v)[N], off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), static_cast<void *>(v), N*sizeof(T), _where) { }
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
    async_data_op_req()
    {
        static_assert(std::is_trivial<T>::value, "async_data_op_req<T> has not been specialised for this non-trivial type, which suggests you are trying to read or write a complex C++ type! Either add a custom specialisation, or directly instantiate an async_data_op_req with a void * and size_t length to some serialised representation.");
    }
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
    //! \async_data_op_req1 \tparam N The size of the array
    template<size_t N> async_data_op_req(async_io_op _precondition, const T (&v)[N], off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), static_cast<const void *>(v), N*sizeof(const T), _where) { }
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
//! \brief A convenience bundle of precondition, data and where for reading into a `std::vector<boost::asio::mutable_buffer, A>`. Data \b MUST stay around until the operation completes. \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class A> struct async_data_op_req<std::vector<boost::asio::mutable_buffer, A>> : public detail::async_data_op_req_impl<false>
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
    async_data_op_req(async_io_op _precondition, std::vector<boost::asio::mutable_buffer, A> v, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), std::move(v), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `std::vector<boost::asio::const_buffer, A>`. Data \b MUST stay around until the operation completes. \tparam "class A" Any STL allocator \ingroup async_data_op_req
template<class A> struct async_data_op_req<std::vector<boost::asio::const_buffer, A>> : public detail::async_data_op_req_impl<true>
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
    async_data_op_req(const async_data_op_req<std::vector<boost::asio::mutable_buffer, A>> &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
    async_data_op_req(async_data_op_req<std::vector<boost::asio::mutable_buffer, A>> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cassign
    async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
    //! \massign
    async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
    template<class T, class A2> async_data_op_req(async_io_op _precondition, std::vector<T, A2> v, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), std::move(v), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for reading into a `std::array<boost::asio::mutable_buffer, N>`. Data \b MUST stay around until the operation completes. \tparam N The size of the array. \ingroup async_data_op_req
template<size_t N> struct async_data_op_req<std::array<boost::asio::mutable_buffer, N>> : public detail::async_data_op_req_impl<false>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
    //! A precondition containing an open file handle for this operation
    async_io_op precondition;
    //! A sequence of mutable Boost.ASIO buffers to read into
    std::array<boost::asio::mutable_buffer, N> buffers;
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
    async_data_op_req(async_io_op _precondition, std::array<boost::asio::mutable_buffer, N> v, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), std::move(v), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `std::array<boost::asio::const_buffer, A>`. Data \b MUST stay around until the operation completes. \tparam N The size of the array. \ingroup async_data_op_req
template<size_t N> struct async_data_op_req<std::array<boost::asio::const_buffer, N>> : public detail::async_data_op_req_impl<true>
{
#ifdef DOXYGEN_SHOULD_SKIP_THIS
    //! A precondition containing an open file handle for this operation
    async_io_op precondition;
    //! A sequence of const Boost.ASIO buffers to write from
    std::array<boost::asio::const_buffer, N> buffers;
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
    async_data_op_req(const async_data_op_req<std::array<boost::asio::mutable_buffer, N>> &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
    async_data_op_req(async_data_op_req<std::array<boost::asio::mutable_buffer, N>> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cassign
    async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
    //! \massign
    async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
    template<class T> async_data_op_req(async_io_op _precondition, std::array<T, N> v, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), std::move(v), _where) { }
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
//! \brief A convenience bundle of precondition, data and where for reading into a `boost::asio::mutable_buffer`. Data \b MUST stay around until the operation completes. \ingroup async_data_op_req
template<> struct async_data_op_req<boost::asio::mutable_buffer> : public detail::async_data_op_req_impl<false>
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
    async_data_op_req(async_io_op _precondition, boost::asio::mutable_buffer v, off_t _where) : detail::async_data_op_req_impl<false>(std::move(_precondition), std::move(v), _where) { }
};
//! \brief A convenience bundle of precondition, data and where for writing from a `boost::asio::const_buffer`. Data \b MUST stay around until the operation completes. \ingroup async_data_op_req
template<> struct async_data_op_req<boost::asio::const_buffer> : public detail::async_data_op_req_impl<true>
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
    async_data_op_req(const async_data_op_req<boost::asio::mutable_buffer> &o) : detail::async_data_op_req_impl<true>(o) { }
    //! \mconstr
    async_data_op_req(async_data_op_req<boost::asio::mutable_buffer> &&o) BOOST_NOEXCEPT_OR_NOTHROW : detail::async_data_op_req_impl<true>(std::move(o)) { }
    //! \cassign
    async_data_op_req &operator=(const async_data_op_req &o) { static_cast<detail::async_data_op_req_impl<true>>(*this)=o; return *this; }
    //! \massign
    async_data_op_req &operator=(async_data_op_req &&o) BOOST_NOEXCEPT_OR_NOTHROW { static_cast<detail::async_data_op_req_impl<true>>(*this)=std::move(o); return *this; }
    //! \async_data_op_req1
    template<class T> async_data_op_req(async_io_op _precondition, T v, off_t _where) : detail::async_data_op_req_impl<true>(std::move(_precondition), std::move(v), _where) { }
};

/*! \brief Convenience instantiator of a async_data_op_req, letting the compiler deduce the template specialisation to use.

\return An async_data_op_req matching the supplied parameter type.
\async_data_op_req1
\ingroup make_async_data_op_req
\qbk{
[heading Example]
[readwrite_example]
}
*/
template<class T> inline async_data_op_req<typename std::remove_pointer<typename std::decay<T>::type>::type> make_async_data_op_req(async_io_op _precondition, T &&v, off_t _where)
{
    typedef typename std::remove_pointer<typename std::decay<T>::type>::type _T;
    return async_data_op_req<_T>(_precondition, v, _where);
}
/*! \brief Convenience instantiator of a async_data_op_req, letting the compiler deduce the template specialisation to use.

\return An async_data_op_req matching the supplied parameter type.
\async_data_op_req1
\param _length The number of bytes to transfer
\ingroup make_async_data_op_req
\qbk{
[heading Example]
[readwrite_example]
}
*/
template<class T> inline async_data_op_req<typename std::remove_pointer<typename std::decay<T>::type>::type> make_async_data_op_req(async_io_op _precondition, T &&v, size_t _length, off_t _where)
{
    typedef typename std::remove_pointer<typename std::decay<T>::type>::type _T;
    return async_data_op_req<_T>(_precondition, v, _length, _where);
}


/*! \struct async_enumerate_op_req
\brief A convenience bundle of precondition, number of items to enumerate, item pattern match and metadata to prefetch.
*/
struct async_enumerate_op_req
{
    async_io_op precondition;    //!< A precondition for this operation.
    size_t maxitems;             //!< The maximum number of items to return in this request. Note that setting to one will often invoke two syscalls.
    bool restart;                //!< Restarts the enumeration for this open directory handle.
    std::filesystem::path glob;  //!< An optional shell glob by which to filter the items returned. Done kernel side on Windows, user side on POSIX.
    metadata_flags metadata;     //!< The metadata to prefetch for each item enumerated. AFIO may fetch more metadata than requested if it is cost free.
    //! \constr
    async_enumerate_op_req() : maxitems(0), restart(false), metadata(metadata_flags::None) { }
    /*! \brief Constructs an instance.
    
    \param _precondition The precondition for this operation.
    \param _maxitems The maximum number of items to return in this request. Note that setting to one will often invoke two syscalls.
    \param _restart Restarts the enumeration for this open directory handle.
    \param _glob An optional shell glob by which to filter the items returned. Done kernel side on Windows, user side on POSIX.
    \param _metadata The metadata to prefetch for each item enumerated. AFIO may fetch more metadata than requested if it is cost free.
    */
    async_enumerate_op_req(async_io_op _precondition, size_t _maxitems=2, bool _restart=true, std::filesystem::path _glob=std::filesystem::path(), metadata_flags _metadata=metadata_flags::None) : precondition(std::move(_precondition)), maxitems(_maxitems), restart(_restart), glob(std::move(_glob)), metadata(_metadata) { _validate(); }
    /*! \brief Constructs an instance.
    
    \param _precondition The precondition for this operation.
    \param _glob A shell glob by which to filter the items returned. Done kernel side on Windows, user side on POSIX.
    \param _maxitems The maximum number of items to return in this request. Note that setting to one will often invoke two syscalls.
    \param _restart Restarts the enumeration for this open directory handle.
    \param _metadata The metadata to prefetch for each item enumerated. AFIO may fetch more metadata than requested if it is cost free.
    */
    async_enumerate_op_req(async_io_op _precondition, std::filesystem::path _glob, size_t _maxitems=2, bool _restart=true, metadata_flags _metadata=metadata_flags::None) : precondition(std::move(_precondition)), maxitems(_maxitems), restart(_restart), glob(std::move(_glob)), metadata(_metadata) { _validate(); }
    /*! \brief Constructs an instance.
    
    \param _precondition The precondition for this operation.
    \param _metadata The metadata to prefetch for each item enumerated. AFIO may fetch more metadata than requested if it is cost free.
    \param _maxitems The maximum number of items to return in this request. Note that setting to one will often invoke two syscalls.
    \param _restart Restarts the enumeration for this open directory handle.
    \param _glob An optional shell glob by which to filter the items returned. Done kernel side on Windows, user side on POSIX.
    */
    async_enumerate_op_req(async_io_op _precondition, metadata_flags _metadata, size_t _maxitems=2, bool _restart=true, std::filesystem::path _glob=std::filesystem::path()) : precondition(std::move(_precondition)), maxitems(_maxitems), restart(_restart), glob(std::move(_glob)), metadata(_metadata) { _validate(); }
    //! Validates contents
    bool validate() const
    {
        if(!maxitems) return false;
        return !precondition.id || precondition.validate();
    }
private:
    void _validate() const
    {
#if BOOST_AFIO_VALIDATE_INPUTS
        if(!validate())
            BOOST_AFIO_THROW(std::invalid_argument("Inputs are invalid."));
#endif
    }
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

#if defined(BOOST_AFIO_ENABLE_BENCHMARKING_COMPLETION) // Only really used for benchmarking
inline async_io_op async_file_io_dispatcher_base::completion(const async_io_op &req, const std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *> &callback)
{
    std::vector<async_io_op> r;
    std::vector<std::pair<async_op_flags, async_file_io_dispatcher_base::completion_t *>> i;
    r.reserve(1); i.reserve(1);
    r.push_back(req);
    i.push_back(callback);
    return std::move(completion(r, i).front());
}
#endif
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
    template<class tasktype> std::pair<bool, std::shared_ptr<async_io_handle>> doCall(size_t, std::shared_ptr<async_io_handle> _, exception_ptr *, std::shared_ptr<tasktype> c)
    {
        (*c)();
        return std::make_pair(true, _);
    }
}
template<class R> inline std::pair<std::vector<future<R>>, std::vector<async_io_op>> async_file_io_dispatcher_base::call(const std::vector<async_io_op> &ops, const std::vector<std::function<R()>> &callables)
{
    typedef enqueued_task<R()> tasktype;
    std::vector<future<R>> retfutures;
    std::vector<std::pair<async_op_flags, std::function<completion_t>>> callbacks;
    retfutures.reserve(callables.size());
    callbacks.reserve(callables.size());
    
    BOOST_FOREACH(auto &t, callables)
    {
        std::shared_ptr<tasktype> c(std::make_shared<tasktype>(std::function<R()>(t)));
        retfutures.push_back(c->get_future());
        callbacks.push_back(std::make_pair(async_op_flags::none, std::bind(&detail::doCall<tasktype>, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::move(c))));
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

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

#ifndef DOXYGEN_SHOULD_SKIP_THIS
template<class C, class... Args> inline std::pair<future<typename detail::vs2013_variadic_overload_resolution_workaround<C, Args...>::type>, async_io_op> async_file_io_dispatcher_base::call(const async_io_op &req, C callback, Args... args)
#else
template<class C, class... Args> inline std::pair<future<typename std::result_of<C(Args...)>::type>, async_io_op> async_file_io_dispatcher_base::call(const async_io_op &req, C callback, Args... args)
#endif
{
    typedef typename std::result_of<C(Args...)>::type rettype;
    return call(req, std::function<rettype()>(std::bind<rettype>(callback, args...)));
}

#else

#define BOOST_PP_LOCAL_MACRO(N)                                                                                         \
    template <class C                                                                                                   \
    BOOST_PP_COMMA_IF(N)                                                                                                \
    BOOST_PP_ENUM_PARAMS(N, class A)>                                                                                   \
    inline std::pair<future<typename std::result_of<C(BOOST_PP_ENUM_PARAMS(N, A))>::type>, async_io_op>               \
    async_file_io_dispatcher_base::call (const async_io_op &req, C callback BOOST_PP_COMMA_IF(N) BOOST_PP_ENUM_BINARY_PARAMS(N, A, a))                 \
    {                                                                                                                   \
        typedef typename std::result_of<C(BOOST_PP_ENUM_PARAMS(N, A))>::type rettype;                                   \
        return call(req, std::function<rettype()>(std::bind<rettype>(callback, BOOST_PP_ENUM_PARAMS(N, a))));                                   \
    }
  
#define BOOST_PP_LOCAL_LIMITS     (1, BOOST_AFIO_MAX_PARAMETERS) //should this be 0 or 1 for the min????
#include BOOST_PP_LOCAL_ITERATE()


#endif

inline async_io_op async_file_io_dispatcher_base::adopt(std::shared_ptr<async_io_handle> h)
{
    std::vector<std::shared_ptr<async_io_handle>> i;
    i.reserve(1);
    i.push_back(std::move(h));
    return std::move(adopt(i).front());
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
inline async_io_op async_file_io_dispatcher_base::symlink(const async_path_op_req &req)
{
    std::vector<async_path_op_req> i;
    i.reserve(1);
    i.push_back(req);
    return std::move(symlink(i).front());
}
inline async_io_op async_file_io_dispatcher_base::rmsymlink(const async_path_op_req &req)
{
    std::vector<async_path_op_req> i;
    i.reserve(1);
    i.push_back(req);
    return std::move(rmsymlink(i).front());
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
inline std::pair<future<std::pair<std::vector<directory_entry>, bool>>, async_io_op> async_file_io_dispatcher_base::enumerate(const async_enumerate_op_req &req)
{
    std::vector<async_enumerate_op_req> i;
    i.reserve(1);
    i.push_back(req);
    auto ret=enumerate(i);
    return std::make_pair(std::move(ret.first.front()), std::move(ret.second.front()));
}


} } // namespace boost

// Specialise std::hash<> for directory_entry
#ifndef BOOST_AFIO_DISABLE_STD_HASH_SPECIALIZATION
#include <functional>
namespace std
{
    template<> struct hash<boost::afio::directory_entry>
    {
    public:
        size_t operator()(const boost::afio::directory_entry &p) const
        {
            return boost::afio::directory_entry_hash()(p);
        }
    };

}//namesapce std
#endif

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

#if BOOST_AFIO_HEADERS_ONLY == 1
#undef BOOST_AFIO_VALIDATE_INPUTS // Let BOOST_AFIO_NEVER_VALIDATE_INPUTS take over
#include "detail/impl/afio.ipp"
#endif

#endif
