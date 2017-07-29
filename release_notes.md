Herein lies the beginnings of the proposed AFIO v2 post-peer-review rewrite. Its github
source code repository lives at https://github.com/ned14/boost.afio.

<b>master branch test status</b> Linux: <a href="https://travis-ci.org/ned14/afio"><img src="https://travis-ci.org/ned14/afio.svg?branch=master"></a> Windows: <em>disabled due to lack of VS2017.3 on CI</em> <b>CMake dashboard</b>: http://my.cdash.org/index.php?project=AFIO

\note Note that this code is of late alpha quality. Be careful when using it!

You need these compilers or better:
- GCC 7.0
- clang 4.0
- VS2017 Update 3


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
|   |   |   | BSD and OS X kqueues optimised `io_service`
| ✔ |   |   | Linux KAIO support for native async `O_DIRECT` i/o
| ✔ | ✔ | ✔ | i/o deadlines and cancellation.
|   | ✔ | ✔ | Retrieving and setting the current maximum extent (size) of an open file.
|   | ✔ | ✔ | statfs_t ported over from AFIO v1.
|   | ✔ | ✔ | utils namespace ported over from AFIO v1.
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on lock files
| ✔ | ✔ | ✔ | Byte range shared/exclusive locking.
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on byte ranges
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on atomic append
|   | ✔ | ✔ | Memory mapped files and virtual memory management (`section_handle` and `map_handle`)
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on memory maps
| ✔ | ✔ | ✔ | Universal portable UTF-8 path views.

Todo:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
|   |   |   | "Hole punching" and hole enumeration ported over from AFIO v1.
|   |   |   | Directory handles and very fast directory enumeration ported over from AFIO v1.
| ✔ |   |   | Extended attributes support.
|   |   |   | Reliable directory hierarchy deletion algorithm.
|   |   |   | Reliable directory hierarchy copy algorithm.
| ✔ |   |   | Reliable directory hierarchy update (two and three way) algorithm.
| ✔ |   |   | Algorithm to replace all duplicate content with hard links.
| ✔ |   |   | Algorithm to figure out all paths for a hard linked inode.
