#include "test_functions.hpp"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4201)  // nameless struct union
#endif
BOOST_AFIO_AUTO_TEST_CASE(atomic_log_append, "Tests that atomic append to a shared log file works as expected", 60)
{
    std::cout << "\n\nTesting atomic append to a shared log file\n";
    using namespace BOOST_AFIO_V2_NAMESPACE;
    using BOOST_AFIO_V2_NAMESPACE::off_t;
    try { filesystem::remove_all("testdir"); } catch(...) {}
    filesystem::create_directory("testdir");
    std::vector<thread> threads;
    atomic<bool> done(false);
    for(size_t n=0; n<4; n++)
    {
      threads.push_back(thread([&done, n]{
        try
        {
//[extents_example
            // Create a dispatcher
            auto dispatcher = make_dispatcher().get();
            // Schedule opening the log file for hole punching
            auto logfilez(dispatcher->file(path_req("testdir/log",
                file_flags::create | file_flags::read_write)));
            // Schedule opening the log file for atomic appending of log entries
            auto logfilea(dispatcher->file(path_req("testdir/log",
                file_flags::create | file_flags::write | file_flags::append)));
            // Retrieve any errors which occurred
            logfilez.get(); logfilea.get();
            // Initialise a random number generator
            ranctx ctx; raninit(&ctx, (u4) n);
            while(!done)
            {
              // Each log entry is 32 bytes in length
              union
              {
                char bytes[32];
                struct
                {
                  uint64 id;     // The id of the writer
                  uint64 r;      // A random number
                  uint64 h1, h2; // A hash of the previous two items
                };
              } buffer;
              buffer.id=n;
              buffer.r=ranval(&ctx);
              buffer.h1=buffer.h2=1;
              SpookyHash::Hash128(buffer.bytes, 16, &buffer.h1, &buffer.h2);
              // Atomically append the log entry to the log file and wait for it
              // to complete, then fetch the new size of the log file.
              stat_t s=dispatcher->write(make_io_req(logfilea,
                buffer.bytes, 32, 0))->lstat();
              if(s.st_allocated>8192 || s.st_size>8192)
              {
                // Allocated space exceeds 8Kb. The actual file size reported may be
                // many times larger if the filing system supports hole punching.
                
                // Get the list of allocated extents
                std::vector<std::pair<off_t, off_t>> extents=
                    dispatcher->extents(logfilez).get();
                // Filing system may not yet have allocated any storage ...
                if (!extents.empty())
                {
                  if (extents.back().second > 1024)
                    extents.back().second -= 1024;
                  else
                    extents.resize(extents.size() - 1);
                  if (!extents.empty())
                  {
                    dispatcher->zero(logfilez, extents).get();
                  }
                }
                else
                  std::cout << "NOTE: extents() returns empty despite " << s.st_allocated << " bytes allocated (possibly delayed allocation)" << std::endl;
              }
            }
//]
        }
      catch(const system_error &e) { std::cerr << "ERROR: unit test exits via system_error code " << e.code().value() << "(" << e.what() << ")" << std::endl; BOOST_FAIL("Unit test exits via exception"); abort(); }
      catch(const std::exception &e) { std::cerr << "ERROR: unit test exits via exception (" << e.what() << ")" << std::endl; BOOST_FAIL("Unit test exits via exception"); abort(); }
      catch(...) { std::cerr << "ERROR: unit test exits via unknown exception" << std::endl; BOOST_FAIL("Unit test exits via exception"); abort(); }
      }));
    }
    this_thread::sleep_for(chrono::seconds(10));
    done=true;
    std::cout << "Waiting for threads to exit ..." << std::endl;
    for(auto &i : threads)
      i.join();
    // Examine the file for consistency
    {
      std::cout << "Examining the file, skipping zeros ..." << std::endl;
      std::ifstream is("testdir/log", std::ifstream::binary);
      size_t count=0;
      uint64 hash1, hash2;
      union
      {
        char bytes[32];
        struct { uint64 r1, r2, h1, h2; };
      } buffer;
      bool isZero=true;
      size_t entries=0;
      while(is.good() && isZero)
      {
        is.read(buffer.bytes, 32);
        if(!is) break;
        count+=32;
        for(size_t n=0; n<32; n++)
          if(buffer.bytes[n])
          {
            isZero=false;
            break;
          }
      }
      std::cout << "Examining the file, checking records ..." << std::endl;
      while(is.good())
      {
        if(isZero)
        {
          is.read(buffer.bytes, 32);
          if(!is) break;
          count+=32;
        }
        isZero=true;
        for(size_t n=0; n<32; n++)
          if(buffer.bytes[n]) isZero=false;
        BOOST_CHECK(!isZero);
        if(isZero)
        {
          std::cout << "(zero)" << std::endl;
        }
        else
        {
          // First 16 bytes is random, second 16 bytes is hash
          hash1=hash2=1;
          SpookyHash::Hash128(buffer.bytes, 16, &hash1, &hash2);
          BOOST_CHECK((buffer.h1==hash1 && buffer.h2==hash2));
          entries++;
          isZero=true;
        }
      }
      BOOST_CHECK((entries>=32 && entries<=8192/32+1));
      std::cout << "There were " << entries << " valid entries in " << count << " bytes" << std::endl;
    }
    filesystem::remove_all("testdir");
}
#ifdef _MSC_VER
#pragma warning(pop)
#endif
