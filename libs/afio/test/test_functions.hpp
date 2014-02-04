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

#ifdef __MINGW32__
#include <stdlib.h> // To pull in __MINGW64_VERSION_MAJOR
#ifndef __MINGW64_VERSION_MAJOR
// Mingw32 doesn't define putenv() needed by Boost.Test
extern "C" int putenv(char*);
#endif
// Mingw doesn't define tzset() either
extern "C" void tzset(void);
#endif

#define _CRT_SECURE_NO_WARNINGS

#include <utility>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_set>
#include "boost/afio/afio.hpp"
#include "../detail/SpookyV2.h"
#include "boost/afio/detail/Aligned_Allocator.hpp"
#include "boost/afio/detail/MemoryTransactions.hpp"
#include "boost/afio/detail/valgrind/memcheck.h"
#include <time.h>

//if we're building the tests all together don't define the test main
#ifndef BOOST_AFIO_TEST_ALL
#    define BOOST_TEST_MAIN  //must be defined before unit_test.hpp is included
#endif
#ifndef BOOST_TEST_MODULE
#define BOOST_TEST_MODULE Boost.AFIO
#endif

#ifdef _MSC_VER
#pragma warning(disable: 4535) // calling _set_se_translator() requires /EHa
#endif

#include "boost/test/unit_test.hpp"
#include "boost/test/unit_test_monitor.hpp"

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
#ifdef BOOST_AFIO_USE_BOOST_ATOMIC
    BOOST_TEST_MESSAGE("NOTE: Using Boost for atomic<>");
#endif
#ifdef BOOST_AFIO_USE_BOOST_CHRONO
    BOOST_TEST_MESSAGE("NOTE: Using Boost for chrono");
#endif
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
    BOOST_TEST_MESSAGE("NOTE: Using current exception hack to work around ancient compiler");
#endif
}
#endif

// Boost.Test uses alarm() to timeout tests, which is nearly useless. Hence do our own.
static inline void watchdog_thread(size_t timeout)
{
    boost::afio::detail::set_threadname("watchdog_thread");
    bool docountdown=timeout>10;
    if(docountdown) timeout-=10;
    boost::chrono::duration<size_t> d(timeout);
    boost::mutex m;
    boost::condition_variable cv;
    boost::unique_lock<boost::mutex> lock(m);
    if(boost::cv_status::timeout==cv.wait_for(lock, d))
    {
        if(docountdown)
        {
            std::cerr << "Test about to time out ...";
            d=boost::chrono::duration<size_t>(1);
            for(size_t n=0; n<10; n++)
            {
                if(boost::cv_status::timeout!=cv.wait_for(lock, d))
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

template<class T> inline void wrap_test_method(T &t)
{
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
    // VS2010 segfaults if you ever do a try { throw; } catch(...) without an exception ever having been thrown :(
    try
    {
        throw boost::afio::detail::vs2010_lack_of_decent_current_exception_support_hack_t();
    }
    catch(...)
    {
        boost::afio::detail::vs2010_lack_of_decent_current_exception_support_hack()=boost::current_exception();
#endif
        try
        {
            t.test_method();
        }
        catch(...)
        {
            std::cerr << "ERROR: unit test exits via exception. Exception was " << boost::current_exception_diagnostic_information(true) << std::endl;
        }
#ifdef BOOST_AFIO_NEED_CURRENT_EXCEPTION_HACK
    }
#endif
}

// Define a unit test description and timeout
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
    /*boost::unit_test::unit_test_monitor_t::instance().p_timeout.set(timeout);*/ \
    BOOST_TEST_MESSAGE(desc);                                           \
    std::cout << std::endl << desc << std::endl;                        \
    try { boost::filesystem::remove_all("testdir"); } catch(...) { }    \
    set_maximum_cpus();                                                 \
    boost::thread watchdog(watchdog_thread, timeout);                   \
    boost::unit_test::unit_test_monitor_t::instance().execute([&]() -> int { wrap_test_method(t); watchdog.interrupt(); watchdog.join(); return 0; }); \
}                                                                       \
                                                                        \
struct BOOST_AUTO_TC_UNIQUE_ID( test_name ) {};                         \
                                                                        \
BOOST_AUTO_TU_REGISTRAR( test_name )(                                   \
    boost::unit_test::make_test_case(                                   \
        &BOOST_AUTO_TC_INVOKER( test_name ), #test_name ),              \
    boost::unit_test::ut_detail::auto_tc_exp_fail<                      \
        BOOST_AUTO_TC_UNIQUE_ID( test_name )>::instance()->value() );   \
                                                                        \
void test_name::test_method()                                           \

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

static int donothing(boost::afio::atomic<size_t> *callcount, int i) { ++*callcount; return i; }
static void _1000_open_write_close_deletes(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher, size_t bytes)
{
    try {
        using namespace boost::afio;
        using namespace std;
        using boost::afio::future;
        using boost::afio::ratio;
        namespace chrono = boost::afio::chrono;
        typedef chrono::duration<double, ratio<1>> secs_type;
        auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
        vector<char, boost::afio::detail::aligned_allocator<char, 4096>> towrite(bytes, 'N');
        assert(!(((size_t) &towrite.front()) & 4095));

        // Wait for six seconds to let filing system recover and prime SpeedStep
        auto begin=chrono::high_resolution_clock::now();
        while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<6);

        // Start opening 1000 files
        begin=chrono::high_resolution_clock::now();
        std::vector<async_path_op_req> manyfilereqs;
        manyfilereqs.reserve(1000);
        for(size_t n=0; n<1000; n++)
                manyfilereqs.push_back(async_path_op_req(mkdir, "testdir/"+boost::to_string(n), file_flags::Create|file_flags::Write));
        auto manyopenfiles(dispatcher->file(manyfilereqs));

        // Write to each of those 1000 files as they are opened
        std::vector<async_data_op_req<const char>> manyfilewrites;
        manyfilewrites.reserve(manyfilereqs.size());
        auto openit=manyopenfiles.begin();
        for(size_t n=0; n<manyfilereqs.size(); n++)
                manyfilewrites.push_back(async_data_op_req<const char>(*openit++, &towrite.front(), towrite.size(), 0));
        auto manywrittenfiles(dispatcher->write(manyfilewrites));

        // Close each of those 1000 files once one byte has been written
        auto manyclosedfiles(dispatcher->close(manywrittenfiles));

        // Delete each of those 1000 files once they are closed
        auto it(manyclosedfiles.begin());
        BOOST_FOREACH(auto &i, manyfilereqs)
                i.precondition=*it++;
        auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));

        // As a test of call() which involves significant template metaprogramming, have a do nothing callback
        boost::afio::atomic<size_t> callcount(0);
        typedef int (*callable_type)(boost::afio::atomic<size_t> *, int);
        callable_type callable=donothing;
        std::vector<std::function<int()>> callables;
        callables.reserve(1000);
        for(size_t n=0; n<1000; n++)
                callables.push_back(std::bind(callable, &callcount, 78));
        auto manycallbacks(dispatcher->call(manydeletedfiles, std::move(callables)));
        auto dispatched=chrono::high_resolution_clock::now();
        cout << "There are now " << dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;

        // Wait for all files to open
        when_all(manyopenfiles.begin(), manyopenfiles.end()).wait();
        auto openedsync=chrono::high_resolution_clock::now();
        // Wait for all files to write
        when_all(manywrittenfiles.begin(), manywrittenfiles.end()).wait();
        auto writtensync=chrono::high_resolution_clock::now();
        // Wait for all files to close
        when_all(manyclosedfiles.begin(), manyclosedfiles.end()).wait();
        auto closedsync=chrono::high_resolution_clock::now();
        // Wait for all files to delete
        when_all(manydeletedfiles.begin(), manydeletedfiles.end()).wait();
        auto deletedsync=chrono::high_resolution_clock::now();
        // Wait for all callbacks
        when_all(manycallbacks.second.begin(), manycallbacks.second.end()).wait();

        auto end=deletedsync;
        auto rmdir(dispatcher->rmdir(async_path_op_req("testdir")));

        auto diff=chrono::duration_cast<secs_type>(end-begin);
        cout << "It took " << diff.count() << " secs to do all operations" << endl;
        diff=chrono::duration_cast<secs_type>(dispatched-begin);
        cout << "  It took " << diff.count() << " secs to dispatch all operations" << endl;
        diff=chrono::duration_cast<secs_type>(end-dispatched);
        cout << "  It took " << diff.count() << " secs to finish all operations" << endl << endl;

        diff=chrono::duration_cast<secs_type>(openedsync-begin);
        cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file opens per sec" << endl;
        diff=chrono::duration_cast<secs_type>(writtensync-openedsync);
        cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file writes per sec" << endl;
        diff=chrono::duration_cast<secs_type>(closedsync-writtensync);
        cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file closes per sec" << endl;
        diff=chrono::duration_cast<secs_type>(deletedsync-closedsync);
        cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file deletions per sec" << endl;

        // Fetch any outstanding error
        when_all(rmdir).wait();
        BOOST_CHECK((callcount==1000U));
    } catch(...) {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        throw;
    }
}

#ifdef DEBUG_TORTURE_TEST
    struct Op
    {
            bool write;
            std::vector<char *> data;
            boost::afio::async_data_op_req<char> req;
    };
#else
    struct Op
    {
            Hash256 hash; // Only used for reading
            bool write;
            ranctx seed;
            async_data_op_req<char> req;
    };
#endif

#ifdef DEBUG_TORTURE_TEST
static u4 mkfill() { static char ret='0'; if(ret+1>'z') ret='0'; return ret++; }
#endif

static void evil_random_io(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher, size_t no, size_t bytes, size_t alignment=0)
{
    try {
    using namespace std;
    using boost::afio::future;
    using boost::afio::ratio;
    using boost::afio::off_t;
    namespace chrono = boost::afio::chrono;
    typedef chrono::duration<double, ratio<1>> secs_type;

    boost::afio::detail::aligned_allocator<char, 4096> aligned_allocator;
    vector<vector<char, boost::afio::detail::aligned_allocator<char, 4096>>> towrite(no);
    vector<char *> towriteptrs(no);
    vector<size_t> towritesizes(no);
    
#ifdef DEBUG_TORTURE_TEST
    std::vector<vector<Op>> todo(no);
#else
    static_assert(!(sizeof(PadSizeToMultipleOf<Op, 32>)&31), "Op's stored size must be a multiple of 32 bytes");
    vector<vector<PadSizeToMultipleOf<Op, 32>, boost::afio::detail::aligned_allocator<Op, 32>>> todo(no);
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
                            op.req.buffers.push_back(boost::asio::mutable_buffer(buffertouse, s));
                    }
#ifndef DEBUG_TORTURE_TEST
                    if(!op.write)
                    {
                            op.hash=Hash256();
                            op.hash.AddSHA256To((const char *)(((size_t)towriteptrs[n]+(size_t) op.req.where)), thisbytes);
                            //__sha256_osol((const char *)(((size_t)towriteptrs[n]+(size_t) op.req.where)), thisbytes);
#ifdef _DEBUG
                            cout << "<=SHA256 of " << thisbytes << " bytes at " << op.req.where << " is " << op.hash.asHexString() << endl;
#endif
                    }
#endif
#ifdef _DEBUG
                    // Quickly make sure none of these exceed 10Mb
                    off_t end=op.req.where;
                    BOOST_FOREACH(auto &b, op.req.buffers)
                            end+=boost::asio::buffer_size(b);
                    assert(end<=bytes);
#endif
                    todo[n].push_back(move(op));
                    bytessofar+=thisbytes;
            }
    }
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    cout << "It took " << diff.count() << " secs to simulate torture test in RAM" << endl;
    begin=chrono::high_resolution_clock::now(); // start timer for hashes
                
    // a vector to hold the hash values from SpookyHash
    //SpookyHash returns 2 64bit integers for a 128 bit hash, so we store them as a pair
    vector<std::pair<uint64, uint64>> memhashes(no);
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
    cout << "It took " << diff.count() << " secs to hash the results" << endl;
    for(size_t n=0; n<no; n++)
            memset(towriteptrs[n], 0, towritesizes[n]);

    auto mkdir(dispatcher->dir(boost::afio::async_path_op_req("testdir", boost::afio::file_flags::Create)));
    // Wait for three seconds to let filing system recover and prime SpeedStep
    //begin=chrono::high_resolution_clock::now();
    //while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);

    // Open our test files
    begin=chrono::high_resolution_clock::now();
    std::vector<boost::afio::async_path_op_req> manyfilereqs;
    manyfilereqs.reserve(no);
    for(size_t n=0; n<no; n++)
            manyfilereqs.push_back(boost::afio::async_path_op_req(mkdir, "testdir/"+boost::to_string(n), boost::afio::file_flags::Create|boost::afio::file_flags::ReadWrite));
    auto manyopenfiles(dispatcher->file(manyfilereqs));
    std::vector<off_t> sizes(no, bytes);
    auto manywrittenfiles(dispatcher->truncate(manyopenfiles, sizes));
#if defined(_DEBUG) && 0
    for(size_t n=0; n<manywrittenfiles.size(); n++)
            cout << n << ": " << manywrittenfiles[n].id << " (" << when_all(manywrittenfiles[n]).get().front()->path() << ") " << endl;
#endif
    // Schedule a replay of our in-RAM simulation

    boost::afio::detail::spinlock<size_t> failureslock;
    std::deque<std::pair<const Op *, size_t>> failures;
#if defined(BOOST_MSVC) && BOOST_MSVC < 1700 // <= VS2010
    std::function<std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>>(Op &, char *, size_t, boost::afio::async_io_op)> checkHash=[&failureslock, &failures](Op &op, char *base, size_t, boost::afio::async_io_op _h) -> std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>> {
#else
    auto checkHash=[&failureslock, &failures](Op &op, char *base, size_t, boost::afio::async_io_op _h) -> std::pair<bool, std::shared_ptr<boost::afio::async_io_handle>> {
#endif
            const char *data=(const char *)(((size_t) base+(size_t) op.req.where));
            size_t idxoffset=0;

            for(size_t m=0; m<op.req.buffers.size(); m++)
            {
                    const char *buffer=op.data[m];
                    size_t idx;
                    for(idx=0; idx < boost::asio::buffer_size(op.req.buffers[m]); idx++)
                    {
                            if(data[idx]!=buffer[idx])
                            {
                                    BOOST_BEGIN_MEMORY_TRANSACTION(failureslock)
                                    {
                                        failures.push_back(std::make_pair(&op, idxoffset+idx));
                                    }
                                    BOOST_END_MEMORY_TRANSACTION(failureslock)
#ifdef _DEBUG
                                    std::string contents(data, 8), shouldbe(buffer, 8);
                                    std::cout << "Contents of file at " << op.req.where << " +" << idx << " contains " << contents << " instead of " << shouldbe << std::endl;
#endif
                                    break;
                            }
                    }
                    if(idx!=boost::asio::buffer_size(op.req.buffers[m])) break;
                    data+=boost::asio::buffer_size(op.req.buffers[m]);
                    idxoffset+=boost::asio::buffer_size(op.req.buffers[m]);
            }
            return std::make_pair(true, _h.get());
    };
#pragma omp parallel for
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
    {
            BOOST_FOREACH(Op &op, todo[n])
            {
                    op.req.precondition=manywrittenfiles[n];
                    if(op.write)
                    {
                            manywrittenfiles[n]=dispatcher->write(op.req);
                    }
                    else
                        manywrittenfiles[n]=dispatcher->completion(dispatcher->read(op.req), std::pair<boost::afio::async_op_flags, std::function<boost::afio::async_file_io_dispatcher_base::completion_t>>(boost::afio::async_op_flags::none, std::bind(checkHash, ref(op), towriteptrs[n], placeholders::_1, placeholders::_2)));
            }
            // After replay, read the entire file into memory
            manywrittenfiles[n]=dispatcher->read(boost::afio::async_data_op_req<char>(manywrittenfiles[n], towriteptrs[n], towritesizes[n], 0));
    }
   
    // Close each of those files
    auto manyclosedfiles(dispatcher->close(manywrittenfiles));
    auto dispatched=chrono::high_resolution_clock::now();
    cout << "There are now " << dec << dispatcher->fd_count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;

    // Wait for all files to open
    when_all(manyopenfiles.begin(), manyopenfiles.end()).wait();
    auto openedsync=chrono::high_resolution_clock::now();
    // Wait for all files to write
    when_all(manywrittenfiles.begin(), manywrittenfiles.end()).wait();
    auto writtensync=chrono::high_resolution_clock::now();
    // Wait for all files to close
    when_all(manyclosedfiles.begin(), manyclosedfiles.end()).wait();
    auto closedsync=chrono::high_resolution_clock::now();
    end=closedsync;

    diff=chrono::duration_cast<secs_type>(end-begin);
    cout << "It took " << diff.count() << " secs to do all operations" << endl;
    diff=chrono::duration_cast<secs_type>(dispatched-begin);
    cout << "  It took " << diff.count() << " secs to dispatch all operations" << endl;
    diff=chrono::duration_cast<secs_type>(end-dispatched);
    cout << "  It took " << diff.count() << " secs to finish all operations" << endl << endl;
    off_t readed=0, written=0;
    size_t ops=0;
    diff=chrono::duration_cast<secs_type>(end-begin);
    BOOST_FOREACH(auto &i, manyclosedfiles)
    {
            readed+=when_all(i).get().front()->read_count();
            written+=when_all(i).get().front()->write_count();
    }
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
            ops+=todo[n].size();
    cout << "We read " << readed << " bytes and wrote " << written << " bytes during " << ops << " operations." << endl;
    cout << "  That makes " << (readed+written)/diff.count()/1024/1024 << " Mb/sec" << endl;

    diff=chrono::duration_cast<secs_type>(openedsync-begin);
    cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file opens per sec" << endl;
    diff=chrono::duration_cast<secs_type>(writtensync-openedsync);
    cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file reads and writes per sec" << endl;
    diff=chrono::duration_cast<secs_type>(closedsync-writtensync);
    cout << "It took " << diff.count() << " secs to do " << manyfilereqs.size()/diff.count() << " file closes per sec" << endl;

    BOOST_CHECK(failures.empty());
    if(!failures.empty())
    {
            cerr << "The following hash failures occurred:" << endl;
            while(!failures.empty())
            {
                pair<const Op *, size_t> &failedop=failures.front();
                size_t bytes=0;
                BOOST_FOREACH(auto &b, failedop.first->req.buffers)
                    bytes+=boost::asio::buffer_size(b);
                cerr << "   " << (failedop.first->write ? "Write to" : "Read from") << " " << boost::to_string(failedop.first->req.where) << " at offset " << failedop.second << " into bytes " << bytes << endl;
                failures.pop_front();
            }
    }
    BOOST_TEST_MESSAGE("Checking if the final files have exactly the right contents ... this may take a bit ...");
    cout << "Checking if the final files have exactly the right contents ... this may take a bit ..." << endl;
    {
        // a vector for holding hash results from SpookyHash
        //SpookyHash returns 2 64bit integers for a 128 bit hash, so we store them as a pair
        vector<std::pair<uint64, uint64>> filehashes(no);

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
                    string failmsg("File "+boost::to_string(n)+" contents were not what they were supposed to be!");
                    BOOST_TEST_MESSAGE(failmsg.c_str());
                    std::cerr << failmsg << std::endl;
            }
    }
#ifdef DEBUG_TORTURE_TEST
    for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
    {
            BOOST_FOREACH(Op &op, todo[n])
            {
                    BOOST_FOREACH(auto &i, op.data)
                            aligned_allocator.deallocate(i, 0);
            }
    }
#endif
    // Delete each of those files once they are closed
    auto it(manyclosedfiles.begin());
    BOOST_FOREACH(auto &i, manyfilereqs)
            i.precondition=*it++;
    auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));
    // Wait for all files to delete
    when_all(manydeletedfiles.begin(), manydeletedfiles.end()).wait();
    auto rmdir(dispatcher->rmdir(boost::afio::async_path_op_req("testdir")));
    // Fetch any outstanding error
    when_all(rmdir).wait();
    } catch(...) {
        std::cerr << boost::current_exception_diagnostic_information(true) << std::endl;
        throw;
    }
}


static std::ostream &operator<<(std::ostream &s, const boost::afio::chrono::system_clock::time_point &ts)
{
    char buf[32];
    struct tm *t;
    size_t len=sizeof(buf);
    size_t ret;
    time_t v=boost::afio::chrono::system_clock::to_time_t(ts);
    boost::afio::chrono::system_clock::duration remainder(ts-boost::afio::chrono::system_clock::from_time_t(v));

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

    sprintf(&buf[strlen(buf)], ".%09ld", (long) remainder.count());
    s << buf;

    return s;
}

static boost::afio::stat_t print_stat(std::shared_ptr<boost::afio::async_io_handle> h)
{
    using namespace boost::afio;
    auto entry=h->lstat(metadata_flags::All);
    std::cout << "Entry " << h->path() << " is a ";
    switch(entry.st_type)
    {
    case std::filesystem::file_type::symlink_file:
        std::cout << "link";
        break;
    case std::filesystem::file_type::directory_file:
        std::cout << "directory";
        break;
    case std::filesystem::file_type::regular_file:
        std::cout << "file";
        break;
    default:
        std::cout << "unknown";
        break;
    }
    std::cout << " and it has the following information:" << std::endl;
    if(std::filesystem::file_type::symlink_file==entry.st_type)
    {
        std::cout << "  Target=" << h->target() << std::endl;
    }
#define PRINT_FIELD(field) \
    std::cout << "  st_" #field ": "; if(!!(directory_entry::metadata_supported()&metadata_flags::field)) std::cout << entry.st_##field; else std::cout << "unknown"; std::cout << std::endl
#ifndef WIN32
    PRINT_FIELD(dev);
#endif
    PRINT_FIELD(ino);
    PRINT_FIELD(type);
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
#undef PRINT_FIELD
    return entry;
}

static void print_stat(std::shared_ptr<boost::afio::async_io_handle> dirh, boost::afio::directory_entry direntry)
{
    using namespace boost::afio;
    std::cout << "Entry " << direntry.name() << " is a ";
    auto entry=direntry.fetch_lstat(dirh);
    switch(entry.st_type)
    {
    case std::filesystem::file_type::symlink_file:
        std::cout << "link";
        break;
    case std::filesystem::file_type::directory_file:
        std::cout << "directory";
        break;
    case std::filesystem::file_type::regular_file:
        std::cout << "file";
        break;
    default:
        std::cout << "unknown";
        break;
    }
    std::cout << " and it has the following information:" << std::endl;

#define PRINT_FIELD(field) \
    std::cout << "  st_" #field ": "; if(!!(direntry.metadata_ready()&metadata_flags::field)) std::cout << entry.st_##field; else std::cout << "unknown"; std::cout << std::endl
#ifndef WIN32
    PRINT_FIELD(dev);
#endif
    PRINT_FIELD(ino);
    PRINT_FIELD(type);
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
#undef PRINT_FIELD
}

#endif
