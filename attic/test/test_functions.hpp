/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#ifndef BOOST_AFIO_TEST_FUNCTIONS_HPP
#define BOOST_AFIO_TEST_FUNCTIONS_HPP

// Uses a ton more memory and is many orders of magnitude slower, but no longer optional.
#define DEBUG_TORTURE_TEST 1

// Reduces CPU cores used to execute, which can be useful for certain kinds of race condition.
//#define MAXIMUM_TEST_CPUS 1

#define _CRT_SECURE_NO_WARNINGS

#include "boost/afio/afio.hpp"

#ifdef __MINGW32__
#include <stdlib.h> // To pull in __MINGW64_VERSION_MAJOR
#ifndef __MINGW64_VERSION_MAJOR
// Mingw32 doesn't define putenv() needed by Boost.Test
extern "C" int putenv(char*);
#endif
// Mingw doesn't define tzset() either
extern "C" void tzset(void);
#endif

#include <utility>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <deque>
#include <set>
#include <unordered_set>
#include <random>
#include <fstream>
#include "../detail/SpookyV2.h"
#include "Aligned_Allocator.hpp"
#include "boost/afio/v2/detail/valgrind/valgrind.h"
#include <time.h>

#ifdef BOOST_AFIO_INCLUDE_SPOOKY_IMPL
#include "../detail/SpookyV2.cpp"
#endif

#if defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define RUNNING_ON_SANITIZER 1
#  endif
#endif
#ifndef RUNNING_ON_SANITIZER
# define RUNNING_ON_SANITIZER 0
#endif

//if we're building the tests all together don't define the test main
#ifndef BOOST_AFIO_TEST_ALL
# define BOOST_TEST_MAIN  //must be defined before unit_test.hpp is included
#endif
#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE Boost.AFIO
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4535) // calling _set_se_translator() requires /EHa
#endif

#ifndef BOOST_AFIO_USE_BOOST_UNIT_TEST
# define BOOST_AFIO_USE_BOOST_UNIT_TEST 0
#endif
#if BOOST_AFIO_USE_BOOST_UNIT_TEST
# include "boost/test/unit_test.hpp"
# include "boost/test/unit_test_monitor.hpp"

//define a simple macro to check any exception using Boost.Test
#define BOOST_AFIO_CHECK_THROWS(expr)\
try{\
    expr;\
    BOOST_FAIL("Exception was not thrown");\
}catch(...){/*BOOST_CHECK(true);*/}
#define BOOST_AFIO_CHECK_NO_THROW(expr)\
try{\
    expr;\
    /*BOOST_CHECK(true);*/ \
}catch(...){BOOST_FAIL("Exception was thrown");}

#else
# include "../include/boost/afio/bindlib/include/boost/test/unit_test.hpp"
# define BOOST_AFIO_CHECK_THROWS(expr) BOOST_CHECK_THROWS(expr)
# define BOOST_AFIO_CHECK_NO_THROW(expr) BOOST_CHECK_NO_THROW(expr)
#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN

// Force maximum CPUs available to threads in this process, if available on this platform
#ifdef MAXIMUM_TEST_CPUS
static inline void set_maximum_cpus(size_t no=MAXIMUM_TEST_CPUS)
{
#ifdef WIN32
    DWORD_PTR mask=0;
    for(size_t n=0; n<no; n++)
        mask|=(size_t)1<<n;
    if(!SetProcessAffinityMask(GetCurrentProcess(), mask))
    {
        std::cerr << "ERROR: Failed to set process affinity mask due to reason " << GetLastError() << std::endl;
        abort();
    }
#endif
#ifdef __linux__
    cpu_set_t mask;
    CPU_ZERO(&mask);
    for(size_t n=0; n<no; n++)
        CPU_SET(n, &mask);
    sched_setaffinity(getpid(), sizeof(mask), &mask);
#endif
}
#else
static inline void set_maximum_cpus(size_t no=0)
{
}
#endif

// Boost.Test uses alarm() to timeout tests, which is nearly useless. Hence do our own.
static inline void watchdog_thread(size_t timeout, std::shared_ptr<std::pair<atomic<bool>, condition_variable>> cv)
{
    detail::set_threadname("watchdog_thread");
    if(getenv("BOOST_AFIO_TEST_DISABLE_WATCHDOG_TIMER"))
      return;
    bool docountdown=timeout>10;
    if(docountdown) timeout-=10;
    chrono::duration<size_t, ratio<1, 1>> d(timeout);
    mutex m;
    unique_lock<mutex> lock(m);
    if(!cv->second.wait_for(lock, d, [cv]{return !!cv->first;}))
    {
        if(docountdown)
        {
            std::cerr << "Test about to time out ...";
            d=chrono::duration<size_t, ratio<1, 1>>(1);
            for(size_t n=0; n<10; n++)
            {
                if(cv->second.wait_for(lock, d, [cv]{return !!cv->first;}))
                    return;
                std::cerr << " " << 10-n;
            }
            std::cerr << " ... ";
        }
        BOOST_CHECK_MESSAGE(false, "Test timed out");
        std::cerr << "Test timed out, aborting" << std::endl;
        abort();
    }
}

#define BOOST_AFIO_TRAP_EXCEPTIONS_IN_TEST(callable) \
  try { callable; } \
  catch(const BOOST_AFIO_V2_NAMESPACE::system_error &e) { std::cerr << "ERROR: unit test exits via system_error code " << e.code().value() << " (" << e.what() << ")" << std::endl; throw; } \
  catch(const std::exception &e) { std::cerr << "ERROR: unit test exits via exception (" << e.what() << ")" << std::endl; throw; }
#if BOOST_AFIO_USE_BOOST_UNIT_TEST
template<class T> inline void wrap_test_method(T &t)
{
  BOOST_AFIO_TRAP_EXCEPTIONS_IN_TEST(t.test_method())
  catch(const boost::execution_aborted &) { throw; }
  catch(...) { std::cerr << "ERROR: unit test exits via unknown exception" << std::endl; throw; }
}
#endif

BOOST_AFIO_V2_NAMESPACE_END

#if BOOST_AFIO_USE_BOOST_UNIT_TEST
// Define a unit test description and timeout
#ifdef BOOST_TEST_UNIT_TEST_SUITE_IMPL_HPP_071894GER
#define BOOST_AFIO_AUTO_TEST_CASE_REGISTRAR(test_name)                  \
BOOST_AUTO_TU_REGISTRAR( test_name )(                                   \
    boost::unit_test::make_test_case(                                   \
        &BOOST_AUTO_TC_INVOKER( test_name ), #test_name ),              \
    boost::unit_test::ut_detail::auto_tc_exp_fail<                      \
        BOOST_AUTO_TC_UNIQUE_ID( test_name )>::instance()->value() );   \
                                                                        \
void test_name::test_method()                                           \

#else
#define BOOST_AFIO_AUTO_TEST_CASE_REGISTRAR(test_name)                  \
BOOST_AUTO_TU_REGISTRAR( test_name )(                                   \
    boost::unit_test::make_test_case(                                   \
        &BOOST_AUTO_TC_INVOKER( test_name ),                            \
        #test_name, __FILE__, __LINE__ ),                               \
    boost::unit_test::decorator::collector::instance() );               \
                                                                        \
void test_name::test_method()                                           \

#endif

#define BOOST_AFIO_AUTO_TEST_CASE(test_name, desc, _timeout)            \
struct test_name : public BOOST_AUTO_TEST_CASE_FIXTURE { void test_method(); }; \
                                                                        \
static void BOOST_AUTO_TC_INVOKER( test_name )()                        \
{                                                                       \
    test_name t;                                                        \
    size_t timeout=_timeout;                                            \
    if(RUNNING_ON_VALGRIND) {                                           \
        VALGRIND_PRINTF("BOOST.AFIO TEST INVOKER: Unit test running in valgrind so tripling timeout\n"); \
        timeout*=3;                                                     \
    }                                                                   \
    if(RUNNING_ON_SANITIZER) {                                           \
        BOOST_TEST_MESSAGE("BOOST.AFIO TEST INVOKER: Unit test running in thread sanitiser so tripling timeout"); \
        timeout*=3;                                                     \
    }                                                                   \
    /*boost::unit_test::unit_test_monitor_t::instance().p_timeout.set(timeout);*/ \
    BOOST_TEST_MESSAGE(desc);                                           \
    std::cout << std::endl << desc << std::endl;                        \
    try { BOOST_AFIO_V2_NAMESPACE::filesystem::remove_all("testdir"); } catch(...) { } \
    BOOST_AFIO_V2_NAMESPACE::set_maximum_cpus();                                                 \
    auto cv=std::make_shared<std::pair<BOOST_AFIO_V2_NAMESPACE::atomic<bool>, BOOST_AFIO_V2_NAMESPACE::condition_variable>>(); \
    cv->first=false;                                                    \
    BOOST_AFIO_V2_NAMESPACE::thread watchdog(BOOST_AFIO_V2_NAMESPACE::watchdog_thread, timeout, cv);                 \
    boost::unit_test::unit_test_monitor_t::instance().execute([&]() -> int { BOOST_AFIO_V2_NAMESPACE::wrap_test_method(t); cv->first=true; cv->second.notify_all(); watchdog.join(); return 0; }); \
}                                                                       \
                                                                        \
struct BOOST_AUTO_TC_UNIQUE_ID( test_name ) {};                         \
                                                                        \
BOOST_AFIO_AUTO_TEST_CASE_REGISTRAR ( test_name )

#else

#define BOOST_AFIO_AUTO_TEST_CASE(__test_name, __desc, __timeout)       \
static void __test_name ## _impl();                                     \
CATCH_TEST_CASE(BOOST_CATCH_AUTO_TEST_CASE_NAME(__test_name), __desc)                                    \
{                                                                       \
    size_t timeout=__timeout;                                           \
    if(RUNNING_ON_VALGRIND) {                                           \
        VALGRIND_PRINTF("BOOST.AFIO TEST INVOKER: Unit test running in valgrind so tripling timeout\n"); \
        timeout*=3;                                                     \
    }                                                                   \
    if(RUNNING_ON_SANITIZER) {                                           \
        BOOST_TEST_MESSAGE("BOOST.AFIO TEST INVOKER: Unit test running in thread sanitiser so tripling timeout"); \
        timeout*=3;                                                     \
    }                                                                   \
    BOOST_TEST_MESSAGE(__desc);                                           \
    std::cout << std::endl << __desc << std::endl;                        \
    try { BOOST_AFIO_V2_NAMESPACE::filesystem::remove_all("testdir"); } catch(...) { } \
    BOOST_AFIO_V2_NAMESPACE::set_maximum_cpus();                                                 \
    auto cv=std::make_shared<std::pair<BOOST_AFIO_V2_NAMESPACE::atomic<bool>, BOOST_AFIO_V2_NAMESPACE::condition_variable>>(); \
    cv->first=false;                                                    \
    struct __deleter_t {                                                \
      std::shared_ptr<std::pair<BOOST_AFIO_V2_NAMESPACE::atomic<bool>, BOOST_AFIO_V2_NAMESPACE::condition_variable>> cv;     \
      BOOST_AFIO_V2_NAMESPACE::thread watchdog;                                             \
      __deleter_t(std::shared_ptr<std::pair<BOOST_AFIO_V2_NAMESPACE::atomic<bool>, BOOST_AFIO_V2_NAMESPACE::condition_variable>> _cv, \
        BOOST_AFIO_V2_NAMESPACE::thread &&_watchdog) : cv(std::move(_cv)), watchdog(std::move(_watchdog)) { } \
      ~__deleter_t() { cv->first=true; cv->second.notify_all(); watchdog.join(); } \
    } __deleter(cv, BOOST_AFIO_V2_NAMESPACE::thread(BOOST_AFIO_V2_NAMESPACE::watchdog_thread, timeout, cv));         \
    BOOST_AFIO_TRAP_EXCEPTIONS_IN_TEST(__test_name ## _impl())         \
    catch(...) { std::cerr << "ERROR: unit test exits via unknown exception" << std::endl; throw; } \
}                                                                       \
static void __test_name ## _impl()                                      \

#endif

BOOST_AFIO_V2_NAMESPACE_BEGIN

// From http://burtleburtle.net/bob/rand/smallprng.html
typedef unsigned int  u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
static u4 ranval(ranctx *x) {
    u4 e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

static void raninit(ranctx *x, u4 seed) {
    u4 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i = 0; i < 20; ++i) {
        (void) ranval(x);
    }
}

static void dofilter(atomic<size_t> *callcount, detail::OpType, future<> &) { ++*callcount; }
static void checkwrite(detail::OpType, handle *h, const detail::io_req_impl<true> &req, off_t offset, size_t idx, size_t no, const error_code &, size_t transferred)
{
    size_t amount=0;
    for(auto &i: req.buffers)
        amount+=asio::buffer_size(i);
    if(offset!=0)
        BOOST_CHECK(offset==0);
    if(transferred!=amount)
        BOOST_CHECK(transferred==amount);
}
static int donothing(atomic<size_t> *callcount, int i) { ++*callcount; return i; }
static void _1000_open_write_close_deletes(dispatcher_ptr dispatcher, size_t bytes)
{
        typedef chrono::duration<double, ratio<1, 1>> secs_type;
        auto mkdir(dispatcher->dir(path_req("testdir", file_flags::create)));
        std::vector<char, detail::aligned_allocator<char, 4096>> towrite(bytes, 'N');
        assert(!(((size_t) &towrite.front()) & 4095));

        // Wait for six seconds to let filing system recover and prime SpeedStep
        auto begin=chrono::high_resolution_clock::now();
        while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<6);

        // Install a file open filter
        dispatcher->post_op_filter_clear();
        atomic<size_t> filtercount(0);
        std::vector<std::pair<detail::OpType, std::function<dispatcher::filter_t>>> filters;
        filters.push_back(std::make_pair<detail::OpType, std::function<dispatcher::filter_t>>(detail::OpType::file, std::bind(dofilter, &filtercount, std::placeholders::_1, std::placeholders::_2)));
        dispatcher->post_op_filter(filters);

        // Install a write filter
        std::vector<std::pair<detail::OpType, std::function<dispatcher::filter_readwrite_t>>> rwfilters;
        rwfilters.push_back(std::make_pair(detail::OpType::Unknown, std::function<dispatcher::filter_readwrite_t>(checkwrite)));
        dispatcher->post_readwrite_filter(rwfilters);
        
        // Start opening 1000 files
        begin=chrono::high_resolution_clock::now();
        std::vector<path_req> manyfilereqs;
        manyfilereqs.reserve(1000);
        for(size_t n=0; n<1000; n++)
                manyfilereqs.push_back(path_req::relative(mkdir, to_string(n), file_flags::create|file_flags::write));
        auto manyopenfiles(dispatcher->file(manyfilereqs));

        // Write to each of those 1000 files as they are opened
        std::vector<io_req<const char>> manyfilewrites;
        manyfilewrites.reserve(manyfilereqs.size());
        auto openit=manyopenfiles.begin();
        for(size_t n=0; n<manyfilereqs.size(); n++)
                manyfilewrites.push_back(io_req<const char>(*openit++, &towrite.front(), towrite.size(), 0));
        auto manywrittenfiles(dispatcher->write(manyfilewrites));

        // Close each of those 1000 files once one byte has been written
        auto manyclosedfiles(dispatcher->close(manywrittenfiles));

        // Delete each of those 1000 files once they are closed
        auto it(manyclosedfiles.begin());
        for(auto &i: manyfilereqs)
                i.precondition=dispatcher->depends(*it++, mkdir);
        auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));

        // As a test of call() which involves significant template metaprogramming, have a do nothing callback
        atomic<size_t> callcount(0);
        typedef int (*callable_type)(atomic<size_t> *, int);
        callable_type callable=donothing;
        std::vector<std::function<int()>> callables;
        callables.reserve(1000);
        for(size_t n=0; n<1000; n++)
                callables.push_back(std::bind(callable, &callcount, 78));
        auto manycallbacks(dispatcher->call(manydeletedfiles, std::move(callables)));
        auto dispatched=chrono::high_resolution_clock::now();
        std::cout << "There are now " << std::dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << std::endl;

        // Wait for all files to open
        when_all_p(manyopenfiles.begin(), manyopenfiles.end()).get();
        auto openedsync=chrono::high_resolution_clock::now();
        // Wait for all files to write
        when_all_p(manywrittenfiles.begin(), manywrittenfiles.end()).get();
        auto writtensync=chrono::high_resolution_clock::now();
        // Wait for all files to close
        when_all_p(manyclosedfiles.begin(), manyclosedfiles.end()).get();
        auto closedsync=chrono::high_resolution_clock::now();
        // Wait for all files to delete
        when_all_p(manydeletedfiles.begin(), manydeletedfiles.end()).get();
        auto deletedsync=chrono::high_resolution_clock::now();
        // Wait for all callbacks
        when_all_p(manycallbacks.begin(), manycallbacks.end()).get();

        auto end=deletedsync;
        auto rmdir(dispatcher->rmdir(path_req("testdir")));

        auto diff=chrono::duration_cast<secs_type>(end-begin);
        std::cout << "It took " << diff.count() << " secs to do all operations" << std::endl;
        diff=chrono::duration_cast<secs_type>(dispatched-begin);
        std::cout << "  It took " << diff.count() << " secs to dispatch all operations" << std::endl;
        diff=chrono::duration_cast<secs_type>(end-dispatched);
        std::cout << "  It took " << diff.count() << " secs to finish all operations" << std::endl << std::endl;

        diff=chrono::duration_cast<secs_type>(openedsync-begin);
        std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file opens per sec" << std::endl;
        diff=chrono::duration_cast<secs_type>(writtensync-openedsync);
        std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file writes per sec" << std::endl;
        diff=chrono::duration_cast<secs_type>(closedsync-writtensync);
        std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file closes per sec" << std::endl;
        diff=chrono::duration_cast<secs_type>(deletedsync-closedsync);
        std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file deletions per sec" << std::endl;

        // Fetch any outstanding error
        when_all_p(rmdir).wait();
        BOOST_CHECK((callcount==1000U));
        BOOST_CHECK((filtercount==1000U));
}

#ifdef DEBUG_TORTURE_TEST
    struct Op
    {
            bool write;
            std::vector<char *> data;
            io_req<char> req;
    };
#else
    struct Op
    {
            Hash256 hash; // Only used for reading
            bool write;
            ranctx seed;
            io_req<char> req;
    };
#endif

#ifdef DEBUG_TORTURE_TEST
static u4 mkfill() { static char ret='0'; if(ret+1>'z') ret='0'; return ret++; }
#endif

static void evil_random_io(dispatcher_ptr dispatcher, size_t no, size_t bytes, size_t alignment=0)
{
    typedef chrono::duration<double, ratio<1, 1>> secs_type;

    detail::aligned_allocator<char, 4096> aligned_allocator;
    std::vector<std::vector<char, detail::aligned_allocator<char, 4096>>> towrite(no);
    std::vector<char *> towriteptrs(no);
    std::vector<size_t> towritesizes(no);
    
#ifdef DEBUG_TORTURE_TEST
    std::vector<std::vector<Op>> todo(no);
#else
    static_assert(!(sizeof(PadSizeToMultipleOf<Op, 32>)&31), "Op's stored size must be a multiple of 32 bytes");
    std::vector<std::vector<PadSizeToMultipleOf<Op, 32>, detail::aligned_allocator<Op, 32>>> todo(no);
#endif

    for(size_t n=0; n<no; n++)
    {
            towrite[n].reserve(bytes);
            towrite[n].resize(bytes);
            assert(!(((size_t) &towrite[n].front()) & 4095));
            towriteptrs[n]=&towrite[n].front();
            towritesizes[n]=bytes;
    }
    // We create no lots of random writes and reads representing about 100% of bytes
    // We simulate what we _ought_ to see appear in storage during the test and
    // SHA256 out the results
    // We then replay the same with real storage to see if it matches
    auto begin=chrono::high_resolution_clock::now();
#pragma omp parallel for
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
    {
            ranctx gen;
            raninit(&gen, 0x78adbcff^(u4) n);
            for(off_t bytessofar=0; bytessofar<bytes;)
            {
                    Op op;
                    u4 r=ranval(&gen), toissue=(r>>24) & 15;
                    size_t thisbytes=0, m;
                    if(!toissue) toissue=1;
                    op.write=bytessofar<bytes/4 ? true : !(r&(1<<31));
                    op.req.where=r & (bytes-1);
                    if(op.req.where>bytes-1024*1024) op.req.where=bytes-1024*1024;
                    if(alignment)
                            op.req.where&=~(alignment-1);
#ifdef DEBUG_TORTURE_TEST
                    u4 fillvalue=mkfill();
                    fillvalue|=fillvalue<<8;
                    fillvalue|=fillvalue<<16;
#else
                    ranctx writeseed=op.seed=gen;
#endif
                    for(m=0; m<toissue; m++)
                    {
                            u4 s=ranval(&gen) & ((256*1024-1)&~63); // Must be a multiple of 64 bytes for SHA256
                            if(s<64) s=64;
                            if(alignment)
                                    s=(s+4095)&~(alignment-1);
                            if(thisbytes+s>1024*1024) break;
                            char *buffertouse=(char *)((size_t)towriteptrs[n]+(size_t) op.req.where+thisbytes);
#ifdef DEBUG_TORTURE_TEST
                            op.data.push_back(aligned_allocator.allocate(s));
                            char *buffer=op.data.back();
#endif
                            if(op.write)
                            {
                                    for(size_t x=0; x<s; x+=4)
#ifndef DEBUG_TORTURE_TEST
                                            *(u4 *)(buffer+thisbytes+x)=ranval(&writeseed);
#else
                                            *(u4 *)(buffer+x)=fillvalue;
                                    memcpy((char *)((size_t)towriteptrs[n]+(size_t) op.req.where+thisbytes), buffer, s);
                                    buffertouse=buffer;
#endif
                            }
#ifdef DEBUG_TORTURE_TEST
                            else
                                    memcpy(buffer, (char *)((size_t)towriteptrs[n]+(size_t) op.req.where+thisbytes), s);
#endif
                            thisbytes+=s;
                            op.req.buffers.push_back(asio::mutable_buffer(buffertouse, s));
                    }
#ifndef DEBUG_TORTURE_TEST
                    if(!op.write)
                    {
                            op.hash=Hash256();
                            op.hash.AddSHA256To((const char *)(((size_t)towriteptrs[n]+(size_t) op.req.where)), thisbytes);
                            //__sha256_osol((const char *)(((size_t)towriteptrs[n]+(size_t) op.req.where)), thisbytes);
#ifdef _DEBUG
                            std::cout << "<=SHA256 of " << thisbytes << " bytes at " << op.req.where << " is " << op.hash.asHexString() << std::endl;
#endif
                    }
#endif
#ifdef _DEBUG
                    // Quickly make sure none of these exceed 10Mb
                    off_t end=op.req.where;
                    for(auto &b: op.req.buffers)
                            end+=asio::buffer_size(b);
                    assert(end<=bytes);
#endif
                    todo[n].push_back(std::move(op));
                    bytessofar+=thisbytes;
            }
    }
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to simulate torture test in RAM" << std::endl;
    begin=chrono::high_resolution_clock::now(); // start timer for hashes
                
    // a vector to hold the hash values from SpookyHash
    //SpookyHash returns 2 64bit integers for a 128 bit hash, so we store them as a pair
    std::vector<std::pair<uint64, uint64>> memhashes(no);
    //  variables to seed and return the hashed values
    uint64 hash1, hash2, seed;
    seed = 1; //initialize the seed value. Completely arbitrary, but it needs to remain consistent 
              
    for(size_t i = 0; i < no; ++i)
    {
        // set up seeds and a variables to store hash values
        hash1 = seed;
        hash2 = seed;
                    
        //hash the data from towriteptrs
        SpookyHash::Hash128(towriteptrs[i], towritesizes[i], &hash1, &hash2);
                    
        // store the hash values for this data in memhashes for later comparison
        memhashes[i]= std::make_pair(hash1, hash2);
                    
    }
                
    end=chrono::high_resolution_clock::now(); // end timer for hashes
    diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to hash the results" << std::endl;
    for(size_t n=0; n<no; n++)
            memset(towriteptrs[n], 0, towritesizes[n]);

    auto mkdir(dispatcher->dir(path_req("testdir", file_flags::create)));
    // Wait for three seconds to let filing system recover and prime SpeedStep
    //begin=chrono::high_resolution_clock::now();
    //while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);

    // Open our test files
    begin=chrono::high_resolution_clock::now();
    std::vector<path_req> manyfilereqs;
    manyfilereqs.reserve(no);
    for(size_t n=0; n<no; n++)
            manyfilereqs.push_back(path_req::relative(mkdir, to_string(n), file_flags::create|file_flags::read_write));
    auto manyopenfiles(dispatcher->file(manyfilereqs));
    std::vector<off_t> sizes(no, bytes);
    auto manywrittenfiles(dispatcher->truncate(manyopenfiles, sizes));
#if defined(_DEBUG) && 0
    for(size_t n=0; n<manywrittenfiles.size(); n++)
            std::cout << n << ": " << manywrittenfiles[n].id << " (" << when_all_p(manywrittenfiles[n]).get().front()->path() << ") " << std::endl;
#endif
    // Schedule a replay of our in-RAM simulation

    spinlock<size_t> failureslock;
    std::deque<std::pair<const Op *, size_t>> failures;
    auto checkHash=[&failureslock, &failures](Op &op, char *base, size_t, future<> _h) -> std::pair<bool, handle_ptr> {
            const char *data=(const char *)(((size_t) base+(size_t) op.req.where));
            size_t idxoffset=0;

            for(size_t m=0; m<op.req.buffers.size(); m++)
            {
                    const char *buffer=op.data[m];
                    size_t idx;
                    for(idx=0; idx < asio::buffer_size(op.req.buffers[m]); idx++)
                    {
                            if(data[idx]!=buffer[idx])
                            {
                                    {
                                        lock_guard<decltype(failureslock)> h(failureslock);
                                        failures.push_back(std::make_pair(&op, idxoffset+idx));
                                    }
#ifdef _DEBUG
                                    std::string contents(data, 8), shouldbe(buffer, 8);
                                    std::cout << "Contents of file at " << op.req.where << " +" << idx << " contains " << contents << " instead of " << shouldbe << std::endl;
#endif
                                    break;
                            }
                    }
                    if(idx!=asio::buffer_size(op.req.buffers[m])) break;
                    data+=asio::buffer_size(op.req.buffers[m]);
                    idxoffset+=asio::buffer_size(op.req.buffers[m]);
            }
            return std::make_pair(true, _h.get_handle());
    };
#pragma omp parallel for
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
    {
            for(Op &op: todo[n])
            {
                    op.req.precondition=manywrittenfiles[n];
                    if(op.write)
                    {
                            manywrittenfiles[n]=dispatcher->write(op.req);
                    }
                    else
                        manywrittenfiles[n]=dispatcher->completion(dispatcher->read(op.req), std::pair<async_op_flags, std::function<dispatcher::completion_t>>(async_op_flags::none, std::bind(checkHash, std::ref(op), towriteptrs[n], std::placeholders::_1, std::placeholders::_2)));
            }
            // After replay, read the entire file into memory
            manywrittenfiles[n]=dispatcher->read(io_req<char>(manywrittenfiles[n], towriteptrs[n], towritesizes[n], 0));
    }
   
    // Close each of those files
    auto manyclosedfiles(dispatcher->close(manywrittenfiles));
    auto dispatched=chrono::high_resolution_clock::now();
    std::cout << "There are now " << std::dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << std::endl;

    // Wait for all files to open
    when_all_p(manyopenfiles.begin(), manyopenfiles.end()).get();
    auto openedsync=chrono::high_resolution_clock::now();
    // Wait for all files to write
    when_all_p(manywrittenfiles.begin(), manywrittenfiles.end()).get();
    auto writtensync=chrono::high_resolution_clock::now();
    // Wait for all files to close
    when_all_p(manyclosedfiles.begin(), manyclosedfiles.end()).get();
    auto closedsync=chrono::high_resolution_clock::now();
    end=closedsync;

    diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "It took " << diff.count() << " secs to do all operations" << std::endl;
    diff=chrono::duration_cast<secs_type>(dispatched-begin);
    std::cout << "  It took " << diff.count() << " secs to dispatch all operations" << std::endl;
    diff=chrono::duration_cast<secs_type>(end-dispatched);
    std::cout << "  It took " << diff.count() << " secs to finish all operations" << std::endl << std::endl;
    off_t readed=0, written=0;
    size_t ops=0;
    diff=chrono::duration_cast<secs_type>(end-begin);
    for(auto &i: manyclosedfiles)
    {
            readed+=when_all_p(i).get().front()->read_count();
            written+=when_all_p(i).get().front()->write_count();
    }
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
            ops+=todo[n].size();
    std::cout << "We read " << readed << " bytes and wrote " << written << " bytes during " << ops << " operations." << std::endl;
    std::cout << "  That makes " << (readed+written)/diff.count()/1024/1024 << " Mb/sec" << std::endl;

    diff=chrono::duration_cast<secs_type>(openedsync-begin);
    std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file opens per sec" << std::endl;
    diff=chrono::duration_cast<secs_type>(writtensync-openedsync);
    std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file reads and writes per sec" << std::endl;
    diff=chrono::duration_cast<secs_type>(closedsync-writtensync);
    std::cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file closes per sec" << std::endl;

    BOOST_CHECK(failures.empty());
    if(!failures.empty())
    {
            std::cerr << "The following hash failures occurred:" << std::endl;
            while(!failures.empty())
            {
                std::pair<const Op *, size_t> &failedop=failures.front();
                size_t _bytes=0;
                for(auto &b: failedop.first->req.buffers)
                    _bytes+=asio::buffer_size(b);
                std::cerr << "   " << (failedop.first->write ? "Write to" : "Read from") << " " << to_string(failedop.first->req.where) << " at offset " << failedop.second << " into bytes " << _bytes << std::endl;
                failures.pop_front();
            }
    }
    BOOST_TEST_MESSAGE("Checking if the final files have exactly the right contents ... this may take a bit ...");
    std::cout << "Checking if the final files have exactly the right contents ... this may take a bit ..." << std::endl;
    {
        // a vector for holding hash results from SpookyHash
        //SpookyHash returns 2 64bit integers for a 128 bit hash, so we store them as a pair
        std::vector<std::pair<uint64, uint64>> filehashes(no);

        for(size_t i = 0; i < no; ++i)
        {
            // set up seeds and a variables to store hash values
            hash1 = seed;
            hash2 = seed; 
                        
            // hash the data from towriteptrs
            SpookyHash::Hash128(towriteptrs[i], towritesizes[i], &hash1, &hash2);

            // store the hash values for this data in filehashes for a later comparison
            filehashes[i]= std::make_pair(hash1, hash2);

        }
        for(size_t n=0; n<no; n++)
            if(memhashes[n]!=filehashes[n]) // compare hash values from ram and actual IO
            {
                    std::string failmsg("File "+to_string(n)+" contents were not what they were supposed to be!");
                    BOOST_TEST_MESSAGE(failmsg.c_str());
                    std::cerr << failmsg << std::endl;
            }
    }
#ifdef DEBUG_TORTURE_TEST
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
    {
            for(Op &op: todo[n])
            {
                    for(auto &i: op.data)
                            aligned_allocator.deallocate(i, 0);
            }
    }
#endif
    // Delete each of those files once they are closed
    auto it(manyclosedfiles.begin());
    for(auto &i: manyfilereqs)
            i.precondition=dispatcher->depends(*it++, mkdir);
    auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));
    // Wait for all files to delete
    when_all_p(manydeletedfiles.begin(), manydeletedfiles.end()).get();
    auto rmdir(dispatcher->rmdir(path_req("testdir")));
    // Fetch any outstanding error
    when_all_p(rmdir).get();
}


static std::ostream &operator<<(std::ostream &s, const chrono::system_clock::time_point &ts)
{
    char buf[64];
    struct tm *t;
    size_t len=sizeof(buf);
    size_t ret;
    time_t v=chrono::system_clock::to_time_t(ts);
    chrono::system_clock::duration remainder(ts-chrono::system_clock::from_time_t(v));

#ifdef _MSC_VER
    _tzset();
#else
    tzset();
#endif
    if ((t=localtime(&v)) == NULL)
    {
        s << "<bad timespec>";
        return s;
    }

    ret = strftime(buf, len, "%Y-%m-%d %H:%M:%S", t);
    if (ret == 0)
    {
        s << "<bad timespec>";
        return s;
    }
    //len -= ret - 1;

    size_t end=strlen(buf);
    sprintf(&buf[end], "%f", remainder.count()/((double) chrono::system_clock::period::den / chrono::system_clock::period::num));
    memmove(&buf[end], &buf[end+1], strlen(&buf[end]));
    s << buf;

    return s;
}

static stat_t print_stat(handle_ptr h)
{
    using namespace boost::afio;
    auto entry=h->lstat(metadata_flags::All);
    std::cout << "Entry " << h->path(true) << " is a ";
    switch(entry.st_type)
    {
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
    case filesystem::file_type::symlink_file:
        std::cout << "link";
        break;
    case filesystem::file_type::directory_file:
        std::cout << "directory";
        break;
    case filesystem::file_type::regular_file:
        std::cout << "file";
        break;
    default:
        std::cout << "unknown";
        break;
#else
    case filesystem::file_type::symlink:
        std::cout << "link";
        break;
    case filesystem::file_type::directory:
        std::cout << "directory";
        break;
    case filesystem::file_type::regular:
        std::cout << "file";
        break;
    default:
        std::cout << "unknown";
        break;
#endif
    }
    std::cout << " and it has the following information:" << std::endl;
    if(!h->path().empty())
    {
      std::cout << "  Normalised path: " << normalise_path(h->path()) << std::endl;
      std::cout << "  Normalised path with volume GUID: " << normalise_path(h->path(), path_normalise::guid_volume) << std::endl;
      std::cout << "  Normalised path as GUID: ";
      try
      {
        std::cout << normalise_path(h->path(), path_normalise::guid_all) << std::endl;
      }
      catch (...)
      {
        std::cout << "EXCEPTION THROWN!" << std::endl;
      }
      }
    if(
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
      filesystem::file_type::symlink_file
#else
      filesystem::file_type::symlink
#endif
      ==entry.st_type)
    {
        std::cout << "  Target=" << h->target() << std::endl;
    }
#define PRINT_FIELD(field, ...) \
    std::cout << "  st_" #field ": "; if(!!(directory_entry::metadata_supported()&metadata_flags::field)) std::cout << __VA_ARGS__ entry.st_##field; else std::cout << "unknown"; std::cout << std::endl
#ifndef WIN32
    PRINT_FIELD(dev);
#endif
    PRINT_FIELD(ino);
    PRINT_FIELD(type, (int));
#ifndef WIN32
    PRINT_FIELD(perms);
#endif
    PRINT_FIELD(nlink);
#ifndef WIN32
    PRINT_FIELD(uid);
    PRINT_FIELD(gid);
    PRINT_FIELD(rdev);
#endif
    PRINT_FIELD(atim);
    PRINT_FIELD(mtim);
    PRINT_FIELD(ctim);
    PRINT_FIELD(size);
    PRINT_FIELD(allocated);
    PRINT_FIELD(blocks);
    PRINT_FIELD(blksize);
    PRINT_FIELD(flags);
    PRINT_FIELD(gen);
    PRINT_FIELD(birthtim);
    PRINT_FIELD(sparse);
    PRINT_FIELD(compressed);
    PRINT_FIELD(reparse_point);
#undef PRINT_FIELD
    return entry;
}

static void print_stat(handle_ptr dirh, directory_entry direntry)
{
    using namespace boost::afio;
    std::cout << "Entry " << direntry.name() << " is a ";
    auto entry=direntry.fetch_lstat(dirh);
    switch(entry.st_type)
    {
#ifdef BOOST_AFIO_USE_LEGACY_FILESYSTEM_SEMANTICS
    case filesystem::file_type::symlink_file:
        std::cout << "link";
        break;
    case filesystem::file_type::directory_file:
        std::cout << "directory";
        break;
    case filesystem::file_type::regular_file:
        std::cout << "file";
        break;
    default:
        std::cout << "unknown";
        break;
#else
    case filesystem::file_type::symlink:
        std::cout << "link";
        break;
    case filesystem::file_type::directory:
        std::cout << "directory";
        break;
    case filesystem::file_type::regular:
        std::cout << "file";
        break;
    default:
        std::cout << "unknown";
        break;
#endif
    }
    std::cout << " and it has the following information:" << std::endl;

#define PRINT_FIELD(field, ...) \
    std::cout << "  st_" #field ": "; if(!!(direntry.metadata_ready()&metadata_flags::field)) std::cout << __VA_ARGS__ entry.st_##field; else std::cout << "unknown"; std::cout << std::endl
#ifndef WIN32
    PRINT_FIELD(dev);
#endif
    PRINT_FIELD(ino);
    PRINT_FIELD(type, (int));
#ifndef WIN32
    PRINT_FIELD(perms);
#endif
    PRINT_FIELD(nlink);
#ifndef WIN32
    PRINT_FIELD(uid);
    PRINT_FIELD(gid);
    PRINT_FIELD(rdev);
#endif
    PRINT_FIELD(atim);
    PRINT_FIELD(mtim);
    PRINT_FIELD(ctim);
    PRINT_FIELD(size);
    PRINT_FIELD(allocated);
    PRINT_FIELD(blocks);
    PRINT_FIELD(blksize);
    PRINT_FIELD(flags);
    PRINT_FIELD(gen);
    PRINT_FIELD(birthtim);
    PRINT_FIELD(sparse);
    PRINT_FIELD(compressed);
    PRINT_FIELD(reparse_point);
#undef PRINT_FIELD
}

BOOST_AFIO_V2_NAMESPACE_END

#endif
