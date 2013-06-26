/* async_file_io
Provides a threadpool and asynchronous file i/o infrastructure based on Boost.ASIO, Boost.Iostreams and filesystem
(C) 2013 Niall Douglas http://www.nedprod.com/
File Created: Mar 2013
*/

#ifndef TRIPLEGIT_ASYNC_FILE_IO_H
#define TRIPLEGIT_ASYNC_FILE_IO_H

#include "../NiallsCPP11Utilities/NiallsCPP11Utilities.hpp"
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
#include<boost/config.hpp>

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

#ifndef BOOST_AFIO_API
#ifdef BOOST_AFIO_DLL_EXPORTS
//#define BOOST_AFIO_API DLLEXPORTMARKUP
#define BOOST_AFIO_API BOOST_SYMBOL_EXPORT
#else
//#define BOOST_AFIO_API DLLIMPORTMARKUP
#define BOOST_AFIO_API BOOST_SYMBOL_IMPORT
#endif
#endif

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

/*! \file async_file_io.hpp
\brief Provides a batch asynchronous file i/o implementation based on Boost.ASIO

My Seagate 7200rpm drive:

-----------------------------------------------------------------------
CrystalDiskMark 3.0.2 x64 (C) 2007-2013 hiyohiyo
                           Crystal Dew World : http://crystalmark.info/
-----------------------------------------------------------------------
* MB/s = 1,000,000 byte/s [SATA/300 = 300,000,000 byte/s]

           Sequential Read :    46.012 MB/s
          Sequential Write :    44.849 MB/s
         Random Read 512KB :    26.367 MB/s
        Random Write 512KB :    23.521 MB/s
    Random Read 4KB (QD=1) :     0.487 MB/s [   118.8 IOPS]
   Random Write 4KB (QD=1) :     0.771 MB/s [   188.3 IOPS]
   Random Read 4KB (QD=32) :     0.819 MB/s [   199.8 IOPS]
  Random Write 4KB (QD=32) :     0.789 MB/s [   192.6 IOPS]

  Test : 1000 MB [G: 99.0% (178.2/180.1 GB)] (x5)
  Date : 2013/04/13 23:02:23
    OS : Windows 8  [6.2 Build 9200] (x64)
  

Windows IOCP backend, 3.5Ghz Ivy Bridge Windows 8 x64 on 
my Seagate 7200rpm drive:

1000 file opens, writes 1 byte, closes, and deletes:
It took 0.26311 secs to do all operations
  It took 0.0079942 secs to dispatch all operations
  It took 0.255116 secs to finish all operations

It took 0.0899875 secs to do 11112.7 file opens per sec
It took 0.0100023 secs to do 99977 file writes per sec
It took 0.0210025 secs to do 47613.4 file closes per sec
It took 0.142118 secs to do 7036.43 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes:
It took 1.01195 secs to do all operations
  It took 0.0079939 secs to dispatch all operations
  It took 1.00396 secs to finish all operations

It took 0.10599 secs to do 9434.83 file opens per sec
It took 0.0410053 secs to do 24387.1 file writes per sec
It took 0.769099 secs to do 1300.22 file closes per sec
It took 0.0958558 secs to do 10432.3 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with synchronous i/o:
It took 9.28653 secs to do all operations
  It took 0.142254 secs to dispatch all operations
  It took 9.14427 secs to finish all operations

It took 8.92534 secs to do 112.041 file opens per sec
It took 0.112127 secs to do 8918.5 file writes per sec
It took 0.177022 secs to do 5649.03 file closes per sec
It took 0.0720397 secs to do 13881.2 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:
It took 3.03522 secs to do all operations
  It took 0.0212259 secs to dispatch all operations
  It took 3.01399 secs to finish all operations

It took 1.92052 secs to do 520.694 file opens per sec
It took 0.0720104 secs to do 13886.9 file writes per sec
It took 0.806102 secs to do 1240.54 file closes per sec
It took 0.236588 secs to do 4226.75 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with autoflush i/o:
It took 11.9628 secs to do all operations
  It took 0.0080006 secs to dispatch all operations
  It took 11.9548 secs to finish all operations

It took 0.0940122 secs to do 10636.9 file opens per sec
It took 0.46754 secs to do 2138.85 file writes per sec
It took 11.2472 secs to do 88.9107 file closes per sec
It took 0.154042 secs to do 6491.76 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with autoflush i/o:
It took 14.5824 secs to do all operations
  It took 0.0080019 secs to dispatch all operations
  It took 14.5744 secs to finish all operations

It took 0.0980132 secs to do 10202.7 file opens per sec
It took 0.465669 secs to do 2147.45 file writes per sec
It took 13.8906 secs to do 71.9913 file closes per sec
It took 0.128166 secs to do 7802.36 file deletions per sec



POSIX compat backend, 3.5Ghz Ivy Bridge Linux 3.2 x64 on 
my Seagate 7200rpm drive:

1000 file opens, writes 1 byte, closes, and deletes:
It took 0.03907 secs to do all operations
  It took 0.00425 secs to dispatch all operations
  It took 0.03482 secs to finish all operations

It took 0.015097 secs to do 66238.3 file opens per sec
It took 0.009744 secs to do 102627 file writes per sec
It took 0.008307 secs to do 120380 file closes per sec
It took 0.005922 secs to do 168862 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes:
It took 0.039257 secs to do all operations
  It took 0.004198 secs to dispatch all operations
  It took 0.035059 secs to finish all operations

It took 0.017462 secs to do 57267.2 file opens per sec
It took 0.010241 secs to do 97646.7 file writes per sec
It took 0.007234 secs to do 138236 file closes per sec
It took 0.00432 secs to do 231481 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with synchronous i/o:
It took 7.53982 secs to do all operations
  It took 0.003997 secs to dispatch all operations
  It took 7.53582 secs to finish all operations

It took 6.35027 secs to do 157.473 file opens per sec
It took 1.18224 secs to do 845.848 file writes per sec
It took 0.005773 secs to do 173220 file closes per sec
It took 0.001528 secs to do 654450 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:
It took 7.97113 secs to do all operations
  It took 0.004116 secs to dispatch all operations
  It took 7.96701 secs to finish all operations

It took 6.32829 secs to do 158.021 file opens per sec
It took 1.63634 secs to do 611.12 file writes per sec
It took 0.004897 secs to do 204207 file closes per sec
It took 0.001605 secs to do 623053 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with autoflush i/o:
It took 7.46535 secs to do all operations
  It took 0.00405 secs to dispatch all operations
  It took 7.4613 secs to finish all operations

It took 6.30895 secs to do 158.505 file opens per sec
It took 1.12623 secs to do 887.922 file writes per sec
It took 0.02834 secs to do 35285.8 file closes per sec
It took 0.001838 secs to do 544070 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with autoflush i/o:
It took 8.0235 secs to do all operations
  It took 0.00419 secs to dispatch all operations
  It took 8.01931 secs to finish all operations

It took 6.29931 secs to do 158.748 file opens per sec
It took 1.68728 secs to do 592.67 file writes per sec
It took 0.035096 secs to do 28493.3 file closes per sec
It took 0.001815 secs to do 550964 file deletions per sec




My Samsung 256Gb 830 SSD:
-----------------------------------------------------------------------
CrystalDiskMark 3.0.2 x64 (C) 2007-2013 hiyohiyo
                           Crystal Dew World : http://crystalmark.info/
-----------------------------------------------------------------------
* MB/s = 1,000,000 byte/s [SATA/300 = 300,000,000 byte/s]

           Sequential Read :   478.802 MB/s
          Sequential Write :   406.425 MB/s
         Random Read 512KB :   337.185 MB/s
        Random Write 512KB :   390.353 MB/s
    Random Read 4KB (QD=1) :    21.235 MB/s [  5184.4 IOPS]
   Random Write 4KB (QD=1) :    58.373 MB/s [ 14251.1 IOPS]
   Random Read 4KB (QD=32) :   301.170 MB/s [ 73527.7 IOPS]
  Random Write 4KB (QD=32) :   148.339 MB/s [ 36215.5 IOPS]

  Test : 1000 MB [C: 72.2% (57.3/79.4 GB)] (x5)
  Date : 2013/04/13 23:10:09
    OS : Windows 8  [6.2 Build 9200] (x64)


Windows IOCP backend, 3.5Ghz Ivy Bridge Windows 8 x64 on 
my Samsung 256Gb 830 SSD drive:

1000 file opens, writes 1 byte, closes, and deletes:
It took 0.187081 secs to do all operations
  It took 0.0079831 secs to dispatch all operations
  It took 0.179098 secs to finish all operations

It took 0.0789921 secs to do 12659.5 file opens per sec
It took 0.0110014 secs to do 90897.5 file writes per sec
It took 0.0250035 secs to do 39994.4 file closes per sec
It took 0.0720843 secs to do 13872.6 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes:
It took 0.924099 secs to do all operations
  It took 0.007996 secs to dispatch all operations
  It took 0.916103 secs to finish all operations

It took 0.0859878 secs to do 11629.6 file opens per sec
It took 0.0230033 secs to do 43472 file writes per sec
It took 0.7571 secs to do 1320.83 file closes per sec
It took 0.0580077 secs to do 17239.1 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with synchronous i/o:
It took 2.98316 secs to do all operations
  It took 0.0446 secs to dispatch all operations
  It took 2.93856 secs to finish all operations

It took 2.71914 secs to do 367.763 file opens per sec
It took 0.0359893 secs to do 27786 file writes per sec
It took 0.151021 secs to do 6621.6 file closes per sec
It took 0.0770071 secs to do 12985.8 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:
It took 1.65352 secs to do all operations
  It took 0.0149789 secs to dispatch all operations
  It took 1.63854 secs to finish all operations

It took 0.714499 secs to do 1399.58 file opens per sec
It took 0.0420066 secs to do 23805.8 file writes per sec
It took 0.835109 secs to do 1197.45 file closes per sec
It took 0.0619072 secs to do 16153.2 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with autoflush i/o:
It took 2.70265 secs to do all operations
  It took 0.0089945 secs to dispatch all operations
  It took 2.69366 secs to finish all operations

It took 0.0759873 secs to do 13160.1 file opens per sec
It took 0.0334323 secs to do 29911.2 file writes per sec
It took 2.46722 secs to do 405.315 file closes per sec
It took 0.126015 secs to do 7935.54 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with autoflush i/o:
It took 2.79222 secs to do all operations
  It took 0.0079943 secs to dispatch all operations
  It took 2.78422 secs to finish all operations

It took 0.0829872 secs to do 12050.1 file opens per sec
It took 0.0381194 secs to do 26233.4 file writes per sec
It took 2.6111 secs to do 382.98 file closes per sec
It took 0.0600064 secs to do 16664.9 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with direct i/o:
It took 0.178027 secs to do all operations
  It took 0.0070116 secs to dispatch all operations
  It took 0.171015 secs to finish all operations

It took 0.0629898 secs to do 15875.6 file opens per sec
It took 0.0110008 secs to do 90902.5 file writes per sec
It took 0.0190018 secs to do 52626.6 file closes per sec
It took 0.0850346 secs to do 11759.9 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with direct i/o:
It took 1.67592 secs to do all operations
  It took 0.0169852 secs to dispatch all operations
  It took 1.65893 secs to finish all operations

It took 0.711476 secs to do 1405.53 file opens per sec
It took 0.0163148 secs to do 61294 file writes per sec
It took 0.890117 secs to do 1123.45 file closes per sec
It took 0.058008 secs to do 17239 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with direct synchronous i/o:

It took 0.309121 secs to do all operations
  It took 0.00698 secs to dispatch all operations
  It took 0.302141 secs to finish all operations

It took 0.138198 secs to do 7235.99 file opens per sec
It took 0.0142443 secs to do 70203.5 file writes per sec
It took 0.0796398 secs to do 12556.5 file closes per sec
It took 0.0770388 secs to do 12980.5 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with direct synchronous i/o:
It took 1.95023 secs to do all operations
  It took 0.020987 secs to dispatch all operations
  It took 1.92924 secs to finish all operations

It took 0.967097 secs to do 1034.02 file opens per sec
It took 0.0290068 secs to do 34474.7 file writes per sec
It took 0.901115 secs to do 1109.74 file closes per sec
It took 0.0530077 secs to do 18865.2 file deletions per sec





iozone -e -I -a -s 1000M -r 4k -r 512k -i 0 -i 1 -i 2
                                                            random  random    bkwd   record   stride                                   
              KB  reclen   write rewrite    read    reread    read   write    read  rewrite     read   fwrite frewrite   fread  freread
         1024000       4    2528    3044    12000    11693   11337     931                                                          
         1024000     512   17393   17039    68703    68524   67958    3881                                                          

iozone -e -I -s 31M -r 4k -t 32 -T -i 0 -i 2

        Children see throughput for 2 initial writers = 1502.15 KB/sec
        Parent sees throughput for 2 initial writers = 1502.13 KB/sec
        Min throughput per thread = 751.07 KB/sec
        Max throughput per thread = 751.08 KB/sec
        Avg throughput per thread = 751.07 KB/sec
        Min xfer = 511992.00 KB

        Children see throughput for 2 rewriters = 1537.12 KB/sec
        Parent sees throughput for 2 rewriters = 1537.11 KB/sec
        Min throughput per thread = 768.34 KB/sec
        Max throughput per thread = 768.77 KB/sec
        Avg throughput per thread = 768.56 KB/sec
        Min xfer = 511716.00 KB

        Children see throughput for 2 random readers = 9044.48 KB/sec
        Parent sees throughput for 2 random readers = 9044.42 KB/sec
        Min throughput per thread = 4521.79 KB/sec
        Max throughput per thread = 4522.69 KB/sec
        Avg throughput per thread = 4522.24 KB/sec
        Min xfer = 511900.00 KB

        Children see throughput for 2 random writers = 825.19 KB/sec
        Parent sees throughput for 2 random writers = 825.14 KB/sec
        Min throughput per thread = 412.52 KB/sec
        Max throughput per thread = 412.67 KB/sec
        Avg throughput per thread = 412.60 KB/sec
        Min xfer = 511812.00 KB


From these I infer:

           Sequential Read :   67.09 MB/s
          Sequential Write :   16.98 MB/s
         Random Read 512KB :   66.36 MB/s
        Random Write 512KB :    3.79 MB/s
    Random Read 4KB (QD=1) :   11.42 MB/s [ 2834.3 IOPS]
   Random Write 4KB (QD=1) :    0.91 MB/s [  232.8 IOPS]
   Random Read 4KB (QD=32) :      ? MB/s  [      ? IOPS]
  Random Write 4KB (QD=32) :      ? MB/s  [      ? IOPS]


POSIX compat backend, 1.7Ghz ARM Cortex-A15 Linux 3.4 on 
Samsung Chromebook eMMC internal drive:

1000 file opens, writes 1 byte, closes, and deletes:
It took 0.386876 secs to do all operations
  It took 0.110156 secs to dispatch all operations
  It took 0.27672 secs to finish all operations

It took 0.181143 secs to do 5520.5 file opens per sec
It took 0.094321 secs to do 10602.1 file writes per sec
It took 0.070552 secs to do 14173.9 file closes per sec
It took 0.04086 secs to do 24473.8 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes:
It took 0.425939 secs to do all operations
  It took 0.147966 secs to dispatch all operations
  It took 0.277973 secs to finish all operations

It took 0.17901 secs to do 5586.28 file opens per sec
It took 0.152722 secs to do 6547.85 file writes per sec
It took 0.054538 secs to do 18335.8 file closes per sec
It took 0.039669 secs to do 25208.6 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with synchronous i/o:
It took 19.4941 secs to do all operations
  It took 0.07904 secs to dispatch all operations
  It took 19.4151 secs to finish all operations

It took 13.323 secs to do 75.058 file opens per sec
It took 6.06393 secs to do 164.909 file writes per sec
It took 0.071753 secs to do 13936.7 file closes per sec
It took 0.035389 secs to do 28257.4 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with synchronous i/o:
It took 22.8757 secs to do all operations
  It took 0.102336 secs to dispatch all operations
  It took 22.7734 secs to finish all operations

It took 5.35023 secs to do 186.908 file opens per sec
It took 17.498 secs to do 57.1495 file writes per sec
It took 0.014796 secs to do 67585.8 file closes per sec
It took 0.012747 secs to do 78449.8 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with autoflush i/o:
It took 13.2068 secs to do all operations
  It took 0.085519 secs to dispatch all operations
  It took 13.1213 secs to finish all operations

It took 9.65411 secs to do 103.583 file opens per sec
It took 3.40673 secs to do 293.537 file writes per sec
It took 0.119205 secs to do 8388.91 file closes per sec
It took 0.02677 secs to do 37355.2 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with autoflush i/o:
It took 24.0624 secs to do all operations
  It took 0.092833 secs to dispatch all operations
  It took 23.9695 secs to finish all operations

It took 12.9862 secs to do 77.0047 file opens per sec
It took 10.8002 secs to do 92.5906 file writes per sec
It took 0.219705 secs to do 4551.56 file closes per sec
It took 0.05619 secs to do 17796.8 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with direct i/o:
[Started testing]

[Running: async_io/works/1/direct]
unittests/main.cpp:266: !(((size_t) &towrite.front()) & 4095) failed with unexpected exception with message: 'Invalid argument (22) in 'triplegit/src/async_file_io.cpp':dowrite:884'
[Finished: 'async_io/works/1/direct' 1 test case failed (1 of 2 assertions failed)]


1000 file opens, writes 64Kb, closes, and deletes with direct i/o:
It took 6.84248 secs to do all operations
  It took 0.125639 secs to dispatch all operations
  It took 6.71684 secs to finish all operations

It took 0.204354 secs to do 4893.47 file opens per sec
It took 6.58176 secs to do 151.935 file writes per sec
It took 0.025033 secs to do 39947.3 file closes per sec
It took 0.031331 secs to do 31917.3 file deletions per sec


1000 file opens, writes 1 byte, closes, and deletes with direct synchronous i/o:
It took 7.00683 secs to do all operations
  It took 0.054578 secs to dispatch all operations
  It took 6.95225 secs to finish all operations

It took 6.88883 secs to do 145.163 file opens per sec
It took 0.057979 secs to do 17247.6 file writes per sec
It took 0.030898 secs to do 32364.6 file closes per sec
It took 0.029125 secs to do 34334.8 file deletions per sec


1000 file opens, writes 64Kb, closes, and deletes with direct synchronous i/o:
It took 24.9112 secs to do all operations
  It took 0.058847 secs to dispatch all operations
  It took 24.8524 secs to finish all operations

It took 11.8282 secs to do 84.5438 file opens per sec
It took 13.0079 secs to do 76.8762 file writes per sec
It took 0.057255 secs to do 17465.7 file closes per sec
It took 0.017878 secs to do 55934.7 file deletions per sec

// namespaces dont show up in documentation unless I also document the parent namespace
// this is ok for now, but will need to be fixed when we improve the docs.
*/
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
extern BOOST_AFIO_API thread_pool &process_threadpool();

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
class BOOST_AFIO_API async_file_io_dispatcher_base : public std::enable_shared_from_this<async_file_io_dispatcher_base>
{
	//friend BOOST_AFIO_API std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);
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
extern BOOST_AFIO_API std::shared_ptr<async_file_io_dispatcher_base> async_file_io_dispatcher(thread_pool &threadpool=process_threadpool(), file_flags flagsforce=file_flags::None, file_flags flagsmask=file_flags::None);

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
