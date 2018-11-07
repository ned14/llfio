# Build instructions

These compilers and OS are regularly tested:
- GCC 7.0 (Linux 4,x x64)
- clang 4.0 (Linux 4.x x64)
- clang 5.0 (OS X 10.12 x64)
- Visual Studio 2017 (Windows 10 x64)

Other compilers, architectures and OSs may work, but are not tested regularly. You will
need a working Filesystem TS implementation in your STL, and C++ 14. 

## Get a copy of the source

- https://dedi5.nedprod.com/static/files/llfio-v2.0-source-latest.tar.xz

Or from github:

~~~
git config --system core.longpaths true
git clone https://github.com/ned14/llfio.git
cd llfio
git submodule update --init --recursive
~~~

## Header only usage

LLFIO defaults to header only library configuration, so you don't actually need any of the prebuilt
binaries below, or to build anything. Simply:

~~~cpp
#include "llfio/include/llfio.hpp"
~~~

## Prebuilt binaries

It is faster to build programs using LLFIO if you don't user header only build.
In this situation, define `LLFIO_HEADERS_ONLY=0`, and choose one of `LLFIO_DYN_LINK`
or `LLFIO_STATIC_LINK` depending on whether you are using the prebuilt shared or
static libraries respectively.

- https://dedi5.nedprod.com/static/files/llfio-v2.0-binaries-linux64-latest.tgz
- https://dedi5.nedprod.com/static/files/llfio-v2.0-binaries-win64-latest.zip

## Build static libraries from source

You will need cmake installed, v3.1 or better.

To build and test on POSIX (make, ninja etc):

~~~
mkdir build
cd build
cmake ..
cmake --build .
ctest -R llfio_sl
~~~

To build and test on Windows or Mac OS (Visual Studio, XCode etc):

~~~
mkdir build
cd build
cmake .. -G<your generator here>
cmake --build . --config Release
ctest -C Release -R llfio_sl
~~~

## Build shared libraries from source

You will need cmake installed, v3.1 or better.

To build and test on POSIX (make, ninja etc):

~~~
mkdir build
cd build
cmake ..
cmake --build . -- _dl
ctest -R llfio_dl
~~~

To build and test on Windows or Mac OS (Visual Studio, XCode etc):

~~~
mkdir build
cd build
cmake .. -G<your generator here>
cmake --build . --config Release --target _dl
ctest -C Release -R llfio_dl
~~~

## Installing libraries from source

Installing the libraries from cmake does not currently work right due to unfinished
single header generation. It's a todo fixme item.
