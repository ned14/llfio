Herein lies the beginnings of the proposed Boost.AFIO v2 post-peer-review rewrite. Its github
source code repository lives at https://github.com/ned14/boost.afio.

\note Note that this code is so early alpha that no test code, let alone unit test code, exists
yet. Nobody should use this code for anything serious until the unit tests are in place and
being nightly tested via continuous integration.

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

