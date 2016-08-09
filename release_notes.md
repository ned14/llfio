Herein lies the beginnings of the proposed Boost.AFIO v2 post-peer-review rewrite. Its github
source code repository lives at https://github.com/ned14/boost.afio.

<b>master branch test status</b> Linux: [![Build Status](https://travis-ci.org/ned14/boost.afio.svg?branch=master)](https://travis-ci.org/ned14/boost.afio) Windows: [![Build status](https://ci.appveyor.com/api/projects/status/ox59o2r276xbmef7/branch/master?svg=true)](https://ci.appveyor.com/project/ned14/boost-afio/branch/master) Coverage: Boost.KernelTest support for coveralls.io still todo <b>CMake dashboard</b>: http://my.cdash.org/index.php?project=Boost.AFIO

\note Note that this code is so early alpha that no substantial test code exists
yet. Nobody should use this code for anything serious.

You need these compilers or better:
- GCC 5.0
- clang 3.7
- VS2015 Update 2
- clang 3.7 with Microsoft Codegen ("winclang")

It has two mandatory dependencies:

1. https://github.com/ned14/boost-lite, a minimal emulation of Boost for C++ 11.
2. https://github.com/ned14/boost.outcome, a factory and family of policy driven lightweight monads with
the specialisations of `outcome<T>`, `result<T>` and `option<T>`. AFIO v2 leans heavily
on `result<T>` especially, almost every AFIO v2 API returns one of those.


## Architecture and design:

| NEW in v2 | Boost peer review feedback |     |
| --------- | -------------------------- | --- |
| ✔ | ✔ | Universal native handle/fd abstraction instead of `void *`.
| ✔ | ✔ | Perfectly/Ideally low memory (de)allocation per op (usually none).
| ✔ | ✔ | noexcept API throughout returning error_code for failure instead of throwing exceptions.
| ✔ | ✔ | AFIO v1 handle type split into hierarchy of types:<ol><li>handle - provides open, close, get path, clone, set/unset append only, change caching, characteristics<li>io_handle - adds synchronous scatter-gather i/o, byte range locking<li>file_handle - adds open/create file, get and set maximum extent<li>async_file_handle - adds asynchronous scatter-gather i/o</ol>
| ✔ | ✔ | Cancelable i/o (made possible thanks to dropping XP support).
| ✔ | ✔ | All shared_ptr usage removed as all use of multiple threads removed.
| ✔ | ✔ | Use of std::vector to transport scatter-gather sequences replaced with C++ 2x `span<>` borrowed views.
| ✔ | ✔ | Completion callbacks are now some arbitrary type `U&&` instead of a future continuation. Type erasure for its storage is bound into the one single memory allocation for everything needed to execute the op, and so therefore overhead is optimal.
| ✔ | ✔ | Filing system algorithms made generic and broken out into public `afio::algorithms` template library (the AFIO FTL).
| ✔ | ✔ | Abstraction of native handle management via bitfield specified "characteristics".
| ✔ |   | Storage profiles, a YAML database of behaviours of hardware, OS and filing system combinations.
| ✔ |   | Absolute and interval deadline timed i/o throughout (made possible thanks to dropping XP support).
| ✔ |   | Dependency on ASIO/Networking TS removed completely.
| ✔ |   | Three choices of algorithm implementing a shared filing system mutex.
| ✔ |   | Uses CMake, CTest, CDash and CPack with automatic usage of C++ Modules or precompiled headers where available.
| P |   | New multithreaded kernel based testing infrastructure based on LLVM which can permute/fuzz/<b>edge</b> coverage/mock each test kernel with choices of asan/lsan/msan/ubsan/none sanitisation. This new test infrastructure should make possible eventual <b>formal proof</b> that AFIO's implementation is mathematically correct.


## Features implemented:

| NEW in v2 | Windows | POSIX |     |
| --------- | --------| ----- | --- |
| ✔ | ✔ | ✔ | Native handle cloning.
| ✔ (up from four) | ✔ | ✔ | Maximum possible (seven) forms of kernel caching.
|   | ✔ | ✔ | Absolute path open.
|   |   |   | Relative path open ("fat paths").
| ✔ | ✔ |   | Win32 path support (260 path limit).
|   |   |   | NT kernel path support (32,768 path limit).
| ✔ | ✔ | ✔ | Synchronous universal scatter-gather i/o.
| ✔ (POSIX AIO support) | ✔ | ✔ | Asynchronous universal scatter-gather i/o.
| ✔ | ✔ | ✔ | i/o cancellation.
|   | ✔ | ✔ | Retrieving and setting the current maximum extent (size) of an open file.
|   | ✔ | ✔ | statfs_t ported over from AFIO v1.
|   | ✔ | ✔ | utils namespace ported over from AFIO v1.
| ✔ | ✔ | ✔ | `shared_fs_mutex` shared/exclusive entities locking based on lock files
| ✔ | ✔ | P | Byte range shared/exclusive locking.
| ✔ | ✔ | P | `shared_fs_mutex` shared/exclusive entities locking based on byte ranges
| ✔ | ✔ | P | `shared_fs_mutex` shared/exclusive entities locking based on atomic append
|   | P | P | Memory mapped files (`mapped_file_handle`)
| ✔ | P | P | `shared_fs_mutex` shared/exclusive entities locking based on memory maps

