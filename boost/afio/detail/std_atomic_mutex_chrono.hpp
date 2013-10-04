/* std_atomic_mutex_chrono.hpp
Maps in STL atomic, mutex, chrono preferentially, Boost otherwise.
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Sept 2013
*/

#ifndef BOOST_AFIO_MAP_IN_CPP11_STL
#define BOOST_AFIO_MAP_IN_CPP11_STL

#include "boost/thread/thread.hpp"
#include "boost/thread/future.hpp"

#include <cstdint>

// Map in C++11 stuff if available
#if defined(BOOST_NO_CXX11_HDR_MUTEX) || defined(BOOST_NO_CXX11_HDR_RATIO) || defined(BOOST_NO_CXX11_HDR_CHRONO) || defined(BOOST_NO_CXX11_HDR_THREAD)
#include "boost/exception_ptr.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/atomic.hpp"
#include "boost/chrono.hpp"
#define BOOST_AFIO_USE_BOOST_ATOMIC
#define BOOST_AFIO_USE_BOOST_CHRONO
namespace boost {
    namespace afio {
        typedef boost::thread thread;
        namespace this_thread=boost::this_thread;
        inline boost::thread::id get_this_thread_id() { return boost::this_thread::get_id(); }
        // Both VS2010 and Mingw32 need this, but not Mingw-w64
#if (defined(BOOST_MSVC) && BOOST_MSVC < 1700 /* <= VS2010 */) || (defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
        namespace detail { struct vs2010_lack_of_decent_current_exception_support_hack_t { }; BOOST_AFIO_HEADERS_ONLY_FUNC_SPEC boost::exception_ptr &vs2010_lack_of_decent_current_exception_support_hack(); }
        inline boost::exception_ptr current_exception() { boost::exception_ptr ret=boost::current_exception(); return (ret==detail::vs2010_lack_of_decent_current_exception_support_hack()) ? boost::exception_ptr() : ret; }
#define BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
#else
        inline boost::exception_ptr current_exception() { return boost::current_exception(); }
#endif
#define BOOST_AFIO_THROW(x) boost::throw_exception(boost::enable_current_exception(x))
#define BOOST_AFIO_RETHROW throw
        typedef boost::mutex mutex;
        typedef boost::recursive_mutex recursive_mutex;
    }
}
#else
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
namespace boost {
    namespace afio {
        typedef std::thread thread;
        namespace this_thread=std::this_thread;
        inline std::thread::id get_this_thread_id() { return std::this_thread::get_id(); }
        inline std::exception_ptr current_exception() { return std::current_exception(); }
#define BOOST_AFIO_THROW(x) throw x
#define BOOST_AFIO_RETHROW throw
        typedef std::mutex mutex;
        typedef std::recursive_mutex recursive_mutex;
    }
}
#endif

namespace boost { namespace afio {

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
    M &operator=(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW{ static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
    M &operator=(const M &o) { static_cast<B<T> &&>(*this)=o; return *this; } \
    M &operator=(M &&o) BOOST_NOEXCEPT_OR_NOTHROW{ static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
};
#define BOOST_AFIO_FORWARD_STL_IMPL_NC(M, B) \
    template<class T> class M : public B<T> \
{ \
public: \
    M() { } \
    M(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW : B<T>(std::move(o)) { } \
    M(M &&o) BOOST_NOEXCEPT_OR_NOTHROW : B<T>(std::move(o)) { } \
    M &operator=(B<T> &&o) BOOST_NOEXCEPT_OR_NOTHROW{ static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
    M &operator=(M &&o) BOOST_NOEXCEPT_OR_NOTHROW{ static_cast<B<T> &&>(*this)=std::move(o); return *this; } \
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
template<typename T> inline exception_ptr get_exception_ptr(future<T> &f)
{
#if 1
    // Thanks to Vicente for adding this to Boost.Thread
    return f.get_exception_ptr();
#else
    // This seems excessive but I don't see any other legal way to extract the exception ...
    bool success=false;
    try
    {
        f.get();
        success=true;
    }
    catch(...)
    {
        exception_ptr e(afio::make_exception_ptr(afio::current_exception()));
        assert(e);
        return e;
    }
    return exception_ptr();
#endif
}
template<typename T> inline exception_ptr get_exception_ptr(shared_future<T> &f)
{
#if 1
    // Thanks to Vicente for adding this to Boost.Thread
    return f.get_exception_ptr();
#else
    // This seems excessive but I don't see any other legal way to extract the exception ...
    bool success=false;
    try
    {
        f.get();
        success=true;
    }
    catch(...)
    {
        exception_ptr e(afio::make_exception_ptr(afio::current_exception()));
        assert(e);
        return e;
    }
    return exception_ptr();
#endif
}
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
#ifdef BOOST_NO_CXX11_DELETED_FUNCTIONS
        packaged_task(const packaged_task &) /* = delete */;
#else
        packaged_task(const packaged_task &) = delete;
#endif
    public:
        packaged_task() { }
        packaged_task(Base &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(std::move(o)) { }
        packaged_task(packaged_task &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(static_cast<Base &&>(o)) { }
        template<class T> packaged_task(T &&o) BOOST_NOEXCEPT_OR_NOTHROW : Base(std::forward<T>(o)) { }
        packaged_task &operator=(Base &&o) BOOST_NOEXCEPT_OR_NOTHROW{ static_cast<Base &&>(*this)=std::move(o); return *this; }
        packaged_task &operator=(packaged_task &&o) BOOST_NOEXCEPT_OR_NOTHROW{ static_cast<Base &&>(*this)=std::move(o); return *this; }
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
        atomic() : Base() {}
        BOOST_CONSTEXPR atomic(T v) BOOST_NOEXCEPT : Base(std::forward<T>(v)) {}

#ifdef BOOST_NO_CXX11_DELETED_FUNCTIONS
        //private:
        atomic(const Base &) /* =delete */;
#else
        atomic(const Base &) = delete;
#endif
    };//end boost::afio::atomic

    // Map in a chrono implementation
    namespace chrono {
        namespace detail {
            template<typename T> struct ratioToBase { typedef T type; };
        }
    }
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
            template<class Rep2, class Period2> BOOST_CONSTEXPR duration(const boost::chrono::duration<Rep2, Period2> &d) : Base(d) { }
#else
            template<class Rep2, class Period2> BOOST_CONSTEXPR duration(const std::chrono::duration<Rep2, Period2> &d) : Base(d) { }
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
        using boost::chrono::seconds;
        using boost::chrono::milliseconds;
        template <class ToDuration, class Rep, class Period> BOOST_CONSTEXPR ToDuration duration_cast(const boost::chrono::duration<Rep, Period> &d)
        {
            return boost::chrono::duration_cast<typename detail::durationToBase<ToDuration>::type, Rep, typename detail::ratioToBase<Period>::type>(d);
        }
#else
        using std::chrono::system_clock;
        using std::chrono::high_resolution_clock;
        using std::chrono::seconds;
        using std::chrono::milliseconds;
        template <class ToDuration, class Rep, class Period> BOOST_CONSTEXPR ToDuration duration_cast(const std::chrono::duration<Rep, Period> &d)
        {
            return std::chrono::duration_cast<typename detail::durationToBase<ToDuration>::type, Rep, Period>(d);
        }
#endif
    }

} }

#endif
