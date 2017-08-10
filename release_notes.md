<center><table border="0" cellpadding="4">
<tr>
<td align="center"> <a href="https://github.com/ned14/afio">AFIO</a><br><a href="https://github.com/ned14/afio">on GitHub</a> </td>
<td align="center"> <a href="http://my.cdash.org/index.php?project=Boost.AFIO">CTest summary</a><br><a href="http://my.cdash.org/index.php?project=Boost.AFIO">dashboard</a> </td>
<td align="center"> <a href="https://travis-ci.org/ned14/afio">Linux and MacOS CI:</a><img src="https://travis-ci.org/ned14/afio.svg?branch=master"/> </td>
<td align="center"> <a href="https://ci.appveyor.com/project/ned14/afio/branch/master">Windows CI:</a><img src="https://ci.appveyor.com/api/projects/status/680b1pt9srnoprs3/branch/master?svg=true"/> </td>
</tr>
</table></center>

Herein lies my proposed zero whole machine memory copy async file i/o and filesystem
library for Boost and the C++ standard, intended for storage devices with sub-10 microsecond
4Kb read latencies.

It is a complete rewrite after a Boost peer review in August 2015. Its github
source code repository lives at https://github.com/ned14/boost.afio.

<table width="100%" border="0" cellpadding="4">
<tr>
<th colspan="3">Why you might need AFIO<hr></th>
</tr>
<tr>
<td valign="top">
Manufacturer claimed 4Kb transfer latencies for the physical hardware:
- Spinning rust hard drive latency @ QD1: **7000us**
- SATA flash drive latency @ QD1: **800us**
- `memcpy(4Kb)` latency: **500us** (main memory) to **90us** (L2 cache)
- NVMe flash drive latency @ QD1: **300us**
- RTT UDP packet latency over a LAN: **60us**
- XPoint drive latency @ QD1: **10us**
- RTT PCIe latency: **0.5us**
</td>
<td valign="top">
100% read QD1 4Kb transfer latencies for the software with AFIO:
- Average spinning rust hard drive latency: **TODO**
- 99.999% spinning rust hard drive latency: **TODO**
- Average SATA flash drive latency: **TODO**
- 99.999% SATA flash drive latency: **TODO**
- Average NVMe flash drive latency: **98.9us** (10,111 IOPS)
- 99.999% NVMe flash drive latency: **3,146us** (317 IOPS)
</td>
<td valign="top">
75% read 25% write QD4 4Kb transfer latencies for the software with AFIO:
- Average spinning rust hard drive latency: **TODO**
- 99.999% spinning rust hard drive latency: **TODO**
- Average SATA flash drive latency: **TODO**
- 99.999% SATA flash drive latency: **TODO**
- Average NVMe flash drive latency: **344.5us** (11,611 IOPS)
- 99.999% NVMe flash drive latency: **52,164us** (77 IOPS)
</td>
</tr>
</table>

\note Note that this code is of late alpha quality. It's quite reliable, but be careful when using it!

You need these compilers and OS or better:
- GCC 7.0 (Linux)
- clang 4.0 (Linux)
- clang 5.0 (Windows)

Other compilers and OSs may work, but are not tested regularly. You will need a Filesystem TS
implementation in your STL and C++ 14.

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
| ✔ | ✔ | AFIO v1 handle type split into hierarchy of types:<ol><li>handle - provides open, close, get path, clone, set/unset append only, change caching, characteristics<li>path_handle - a race free anchor to a subset of the filesystem<li>io_handle - adds synchronous scatter-gather i/o, byte range locking<li>file_handle - adds open/create file, get and set maximum extent<li>async_file_handle - adds asynchronous scatter-gather i/o</ol>
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
|   | ✔ | ✔ | Memory mapped files and virtual memory management (`section_handle` and `map_handle`)
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on memory maps
| ✔ | ✔ | ✔ | Universal portable UTF-8 path views.
|   | ✔ | ✔ | "Hole punching" and hole enumeration ported over from AFIO v1.

Todo to reach feature parity with AFIO v1:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
|   |   |   | Directory handles and very fast directory enumeration ported over from AFIO v1.
|   |   |   | Hard links and symlinks.
|   |   |   | BSD and OS X kqueues optimised `io_service`

Todo thereafter:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
| ✔ |   |   | Extended attributes support.
| ✔ |   |   | Coroutines TS integration for `async_file_handle`. Use example: https://gist.github.com/anonymous/a67ba4695c223a905ff108ed8b9a342f
| ✔ |   |   | Linux KAIO support for native async `O_DIRECT` i/o
| ✔ |   |   | Reliable directory hierarchy deletion algorithm.
| ✔ |   |   | Reliable directory hierarchy copy algorithm.
| ✔ |   |   | Reliable directory hierarchy update (two and three way) algorithm.
| ✔ |   |   | Algorithm to replace all duplicate content with hard links.
| ✔ |   |   | Algorithm to figure out all paths for a hard linked inode.
