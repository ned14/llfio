#!/bin/sh
if [ -z "$CXX" ]; then
  if [ "$HOSTTYPE" = "FreeBSD" ]; then
    CXX=clang++
  else
    CXX=g++
  fi
fi
HOSTOS=$(uname)
if [ "$HOSTOS" = "Linux" ]; then
  LIBATOMIC="-ldl"
  if [ "$CXX" != "${CXX#clang++}" ] && [ "$NODE_NAME" = "linux-gcc-clang" ]; then
    LIBATOMIC="$LIBATOMIC -latomic"
  fi
fi
if [ "$HOSTOS" = "FreeBSD" ]; then
  LIBATOMIC="-I/usr/local/include -L/usr/local/lib -lexecinfo"
fi
if [ ! -d asio ]; then
  sh -c "git clone https://github.com/chriskohlhoff/asio.git"
fi
cd test
sh ./test_file_glob.sh
cd ..
rm -rf test_all
$CXX -o test_all -g -O3 -std=c++11 -rdynamic -fstrict-aliasing -Wstrict-aliasing -Wno-unused -fasynchronous-unwind-tables test/test_all_multiabi.cpp detail/SpookyV2.cpp -DBOOST_THREAD_VERSION=3 -Wno-constexpr-not-const -Wno-c++1y-extensions -Wno-unused-value -I ~/boost-release -Iinclude -Itest -Iasio/asio/include -lboost_thread -lboost_chrono -lboost_filesystem -lboost_system -lpthread $LIBATOMIC
