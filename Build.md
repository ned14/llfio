# Build instructions

These compilers and OSs are regularly tested by CI:

- [GCC](https://gcc.gnu.org/) on Linux
- [Clang](https://clang.llvm.org/) on Linux
- Xcode on Mac OS
- Visual Studio on Windows

Other compilers, architectures and OSs may work, but are not tested regularly.
You will need a working Filesystem implementation in your STL, and at least C++ 17.

LLFIO has your choice of header-only, static library, and shared library build modes.
Note that on Microsoft Windows, the default header only configuration is unsafe
to use outside of toy projects. You will get warnings of the form:

~~~
warning : LLFIO_HEADERS_ONLY=1, LLFIO_EXPERIMENTAL_STATUS_CODE=0 and NTKERNEL_ERROR_CATEGORY_INLINE=1 on Windows, this can produce unreliable binaries where semantic comparisons of error codes randomly fail!
~~~

... and ...

~~~
Defining custom error code category ntkernel_category() via header only form is unreliable! Semantic comparisons will break! Define NTKERNEL_ERROR_CATEGORY_INLINE to 0 and only ever link in ntkernel_category() from a prebuilt shared library to avoid this problem.
~~~

The cause is that `<system_error>` has a design flaw where custom error code categories
are unsafe when shared libraries are in use.

You have one of three choices here: (i) Use [experimental SG14 `status_code`](https://wg21.link/P1028)
which doesn't have this problem (define `LLFIO_EXPERIMENTAL_STATUS_CODE=1`)
(ii) Use the NT kernel error category as a shared library ([see its
documentation](https://github.com/ned14/ntkernel-error-category)) (iii)
Don't use header only LLFIO on Windows (see below).


## Install from the vcpkg package manager

This is particularly easy, and works on Mac OS, Linux and Microsoft Windows:

```
vcpkg install llfio
```

LLFIO appears at `<llfio/llfio.hpp>`.

## Prebuilt binaries

It is faster to build programs using LLFIO if you don't use a header only build.
In this situation, define `LLFIO_HEADERS_ONLY=0`, and choose one of `LLFIO_DYN_LINK`
or `LLFIO_STATIC_LINK` depending on whether you are using the prebuilt shared or
static libraries respectively.

You can find prebuilt binaries for Mac OS, Ubuntu and Microsoft Windows at
https://github.com/ned14/llfio/releases. Choose a release, and under the Assets
you will find the prebuilt binaries packages which include headers.

## Get a copy of the source

Clone from the GitHub repository:

~~~
git config --system core.longpaths true
git clone --recursive https://github.com/ned14/llfio.git
cd llfio
~~~

The first command is relevant so deeply nested paths on Windows will work when
cloning the repository and submodules. It may require elevated privileges, but
you can also use `git config --global core.longpaths true` instead.

### If you already cloned before reading this

If you had already cloned _this_ repository, but didn't use the `--recursive`
switch, you can simply run the following command from inside the work tree:

~~~
git submodule update --init --recursive
~~~

## Build static libraries from source

You will need [CMake](https://cmake.org/) installed, v3.9 or better. It is important to do an out-of-tree build, because the build will otherwise fail.

If you want C++ Coroutines support, you will need a C++ Coroutines supporting compiler. It should get automatically discovered and detected.

To build and test on POSIX (`make`, `ninja` etc):

~~~
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
ctest -R llfio_sl
~~~

To build and test on Windows or Mac OS (Visual Studio, XCode etc):

~~~
mkdir build
cd build
cmake .. -G<your generator here>
cmake --build . --parallel --config Release
ctest -C Release -R llfio_sl
~~~

## Build shared libraries from source

You will need [CMake](https://cmake.org/) installed, v3.9 or better. It is important to do an out-of-tree build, because the build will otherwise fail.

To build and test on POSIX (`make`, `ninja` etc):

~~~
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel -- _dl
ctest -R llfio_dl
~~~

To build and test on Windows or Mac OS (Visual Studio, XCode etc):

~~~
mkdir build
cd build
cmake .. -G<your generator here>
cmake --build . --parallel --config Release --target _dl
ctest -C Release -R llfio_dl
~~~

## Installing libraries from source

If you add llfio as a subdirectory in cmake (`add_subdirectory()`),
you can link to its exported targets and everything 'just works'. The
dependencies quickcpplib and outcome will be automatically downloaded
into the build directory and used.

If you really want to install llfio into a directory, you can also:

~~~
git config --system core.longpaths true

git clone --recursive https://github.com/ned14/quickcpplib.git
cd quickcpplib
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --parallel
cmake --build . --target install
cd ../..

git clone --recursive https://github.com/ned14/outcome.git
cd outcome
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --parallel
cmake --build . --target install
cd ../..

git clone --recursive https://github.com/ned14/llfio.git
cd llfio
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build . --parallel -- _dl _sl _hl
cmake --build . --target install
cd ../..
~~~

If you chose a different `CMAKE_INSTALL_PREFIX` then it will need
to be supplied to the compiler:

~~~
cd example
g++ -std=c++17 -o test map_file.cpp -I/Users/ned/testinstall/inst/include
~~~

The above also works on Microsoft Windows, but paths etc will need to
be adjusted.
