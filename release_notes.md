<center><table border="0" cellpadding="4">
<tr>
<td align="center"> <a href="https://github.com/ned14/afio">AFIO</a><br><a href="https://github.com/ned14/afio">on GitHub</a> </td>
<td align="center"> <a href="http://my.cdash.org/index.php?project=Boost.AFIO">CTest summary</a><br><a href="http://my.cdash.org/index.php?project=Boost.AFIO">dashboard</a> </td>
<td align="center"> <a href="https://travis-ci.org/ned14/afio">Linux and MacOS CI:</a><img src="https://travis-ci.org/ned14/afio.svg?branch=master"/> </td>
<td align="center"> <a href="https://ci.appveyor.com/project/ned14/afio/branch/master">Windows CI:</a><img src="https://ci.appveyor.com/api/projects/status/680b1pt9srnoprs3/branch/master?svg=true"/> </td>
</tr>
</table></center>

Herein lies my proposed zero whole machine memory copy async file i/o and filesystem
library for Boost and the C++ standard, intended for storage devices with ~1 microsecond
4Kb transfer latencies and those supporting Storage Class Memory (SCM)/Direct Access
Storage (DAX). Its i/o overhead, including syscall overhead, has been benchmarked to
100 nanoseconds on Linux which corresponds to a theoretical maximum of 10M IOPS @ QD1,
approx 40Gb/sec per thread. It has particularly strong support for writing portable
filesystem algorithms which work well with directly mapped non-volatile storage such
as Intel Optane.

It is a complete rewrite after a Boost peer review in August 2015. Its github
source code repository lives at https://github.com/ned14/boost.afio.

- Portable to any conforming C++ 14 compiler with a working Filesystem TS in its STL.
- Will make use of any Concepts TS if you have them.
  - (Coroutines TS support is in the works)
- Provides view adapters into the Ranges TS, so ready for STL2.
- Original error code is always preserved, even down to the original NT kernel error code if a NT kernel API was used.
- Race free filesystem design used throughout (i.e. no TOCTOU).
- Zero malloc, zero exception throw and zero whole system memory copy design used throughout, even down to paths (which can hit 64Kb!).
- Works very well with the C++ standard library, and is intended to be proposed for standardisation into C++ in 2020 or thereabouts.

\note Note that this code is of late alpha quality. It's quite reliable on Windows and Linux, but be careful when using it!

Examples of use:
<table width="100%" border="0" cellpadding="4">
<tr>
<td>
\code
namespace afio = AFIO_V2_NAMESPACE;

// Make me a 1 trillion element sparsely allocated integer array!
afio::mapped_file_handle mfh = afio::mapped_temp_inode().value();

// On an extents based filing system, doesn't actually allocate any physical storage
// but does map approximately 4Tb of all bits zero data into memory
mfh.truncate(1000000000000ULL*sizeof(int));

// Create a typed view of the one trillion integers
afio::algorithm::mapped_view<int> one_trillion_int_array(mfh);

// Write and read as you see fit, if you exceed physical RAM it'll be paged to disk
one_trillion_int_array[0] = 5;
one_trillion_int_array[999999999999ULL] = 6;
\endcode
</td>
<td>
\code
namespace afio = AFIO_V2_NAMESPACE;

// Create an asynchronous file handle
afio::io_service service;
afio::async_file_handle fh = afio::async_file(service, {}, "testfile.txt",
                                              afio::async_file_handle::mode::write,
                                              afio::async_file_handle::creation::if_needed).value();

// Resize it to 1024 bytes
truncate(fh, 1024).value();

// Begin to asynchronously write "hello world" into the file at offset 0,
// suspending execution of this coroutine until completion and then resuming
// execution. Requires the Coroutines TS.
co_await async_write(fh, 0, "hello world", 11).value();
\endcode
</td>
</tr>
</table>

These compilers and OS are regularly tested:
- GCC 7.0 (Linux 4,x x64)
- clang 4.0 (Linux 4.x x64)
- clang 5.0 (Windows 10 x64)

Other compilers, architectures and OSs may work, but are not tested regularly. You will need a Filesystem TS
implementation in your STL and C++ 14. See https://github.com/ned14/afio/blob/master/programs/fs-probe/fs_probe_results.yaml
for a database of latencies for various previously tested OS, filing systems and storage devices.

Todo list for already implemented parts: https://ned14.github.io/afio/todo.html

To build and test (make, ninja etc):

~~~
mkdir build
cd build
cmake ..
cmake --build .
ctest -R afio_sl
~~~

To build and test (Visual Studio, XCode etc):

~~~
mkdir build
cd build
cmake ..
cmake --build . --config Release
ctest -C Release -R afio_sl
~~~

## v2 architecture and design implemented:

| NEW in v2 | Boost peer review feedback |     |
| --------- | -------------------------- | --- |
| ✔ | ✔ | Universal native handle/fd abstraction instead of `void *`.
| ✔ | ✔ | Perfectly/Ideally low memory (de)allocation per op (usually none).
| ✔ | ✔ | noexcept API throughout returning error_code for failure instead of throwing exceptions.
| ✔ | ✔ | AFIO v1 handle type split into hierarchy of types:<ol><li>handle - provides open, close, get path, clone, set/unset append only, change caching, characteristics<li>fs_handle - handles with an inode number<li>path_handle - a race free anchor to a subset of the filesystem<li>directory_handle - enumerates the filesystem<li>io_handle - adds synchronous scatter-gather i/o, byte range locking<li>file_handle - adds open/create file, get and set maximum extent<li>async_file_handle - adds asynchronous scatter-gather i/o<li>mapped_file_handle - adds low latency memory mapped scatter-gather i/o</ol>
| ✔ | ✔ | Cancelable i/o (made possible thanks to dropping XP support).
| ✔ | ✔ | All shared_ptr usage removed as all use of multiple threads removed.
| ✔ | ✔ | Use of std::vector to transport scatter-gather sequences replaced with C++ 20 `span<>` borrowed views.
| ✔ | ✔ | Completion callbacks are now some arbitrary type `U&&` instead of a future continuation. Type erasure for its storage is bound into the one single memory allocation for everything needed to execute the op, and so therefore overhead is optimal.
| ✔ | ✔ | Filing system algorithms made generic and broken out into public `afio::algorithms` template library (the AFIO FTL).
| ✔ | ✔ | Abstraction of native handle management via bitfield specified "characteristics".
| ✔ |   | Storage profiles, a YAML database of behaviours of hardware, OS and filing system combinations.
| ✔ |   | Absolute and interval deadline timed i/o throughout (made possible thanks to dropping XP support).
| ✔ |   | Dependency on ASIO/Networking TS removed completely.
| ✔ |   | Four choices of algorithm implementing a shared filing system mutex.
| ✔ |   | Uses CMake, CTest, CDash and CPack with automatic usage of C++ Modules or precompiled headers where available.
| ✔ |   | Far more comprehensive memory map and virtual memory facilities.
| ✔ |   | Much more granular, micro level unit testing of individual functions.
| ✔ |   | Much more granular, micro level internal logging of every code path taken.
| ✔ |   | Path views used throughout, thus avoiding string copying and allocation in `std::filesystem::path`.
| ✔ |   | Paths are equally interpreted as UTF-8 on all platforms.
| ✔ |   | We never store nor retain a path, as they are inherently racy and are best avoided.

Todo:

| NEW in v2 | Boost peer review feedback |     |
| --------- | -------------------------- | --- |
| ✔ |   | clang AST assisted SWIG bindings for other languages.
| ✔ |   | Statistical tracking of operation latencies so realtime IOPS can be measured.



## Planned features implemented:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
| ✔ | ✔ | ✔ | Native handle cloning.
| ✔ (up from four) | ✔ | ✔ | Maximum possible (seven) forms of kernel caching.
|   | ✔ | ✔ | Absolute path open.
|   | ✔ | ✔ | Relative "anchored" path open enabling race free file system.
| ✔ | ✔ |   | Win32 path support (260 path limit).
|   | ✔ |   | NT kernel path support (32,768 path limit).
| ✔ | ✔ | ✔ | Synchronous universal scatter-gather i/o.
| ✔ (POSIX AIO support) | ✔ | ✔ | Asynchronous universal scatter-gather i/o.
| ✔ | ✔ | ✔ | i/o deadlines and cancellation.
|   | ✔ | ✔ | Retrieving and setting the current maximum extent (size) of an open file.
|   | ✔ | ✔ | Retrieving the current path of an open file irrespective of where it has been renamed to by third parties.
|   | ✔ | ✔ | statfs_t ported over from AFIO v1.
|   | ✔ | ✔ | utils namespace ported over from AFIO v1.
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on lock files
| ✔ | ✔ | ✔ | Byte range shared/exclusive locking.
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on byte ranges
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on atomic append
|   | ✔ | ✔ | Memory mapped files and virtual memory management (`section_handle`, `map_handle` and `mapped_file_handle`)
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on memory maps
| ✔ | ✔ | ✔ | Universal portable UTF-8 path views.
|   | ✔ | ✔ | "Hole punching" and hole enumeration ported over from AFIO v1.
|   | ✔ | ✔ | Directory handles and very fast directory enumeration ported over from AFIO v1.
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on safe byte ranges
|   | ✔ | ✔ | Set random or sequential i/o (prefetch).

Todo to reach feature parity with AFIO v1:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
|   |   |   | Hard links and symlinks.
|   |   |   | BSD and OS X kqueues optimised `io_service`

Todo thereafter:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
| ✔ |   |   | Extended attributes support.
| ✔ |   |   | Coroutines TS integration for `async_file_handle`. Use example: https://gist.github.com/anonymous/a67ba4695c223a905ff108ed8b9a342f
| ✔ |   |   | Linux KAIO support for native non-blocking `O_DIRECT` i/o
| ✔ |   |   | Reliable directory hierarchy deletion algorithm.
| ✔ |   |   | Reliable directory hierarchy copy algorithm.
| ✔ |   |   | Reliable directory hierarchy update (two and three way) algorithm.
| ✔ |   |   | Algorithm to replace all duplicate content with hard links.
| ✔ |   |   | Algorithm to figure out all paths for a hard linked inode.
| ✔ |   |   | Algorithm to compare two or three directory enumerations and give differences. Probably blocked on the Ranges TS.

Features possibly to be added after a Boost peer review:
- Directory change monitoring.


<table width="100%" border="0" cellpadding="4">
<tr>
<th colspan="3">Why you might need AFIO<hr></th>
</tr>
<tr>
<td valign="top" width="33%">
Manufacturer claimed 4Kb transfer latencies for the physical hardware:
- Spinning rust hard drive latency @ QD1: **9000us**
- SATA flash drive latency @ QD1: **800us**
- NVMe flash drive latency @ QD1: **300us**
- RTT UDP packet latency over a LAN: **60us**
- NVMe Optane drive latency @ QD1: **60us**
- `memcpy(4Kb)` latency: **5us** (main memory) to **1.3us** (L3 cache)
- RTT PCIe latency: **0.5us**
</td>
<td valign="top" width="33%">
100% read QD1 4Kb direct transfer latencies for the software with AFIO:
- &lt; 99% spinning rust hard drive latency: Windows **187,231us** FreeBSD **9,836us** Linux **26,468us**
- &lt; 99% SATA flash drive latency: Windows **290us** Linux **158us**
- &lt; 99% NVMe drive latency: Windows **37us** FreeBSD **70us** Linux **30us**
</td>
<td valign="top" width="33%">
75% read 25% write QD4 4Kb direct transfer latencies for the software with AFIO:
- &lt; 99% spinning rust hard drive latency: Windows **48,185us** FreeBSD **61,834us** Linux **104,507us**
- &lt; 99% SATA flash drive latency: Windows **1,812us** Linux **1,416us**
- &lt; 99% NVMe drive latency: Windows **50us** FreeBSD **143us** Linux **40us**
</td>
</tr>
</table>

Max bandwidth for the physical hardware:
- DDR4 2133: **30Gb/sec** (main memory)
- x4 PCIe 4.0: **7.5Gb/sec** (arrives end of 2017, the 2018 NVMe drives will use PCIe 4.0)
- x4 PCIe 3.0: **3.75Gb/sec** (985Mb/sec per PCIe lane)
- 2017 XPoint drive (x4 PCIe 3.0): **2.5Gb/sec**
- 2017 NVMe flash drive (x4 PCIe 3.0): **2Gb/sec**
- 10Gbit LAN: **1.2Gb/sec**
