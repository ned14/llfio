set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_STANDARD 20)
# Coroutines are standard in C++20, so the pre-C++20 -fcoroutines-ts flag is no longer set:
# clang 16 removed that flag and clang 18 (Ubuntu 24.04, where libc++ is new enough to have
# <memory_resource>) rejects it outright. quickcpplib's CheckCXXHasCoroutines still adds a
# flag by itself for compilers which need one.
set(CMAKE_CXX_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-stdlib=libc++")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-stdlib=libc++")

