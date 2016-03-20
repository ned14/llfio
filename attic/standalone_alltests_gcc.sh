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
$CXX -o test_all -g -O3 -DNDEBUG -std=c++11 -rdynamic -fstrict-aliasing -Wstrict-aliasing -Wno-unused -fasynchronous-unwind-tables test/test_all.cpp detail/SpookyV2.cpp -Iinclude -Itest -DAFIO_STANDALONE=1 -Iasio/asio/include -DSPINLOCK_STANDALONE=1 -DASIO_STANDALONE=1  -DBOOST_AFIO_RUNNING_IN_CI=1 -Wno-unused-value -lboost_filesystem -lboost_system -lpthread $LIBATOMIC
