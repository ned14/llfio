/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

// If defined, uses a ton more memory and is many orders of magnitude slower.
#define DEBUG_TORTURE_TEST 1

// Get Boost.ASIO on Windows IOCP working
#if defined(_DEBUG) && defined(WIN32)
#define BOOST_ASIO_BUG_WORKAROUND 1
#endif

#ifdef __MINGW32__
// Mingw doesn't define putenv() needed by Boost.Test
extern int putenv(char*);
#endif

#include <utility>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "../../../boost/afio/afio.hpp"
#include "../../../boost/afio/detail/SpookyV2.h"
#include "../../../boost/afio/detail/Aligned_Allocator.hpp"
#include "boost/lockfree/queue.hpp"
#include "../../../boost/afio/detail/Undoer.hpp"


#define BOOST_TEST_MODULE tester
#include <boost/test/included/unit_test.hpp>

#define BOOST_AFIO_CHECK_THROWS(expr)\
try{\
    expr;\
    BOOST_FAIL("Exception was not thrown");\
}catch(...){BOOST_CHECK(true);}



// From http://burtleburtle.net/bob/rand/smallprng.html
typedef unsigned int  u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
u4 ranval( ranctx *x ) {
    u4 e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void raninit( ranctx *x, u4 seed ) {
    u4 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i=0; i<20; ++i) {
        (void)ranval(x);
    }
}

BOOST_AUTO_TEST_SUITE(all)
    BOOST_AUTO_TEST_SUITE(exclude_async_io_errors)
       
        static int task()
        {
                boost::afio::thread::id this_id = boost::afio::get_this_thread_id();
                std::cout << "I am worker thread " << this_id << std::endl;
                return 78;
        }
        BOOST_AUTO_TEST_CASE(async_io_thread_pool_works)
        {
            BOOST_TEST_MESSAGE("Tests that the async i/o thread pool implementation works");
                using namespace boost::afio;
                boost::afio::thread::id this_id = boost::afio::get_this_thread_id();
                std::cout << "I am main thread " << this_id << std::endl;
                thread_pool pool(4);
                auto r=task();
                BOOST_CHECK(r==78);
                std::vector<future<int>> results(8);
                for(auto &i : results)
                {
                        i=std::move(pool.enqueue(task));
                }
                std::vector<future<int>> results2;
                results2.push_back(pool.enqueue(task));
                results2.push_back(pool.enqueue(task));
                std::pair<size_t, int> allresults2=when_any(results2.begin(), results2.end()).get();
                BOOST_CHECK(allresults2.first<2);
                BOOST_CHECK(allresults2.second==78);
                std::vector<int> allresults=when_all(results.begin(), results.end()).get();
                for(int i : allresults)
                {
                        BOOST_CHECK(i==78);
                }
        }


        static void _1000_open_write_close_deletes(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher, size_t bytes)
        {
                using namespace boost::afio;
                using namespace std;
                using boost::afio::future;
                typedef std::chrono::duration<double, ratio<1>> secs_type;
                auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
                vector<char, boost::afio::detail::aligned_allocator<char, 4096>> towrite(bytes, 'N');
                assert(!(((size_t) &towrite.front()) & 4095));

                // Wait for six seconds to let filing system recover and prime SpeedStep
                auto begin=std::chrono::high_resolution_clock::now();
                while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<6);

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
                for(auto &i : manyfilereqs)
                        i.precondition=*it++;
                auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));

                // As a test of call() which involves significant template metaprogramming, have a do nothing callback
                std::atomic<size_t> callcount(0);
                typedef int (*callable_type)(std::atomic<size_t> *, int);
                callable_type callable=[](std::atomic<size_t> *callcount, int i) { ++*callcount; return i; };
                std::vector<std::function<int()>> callables;
                callables.reserve(1000);
                for(size_t n=0; n<1000; n++)
                        callables.push_back(std::bind(callable, &callcount, 78));
                auto manycallbacks(dispatcher->call(manydeletedfiles, std::move(callables)));
                auto dispatched=chrono::high_resolution_clock::now();
                cout << "There are now " << dec << dispatcher->count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;

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
                rmdir.h->get();
                BOOST_CHECK((callcount==1000U));
        }

        BOOST_AUTO_TEST_CASE(async_io_works_1prime)
        {
            BOOST_TEST_MESSAGE( "Tests that the async i/o implementation works (primes system)");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::None);
                std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes (primes system):\n";
                _1000_open_write_close_deletes(dispatcher, 1);
        }

        BOOST_AUTO_TEST_CASE(async_io_works_1) 
        {
            BOOST_TEST_MESSAGE("Tests that the async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::None);
                std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes:\n";
                _1000_open_write_close_deletes(dispatcher, 1);
        }

        BOOST_AUTO_TEST_CASE(async_io_works_64)
        {
            BOOST_TEST_MESSAGE( "Tests that the async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::None);
                std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes:\n";
                _1000_open_write_close_deletes(dispatcher, 65536);
        }

        BOOST_AUTO_TEST_CASE(async_io_works_1_sync) 
        {
            BOOST_TEST_MESSAGE("Tests that the synchronous async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSSync);
                std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes with synchronous i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 1);
        }

        BOOST_AUTO_TEST_CASE(async_io_works_64_sync)
        {
            BOOST_TEST_MESSAGE("Tests that the synchronous async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSSync);
                std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 65536);
        }

        BOOST_AUTO_TEST_CASE(async_io_works_1_autoflush)
        {
            BOOST_TEST_MESSAGE("Tests that the autoflush async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AutoFlush);
                std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes with autoflush i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 1);
        }

        BOOST_AUTO_TEST_CASE(async_io_works_64_autoflush)
        {
            BOOST_TEST_MESSAGE("Tests that the autoflush async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AutoFlush);
                std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with autoflush i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 65536);
        }

        #if 0
        BOOST_AUTO_TEST_CASE(async_io_works_1_direct)
        {
            BOOST_TEST_MESSAGE( "Tests that the direct async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect);
                std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes with direct i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 1);
        }
        #endif

        BOOST_AUTO_TEST_CASE(async_io_works_64_direct)
        {
            BOOST_TEST_MESSAGE( "Tests that the direct async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect);
                std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with direct i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 65536);
        }

        #if 0
        BOOST_AUTO_TEST_CASE(async_io_works_1_directsync)
        {
            BOOST_TEST_MESSAGE( "Tests that the direct synchronous async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect|boost::afio::file_flags::OSSync);
                std::cout << "\n\n1000 file opens, writes 1 byte, closes, and deletes with direct synchronous i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 1);
        }
        #endif

        BOOST_AUTO_TEST_CASE(async_io_works_64_directsync) 
        {
            BOOST_TEST_MESSAGE("Tests that the direct synchronous async i/o implementation works");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect|boost::afio::file_flags::OSSync);
                std::cout << "\n\n1000 file opens, writes 64Kb, closes, and deletes with direct synchronous i/o:\n";
                _1000_open_write_close_deletes(dispatcher, 65536);
        }

        BOOST_AUTO_TEST_CASE(async_io_barrier)
        {
            BOOST_TEST_MESSAGE( "Tests that the async i/o barrier works correctly under load");
                using namespace boost::afio;
                using namespace std;
                using boost::afio::future;
                using namespace boost::afio::detail;
                using boost::afio::off_t;
                typedef std::chrono::duration<double, ratio<1>> secs_type;
                vector<pair<size_t, int>> groups;
                // Generate 100,000 sorted random numbers between 0-1000
                {
                        ranctx gen;
                        raninit(&gen, 0x78adbcff);
                        vector<int> manynumbers;
                        manynumbers.reserve(100000);
                        for(size_t n=0; n<100000; n++)
                                manynumbers.push_back(ranval(&gen) % 1000);
                        sort(manynumbers.begin(), manynumbers.end());

                        // Collapse into a collection of runs of the same number
                        int lastnumber=-1;
                        for(auto &i : manynumbers)
                        {
                                if(i!=lastnumber)
                                        groups.push_back(make_pair(0, i));
                                groups.back().first++;
                                lastnumber=i;
                        }
                }
                atomic<size_t> callcount[1000];
                memset(&callcount, 0, sizeof(callcount));
                vector<future<bool>> verifies;
                verifies.reserve(groups.size());
                auto inccount=[](atomic<size_t> *count){ for(volatile size_t n=0; n<10000; n++); (*count)++; };
                auto verifybarrier=[](atomic<size_t> *count, size_t shouldbe)
                {
                        if(*count!=shouldbe)
                        {
                                BOOST_CHECK((*count==shouldbe));
                                throw runtime_error("Count was not what it should have been!");
                        }
                        return true;
                };
                // For each of those runs, dispatch ops and a barrier for them
                auto dispatcher=async_file_io_dispatcher();
                auto begin=std::chrono::high_resolution_clock::now();
                size_t opscount=0;
                async_io_op next;
                bool isfirst=true;
                for(auto &run : groups)
                {
                        vector<function<void()>> thisgroupcalls(run.first, bind(inccount, &callcount[run.second]));
                        vector<async_io_op> thisgroupcallops;
                        if(isfirst)
                        {
                                thisgroupcallops=dispatcher->call(thisgroupcalls).second;
                                isfirst=false;
                        }
                        else
                        {
                                vector<async_io_op> dependency(run.first, next);
                                thisgroupcallops=dispatcher->call(dependency, thisgroupcalls).second;
                        }
                        auto thisgroupbarriered=dispatcher->barrier(thisgroupcallops);
                        auto verify=dispatcher->call(thisgroupbarriered.front(), std::function<bool()>(std::bind(verifybarrier, &callcount[run.second], run.first)));
                        verifies.push_back(std::move(verify.first));
                        next=verify.second;
                        opscount+=run.first+2;
                }
                auto dispatched=chrono::high_resolution_clock::now();
                cout << "There are now " << dec << dispatcher->count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;
                BOOST_CHECK_NO_THROW(when_all(next).wait());
                // Retrieve any errors
                for(auto &i : verifies)
                {
                        BOOST_CHECK_NO_THROW(i.get());
                }
                auto end=std::chrono::high_resolution_clock::now();
                auto diff=chrono::duration_cast<secs_type>(end-begin);
                cout << "It took " << diff.count() << " secs to do " << opscount << " operations" << endl;
                diff=chrono::duration_cast<secs_type>(dispatched-begin);
                cout << "  It took " << diff.count() << " secs to dispatch all operations" << endl;
                diff=chrono::duration_cast<secs_type>(end-dispatched);
                cout << "  It took " << diff.count() << " secs to finish all operations" << endl << endl;
                diff=chrono::duration_cast<secs_type>(end-begin);
                cout << "That's a throughput of " << opscount/diff.count() << " ops/sec" << endl;
        }
        BOOST_AUTO_TEST_SUITE_END()// exclude_async_io_errors

    BOOST_AUTO_TEST_CASE(async_io_errors)
    {
        BOOST_MESSAGE( "Tests that the async i/o error handling works");
            using namespace boost::afio;
            using namespace std;
            using boost::afio::future;

            {
                    int hasErrorDirectly, hasErrorFromBarrier;
                    auto dispatcher=async_file_io_dispatcher();
                    auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
                    vector<async_path_op_req> filereqs;
                    filereqs.push_back(async_path_op_req(mkdir, "testdir/a", file_flags::CreateOnlyIfNotExist));
                    filereqs.push_back(async_path_op_req(mkdir, "testdir/a", file_flags::CreateOnlyIfNotExist));
                    {
                            auto manyfilecreates=dispatcher->file(filereqs); // One of these will error
                            auto sync1=dispatcher->barrier(manyfilecreates); // If barrier() doesn't throw due to errored input, barrier() will replicate errors for you
                            BOOST_CHECK_NO_THROW(when_all(std::nothrow_t(), sync1.begin(), sync1.end()).wait()); // nothrow variant must never throw
                            hasErrorDirectly=0;
                            for(auto &i : manyfilecreates)
                            {
                                    // If we ask for has_exception() before the async thread has exited its packaged_task
                                    // this will fail, so no choice but to try { wait(); } catch { success }
                                    try
                                    {
                                            i.h->get();
                                    }
                                    catch(...)
                                    {
                                            hasErrorDirectly++;
                                    }
                            }
                            BOOST_CHECK(hasErrorDirectly==1);
                            hasErrorFromBarrier=0;
                            for(auto &i : sync1)
                            {
                                    try
                                    {
                                            i.h->get();
                                    }
                                    catch(...)
                                    {
                                            hasErrorFromBarrier++;
                                    }
                            }
                            BOOST_CHECK(hasErrorFromBarrier==1);
                            BOOST_AFIO_CHECK_THROWS(when_all(sync1.begin(), sync1.end()).wait()); // throw variant must always throw
                    }

                    auto manyfiledeletes=dispatcher->rmfile(filereqs); // One of these will also error. Same as above.
                    auto sync2=dispatcher->barrier(manyfiledeletes);
                    BOOST_CHECK_NO_THROW(when_all(std::nothrow_t(), sync2.begin(), sync2.end()).wait());
                    hasErrorDirectly=0;
                    for(auto &i : manyfiledeletes)
                    {
                            try
                            {
                                    i.h->get();
                            }
                            catch(...)
                            {
                                    hasErrorDirectly++;
                            }
                    }
                    BOOST_CHECK(hasErrorDirectly==1);
                    hasErrorFromBarrier=0;
                    for(auto &i : sync2)
                    {
                            try
                            {
                                    i.h->get();
                            }
                            catch(...)
                            {
                                    hasErrorFromBarrier++;
                            }
                    }
                    BOOST_CHECK(hasErrorFromBarrier==1);

                    BOOST_AFIO_CHECK_THROWS(when_all(sync2.begin(), sync2.end()).wait());
                    auto rmdir=dispatcher->rmdir(async_path_op_req("testdir"));
            }
    }

    BOOST_AUTO_TEST_SUITE(exclude_async_io_errors)// restart exclude_aysnc_io_errors

        static void evil_random_io(std::shared_ptr<boost::afio::async_file_io_dispatcher_base> dispatcher, size_t no, size_t bytes, size_t alignment=0)
        {
                using namespace boost::afio;
                using namespace std;
                using boost::afio::future;
                using namespace boost::afio::detail;
                using boost::afio::off_t;
                typedef std::chrono::duration<double, ratio<1>> secs_type;

                boost::afio::detail::aligned_allocator<char, 4096> aligned_allocator;
                vector<vector<char, boost::afio::detail::aligned_allocator<char, 4096>>> towrite(no);
                vector<char *> towriteptrs(no);
                vector<size_t> towritesizes(no);
        #ifdef DEBUG_TORTURE_TEST
                struct Op
                {
                        bool write;
                        vector<char *> data;
                        async_data_op_req<char> req;
                };
                vector<vector<Op>> todo(no);
        #else
                struct Op
                {
                        Hash256 hash; // Only used for reading
                        bool write;
                        ranctx seed;
                        async_data_op_req<char> req;
                };
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
        #ifdef DEBUG_TORTURE_TEST
                auto mkfill=[]{ static char ret='0'; if(ret+1>'z') ret='0'; return ret++; };
        #endif
                // We create no lots of random writes and reads representing about 100% of bytes
                // We simulate what we _ought_ to see appear in storage during the test and
                // SHA256 out the results
                // We then replay the same with real storage to see if it matches
                auto begin=std::chrono::high_resolution_clock::now();
        #ifndef BOOST_ASIO_BUG_WORKAROUND
        #pragma omp parallel for
        #endif
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
        #ifdef BOOST_ASIO_BUG_WORKAROUND
                                //if(toissue>4) toissue=4;
                                toissue=1; // clamp for now. I think Boost.ASIO on Win IOCP seems to dislike more than one buffer at a time ?!?
        #endif
                                for(m=0; m<toissue; m++)
                                {
                                        u4 s=ranval(&gen) & ((256*1024-1)&~63); // Must be a multiple of 64 bytes for SHA256
                                        if(s<64) s=64;
        #ifdef BOOST_ASIO_BUG_WORKAROUND
                                        if(s>65536) s=65536; // clamp for now. I think Boost.ASIO won't transfer more than 64Kb at a time anyway ... ?!?
        #endif
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
                                for(auto &b : op.req.buffers)
                                        end+=boost::asio::buffer_size(b);
                                assert(end<=bytes);
        #endif
                                todo[n].push_back(move(op));
                                bytessofar+=thisbytes;
                        }
                }
                auto end=std::chrono::high_resolution_clock::now();
                auto diff=chrono::duration_cast<secs_type>(end-begin);
                cout << "It took " << diff.count() << " secs to simulate torture test in RAM" << endl;
                begin=std::chrono::high_resolution_clock::now(); // start timer for hashes
                
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
                
                end=std::chrono::high_resolution_clock::now(); // end timer for hashes
                diff=chrono::duration_cast<secs_type>(end-begin);
                cout << "It took " << diff.count() << " secs to hash the results" << endl;
                for(size_t n=0; n<no; n++)
                        memset(towriteptrs[n], 0, towritesizes[n]);

                auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
                // Wait for three seconds to let filing system recover and prime SpeedStep
                //begin=std::chrono::high_resolution_clock::now();
                //while(std::chrono::duration_cast<secs_type>(std::chrono::high_resolution_clock::now()-begin).count()<3);

                // Open our test files
                begin=std::chrono::high_resolution_clock::now();
                std::vector<async_path_op_req> manyfilereqs;
                manyfilereqs.reserve(no);
                for(size_t n=0; n<no; n++)
                        manyfilereqs.push_back(async_path_op_req(mkdir, "testdir/"+boost::to_string(n), file_flags::Create|file_flags::ReadWrite));
                auto manyopenfiles(dispatcher->file(manyfilereqs));
                std::vector<off_t> sizes(no, bytes);
                auto manywrittenfiles(dispatcher->truncate(manyopenfiles, sizes));
        #if defined(_DEBUG) && 0
                for(size_t n=0; n<manywrittenfiles.size(); n++)
                        cout << n << ": " << manywrittenfiles[n].id << " (" << manywrittenfiles[n].h->get()->path() << ") " << endl;
        #endif

                // Schedule a replay of our in-RAM simulation
                size_t maxfailures=0;
                for(size_t n=0; n<no; n++)
                        maxfailures+=todo[n].size();
                boost::lockfree::queue<pair<const Op *, size_t> *> failures(maxfailures);
                auto checkHash=[&failures](Op &op, char *base, size_t, std::shared_ptr<boost::afio::detail::async_io_handle> h) -> std::pair<bool, std::shared_ptr<boost::afio::detail::async_io_handle>> {
                        const char *data=(const char *)(((size_t) base+(size_t) op.req.where));
                        size_t idxoffset=0;
                        for(size_t m=0; m<op.req.buffers.size(); m++)
                        {
                                const char *buffer=op.data[m];
                                size_t idx;
                                for(idx=0; idx<boost::asio::buffer_size(op.req.buffers[m]); idx++)
                                {
                                        if(data[idx]!=buffer[idx])
                                        {
                                                failures.push(new pair<const Op *, size_t>(make_pair(&op, idxoffset+idx)));
        #ifdef _DEBUG
                                                string contents(data, 8), shouldbe(buffer, 8);
                                                cout << "Contents of file at " << op.req.where << " +" << idx << " contains " << contents << " instead of " << shouldbe << endl;
        #endif
                                                break;
                                        }
                                }
                                if(idx!=boost::asio::buffer_size(op.req.buffers[m])) break;
                                data+=boost::asio::buffer_size(op.req.buffers[m]);
                                idxoffset+=boost::asio::buffer_size(op.req.buffers[m]);
                        }
                        return make_pair(true, h);
                };
        #ifndef BOOST_ASIO_BUG_WORKAROUND
        #pragma omp parallel for
        #endif
                for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
                {
                        for(Op &op : todo[n])
                        {
                                op.req.precondition=manywrittenfiles[n];
                                if(op.write)
                                {
                                        manywrittenfiles[n]=dispatcher->write(op.req);
                                }
                                else
                                        manywrittenfiles[n]=dispatcher->completion(dispatcher->read(op.req), std::make_pair(async_op_flags::ImmediateCompletion, std::bind(checkHash, ref(op), towriteptrs[n], placeholders::_1, placeholders::_2)));
                        }
                        // After replay, read the entire file into memory
                        manywrittenfiles[n]=dispatcher->read(async_data_op_req<char>(manywrittenfiles[n], towriteptrs[n], towritesizes[n], 0));
                }

                // Close each of those files
                auto manyclosedfiles(dispatcher->close(manywrittenfiles));
                auto dispatched=chrono::high_resolution_clock::now();
                cout << "There are now " << dec << dispatcher->count() << " handles open with a queue depth of " << dispatcher->wait_queue_depth() << endl;

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
                for(auto &i : manyclosedfiles)
                {
                        readed+=i.h->get()->read_count();
                        written+=i.h->get()->write_count();
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
                        pair<Op *, size_t> *failedop;
                        cout << "The following hash failures occurred:" << endl;
                        while(failures.pop(failedop))
                        {
                                auto undofailedop=boost::afio::detail::Undoer([&failedop]{ delete failedop; });
                                size_t bytes=0;
                                for(auto &b : failedop->first->req.buffers)
                                        bytes+=boost::asio::buffer_size(b);
                                cout << "   " << (failedop->first->write ? "Write to" : "Read from") << " " << boost::to_string(failedop->first->req.where) << " at offset " << failedop->second << " into bytes " << bytes << endl;
                        }
                }
                BOOST_TEST_MESSAGE("Checking if the final files have exactly the right contents ... this may take a bit ...");
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
                        }
                }
        #ifdef DEBUG_TORTURE_TEST
                for(ptrdiff_t n=0; n<(ptrdiff_t) no; n++)
                {
                        for(Op &op : todo[n])
                        {
                                for(auto &i : op.data)
                                        aligned_allocator.deallocate(i, 0);
                        }
                }
        #endif
                // Delete each of those files once they are closed
                auto it(manyclosedfiles.begin());
                for(auto &i : manyfilereqs)
                        i.precondition=*it++;
                auto manydeletedfiles(dispatcher->rmfile(manyfilereqs));
                // Wait for all files to delete
                when_all(manydeletedfiles.begin(), manydeletedfiles.end()).wait();
                auto rmdir(dispatcher->rmdir(async_path_op_req("testdir")));
                // Fetch any outstanding error
                rmdir.h->get();
        }

        BOOST_AUTO_TEST_CASE(async_io_torture)
        {
             BOOST_TEST_MESSAGE("Tortures the async i/o implementation");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::None);
                std::cout << "\n\nSustained random i/o to 10 files of 10Mb:\n";
        #ifdef BOOST_ASIO_BUG_WORKAROUND
                evil_random_io(dispatcher, 1, 10*1024*1024);
        #else
                evil_random_io(dispatcher, 10, 10*1024*1024);
        #endif
        }

        BOOST_AUTO_TEST_CASE(async_io_torture_sync)
        {
            BOOST_TEST_MESSAGE( "Tortures the synchronous async i/o implementation");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSSync);
                std::cout << "\n\nSustained random synchronous i/o to 10 files of 1Mb:\n";
                evil_random_io(dispatcher, 10, 1*1024*1024);
        }

        BOOST_AUTO_TEST_CASE(async_io_torture_autoflush)
        {
            BOOST_TEST_MESSAGE( "Tortures the autoflush async i/o implementation");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::AutoFlush);
                std::cout << "\n\nSustained random autoflush i/o to 10 files of 1Mb:\n";
                evil_random_io(dispatcher, 10, 1*1024*1024);
        }

        BOOST_AUTO_TEST_CASE(async_io_torture_direct)
        {
             BOOST_TEST_MESSAGE("Tortures the direct async i/o implementation");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect);
                std::cout << "\n\nSustained random direct i/o to 10 files of 10Mb:\n";
                evil_random_io(dispatcher, 10, 10*1024*1024, 4096);
        }

        BOOST_AUTO_TEST_CASE(async_io_torture_directsync)
        {
            BOOST_TEST_MESSAGE("Tortures the direct synchronous async i/o implementation");
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSDirect|boost::afio::file_flags::OSSync);
                std::cout << "\n\nSustained random direct synchronous i/o to 10 files of 1Mb:\n";
                evil_random_io(dispatcher, 10, 1*1024*1024, 4096);
        }

        BOOST_AUTO_TEST_CASE(async_io_sync)
        {
            BOOST_TEST_MESSAGE( "Tests async fsync");
                using namespace boost::afio;
                using namespace std;
                vector<char> buffer(64, 'n');
                auto dispatcher=boost::afio::async_file_io_dispatcher(boost::afio::process_threadpool(), boost::afio::file_flags::OSSync);
                std::cout << "\n\nTesting synchronous directory and file creation:\n";
                auto mkdir(dispatcher->dir(async_path_op_req("testdir", file_flags::Create)));
                auto mkfile(dispatcher->file(async_path_op_req(mkdir, "testdir/foo", file_flags::Create|file_flags::ReadWrite)));
                auto writefile1(dispatcher->write(async_data_op_req<vector<char>>(mkfile, buffer, 0)));
                auto sync1(dispatcher->sync(writefile1));
                auto writefile2(dispatcher->write(async_data_op_req<vector<char>>(sync1, buffer, 0)));
                auto closefile(dispatcher->close(writefile2));
                auto delfile(dispatcher->rmfile(async_path_op_req(closefile, "testdir/foo")));
                auto deldir(dispatcher->rmdir(async_path_op_req(delfile, "testdir")));
                BOOST_CHECK_NO_THROW(when_all(deldir).wait());
        }

    BOOST_AUTO_TEST_SUITE_END() //end exclude_async_io_erros
BOOST_AUTO_TEST_SUITE_END() // end all        

