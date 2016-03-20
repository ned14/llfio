#include "test_functions.hpp"

BOOST_AFIO_AUTO_TEST_CASE(async_io_pagesize, "Tests that the utility functions work", 120)
{
    using namespace BOOST_AFIO_V2_NAMESPACE;
    namespace asio = BOOST_AFIO_V2_NAMESPACE::asio;
    typedef chrono::duration<double, ratio<1, 1>> secs_type;

    std::cout << "\n\nSystem page sizes are: " << std::endl;
    for(auto &i : utils::page_sizes(false))
      std::cout << "  " << i << " bytes" << std::endl;
    BOOST_CHECK(!utils::page_sizes(false).empty());
    std::cout << "\n\nActually available system page sizes are: " << std::endl;
    for(auto &i : utils::page_sizes(true))
      std::cout << "  " << i << " bytes" << std::endl;
    BOOST_CHECK(!utils::page_sizes(true).empty());
    
    std::vector<char, utils::page_allocator<char>> fba(8*1024*1024);
    auto fba_detail(utils::detail::calculate_large_page_allocation(8*1024*1024));
    std::cout << "\n\nAllocating 8Mb with the file buffer allocator yields an address at " << ((void *) fba.data())
              << " and may use pages of " << fba_detail.page_size_used << " and be actually "
              << fba_detail.actual_size << " bytes allocated." << std::endl;
    
    auto randomstring(utils::random_string(32));
    std::cout << "\n\n256 bits of random string might be: " << randomstring << " which is " << randomstring.size() << " bytes long." << std::endl;
    BOOST_CHECK(randomstring.size()==64);
    auto begin=chrono::high_resolution_clock::now();
    while(chrono::duration_cast<secs_type>(chrono::high_resolution_clock::now()-begin).count()<3);
    static const size_t ITEMS=1000000;
    std::vector<char> buffer(32*ITEMS, ' ');
    begin=chrono::high_resolution_clock::now();
    for(size_t n=0; n<buffer.size()/32; n++)
      utils::random_fill(buffer.data()+n*32, 32);
    auto end=chrono::high_resolution_clock::now();
    auto diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "\n\nKernel can generate " << (buffer.size()/diff.count()/1024/1024) << " Mb/sec of 256 bit cryptographic randomness" << std::endl;

    std::vector<std::vector<char>> filenames1(ITEMS, std::vector<char>(64, ' '));
    begin=chrono::high_resolution_clock::now();
    for(size_t n=0; n<ITEMS; n++)
      utils::to_hex_string(const_cast<char *>(filenames1[n].data()), 64, buffer.data()+n*32, 32);
    end=chrono::high_resolution_clock::now();
    diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "\n\nto_hex_string can convert " << (ITEMS*64/diff.count()/1024/1024) << " Mb/sec of 256 bit numbers to hex" << std::endl;

    std::vector<char> buffer1(32*ITEMS, ' ');
    begin=chrono::high_resolution_clock::now();
    for(size_t n=0; n<ITEMS; n++)
      utils::from_hex_string(buffer1.data()+n*32, 32, filenames1[n].data(), 64);
    end=chrono::high_resolution_clock::now();
    diff=chrono::duration_cast<secs_type>(end-begin);
    std::cout << "\n\nfrom_hex_string can convert " << (ITEMS*64/diff.count()/1024/1024) << " Mb/sec of hex to 256 bit numbers" << std::endl;
    BOOST_CHECK(!memcmp(buffer.data(), buffer1.data(), buffer.size()));    

#if !RUNNING_ON_SANITIZER
#ifndef _MSC_VER
#if defined(__i386__) || defined(__x86_64__)
      static int have_popcnt=[]{
        size_t cx, dx;
#if defined(__x86_64__)
        asm("cpuid": "=c" (cx), "=d" (dx) : "a" (1), "b" (0), "c" (0), "d" (0));
#else
        asm("pushl %%ebx\n\tcpuid\n\tpopl %%ebx\n\t": "=c" (cx), "=d" (dx) : "a" (1), "c" (0), "d" (0));
#endif
        return (dx&(1<<26))!=0/*SSE2*/ && (cx&(1<<23))!=0/*POPCNT*/;
      }();
      std::cout << "\n\nThis CPU has the popcnt instruction: " << have_popcnt << std::endl;
#endif
#endif
    {
      static BOOST_CONSTEXPR_OR_CONST size_t bytes=4096;
      std::vector<char> buffer(bytes);
      utils::random_fill(buffer.data(), bytes);
      utils::secded_ecc<bytes> engine;
      typedef utils::secded_ecc<bytes>::result_type ecc_type;
      ecc_type eccbits=engine.result_bits_valid();
      std::cout << "\n\nECC will be " << eccbits << " bits long" << std::endl;
      ecc_type ecc=engine(buffer.data());
      std::cout << "ECC was calculated to be " << std::hex << ecc << std::dec << std::endl;
     
      auto end=std::chrono::high_resolution_clock::now(), begin=std::chrono::high_resolution_clock::now();  
      auto diff=std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin);
#ifdef _MSC_VER
      auto rdtsc=[]
      {
        return (unsigned long long) __rdtsc();
      };
#else
#ifdef __rdtsc
        return (unsigned long long) __rdtsc();
#elif defined(__x86_64__)
      auto rdtsc=[]
      {
        unsigned lo, hi;
        asm volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        return (unsigned long long) lo | ((unsigned long long) hi<<32);
      };
#elif defined(__i386__)
      auto rdtsc=[]
      {
        unsigned count;
        asm volatile ("rdtsc" : "=a"(count));
        return (unsigned long long) count;
      };
#endif
#if __ARM_ARCH>=6
      auto rdtsc=[]
      {
        unsigned count;
        asm volatile ("MRC p15, 0, %0, c9, c13, 0" : "=r"(count));
        return (unsigned long long) count * 64;
      };
#endif
#endif
      unsigned long long _begin=rdtsc(), _end;
#if 1
      do
      {
        end=std::chrono::high_resolution_clock::now();
      } while(std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin).count()<1);
      _end=rdtsc();
      std::cout << "There are " << (_end-_begin) << " TSCs in 1 second." << std::endl;  
#endif
     
      std::cout << "Flipping every bit in the buffer to see if it is correctly detected ..." << std::endl;  
      begin=std::chrono::high_resolution_clock::now();  
      for(size_t toflip=0; toflip<bytes*8; toflip++)
      {
        buffer[toflip/8]^=((size_t)1<<(toflip%8));
        ecc_type newecc=engine(buffer.data());
        if(ecc==newecc)
        {
          std::cerr << "ERROR: Flipping bit " << toflip << " not detected!" << std::endl;
          BOOST_CHECK(ecc!=newecc);
        }
        else
        {
          ecc_type badbit=engine.find_bad_bit(ecc, newecc);
          if(badbit!=toflip)
          {
            std::cerr << "ERROR: Bad bit " << badbit << " is not the bit " << toflip << " we flipped!" << std::endl;
            BOOST_CHECK(badbit==toflip);
          }
    //      else
    //        std::cout << "SUCCESS: Bit flip " << toflip << " correctly detected" << std::endl;
        }
        if(2!=engine.verify(buffer.data(), ecc))
        {
          std::cerr << "ERROR: verify() did not heal the buffer!" << std::endl;
          BOOST_CHECK(false);
        }
      }
      end=std::chrono::high_resolution_clock::now();
      diff=std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin);
      std::cout << "Checking and fixing is approximately " << (bytes*10000/diff.count()/1024/1024) << " Mb/sec" << std::endl;
      
      std::cout << "\nFlipping two bits in the buffer to see if it is correctly detected ..." << std::endl;  
      buffer[0]^=1;
      begin=std::chrono::high_resolution_clock::now();  
      for(size_t toflip=1; toflip<bytes*8; toflip++)
      {
        buffer[toflip/8]^=((size_t)1<<(toflip%8));
        ecc_type newecc=engine(buffer.data());
        if(ecc==newecc)
        {
          std::cerr << "ERROR: Flipping bits 0 and " << toflip << " not detected!" << std::endl;
          BOOST_CHECK(ecc!=newecc);
        }
        buffer[toflip/8]^=((size_t)1<<(toflip%8));
      }
      end=std::chrono::high_resolution_clock::now();
      diff=std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin);
      std::cout << "Calculating is approximately " << (bytes*10000/diff.count()/1024/1024) << " Mb/sec" << std::endl;
     
      std::cout << "\nCalculating speeds ..." << std::endl;
      size_t foo=0;
      begin=std::chrono::high_resolution_clock::now();
      _begin=rdtsc();
      for(size_t n=0; n<10000; n++)
      {
        buffer[0]=(char)n;
        foo+=engine(buffer.data());
      }
      _end=rdtsc();
      end=std::chrono::high_resolution_clock::now();
      diff=std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin);
      if(foo)
        std::cout << "Fixed buffer size calculating is approximately " << (bytes*10000/diff.count()/1024/1024) << " Mb/sec, or " << ((_end-_begin)/10000.0/4096) << " cycles/byte" << std::endl;
      foo=0;
      begin=std::chrono::high_resolution_clock::now();
      _begin=rdtsc();
      for(size_t n=0; n<10000; n++)
      {
        buffer[0]=(char)n;
        foo+=engine(buffer.data(), bytes);
      }
      _end=rdtsc();
      end=std::chrono::high_resolution_clock::now();
      diff=std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin);
      if(foo)
        std::cout << "Variable buffer size calculating is approximately " << (bytes*10000/diff.count()/1024/1024) << " Mb/sec, or " << ((_end-_begin)/10000.0/4096) << " cycles/byte" << std::endl;
      foo=0;
      begin=std::chrono::high_resolution_clock::now();  
      for(size_t n=0; n<1000; n++)
      {
        buffer[0]=(char)n;
        foo+=engine.verify(buffer.data(), ecc);
      }
      end=std::chrono::high_resolution_clock::now();
      diff=std::chrono::duration_cast<std::chrono::duration<double, std::ratio<1, 1>>>(end-begin);
      if(foo)
        std::cout << "Checking and fixing is approximately " << (bytes*1000/diff.count()/1024/1024) << " Mb/sec" << std::endl;
    }
#endif
    
    auto dispatcher=make_dispatcher().get();
    std::cout << "\n\nThread source use count is: " << dispatcher->threadsource().use_count() << std::endl;
    BOOST_AFIO_CHECK_THROWS(dispatcher->op_from_scheduled_id(78));
}