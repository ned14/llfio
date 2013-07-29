/* Unit tests for TripleGit
(C) 2013 Niall Douglas http://www.nedprod.com/
Created: Feb 2013
*/

#ifndef BOOST_AFIO_TEST_FUNCTIONS_HPP
#define BOOST_AFIO_TEST_FUNCTIONS_HPP


// If defined, uses a ton more memory and is many orders of magnitude slower.
#define DEBUG_TORTURE_TEST 1

#ifdef __MINGW32__
// Mingw doesn't define putenv() needed by Boost.Test
extern int putenv(char*);
#endif

#include <utility>
#include <sstream>
#include <iostream>
#include <algorithm>
#include "boost/lockfree/queue.hpp"
#include "../../../boost/afio/afio.hpp"
#include "../detail/SpookyV2.h"
#include "../../../boost/afio/detail/Aligned_Allocator.hpp"
#include "../../../boost/afio/detail/Undoer.hpp"


//if we're building the tests all together don't define the test main
#ifndef BOOST_AFIO_TEST_ALL
#    define BOOST_TEST_MAIN  //must be defined before unit_test.hpp is included
#endif

#include <boost/test/included/unit_test.hpp>

//define a simple macro to check any exception using Boost.Test
#define BOOST_AFIO_CHECK_THROWS(expr)\
try{\
    expr;\
    BOOST_FAIL("Exception was not thrown");\
}catch(...){BOOST_CHECK(true);}

// From http://burtleburtle.net/bob/rand/smallprng.html
typedef unsigned int  u4;
typedef struct ranctx { u4 a; u4 b; u4 c; u4 d; } ranctx;

#define rot(x,k) (((x)<<(k))|((x)>>(32-(k))))
u4 ranval(ranctx *x) {
    u4 e = x->a - rot(x->b, 27);
    x->a = x->b ^ rot(x->c, 17);
    x->b = x->c + x->d;
    x->c = x->d + e;
    x->d = e + x->a;
    return x->d;
}

void raninit(ranctx *x, u4 seed) {
    u4 i;
    x->a = 0xf1ea5eed, x->b = x->c = x->d = seed;
    for (i = 0; i < 20; ++i) {
        (void) ranval(x);
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
#pragma omp parallel for
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

#endif