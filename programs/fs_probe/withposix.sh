#!/bin/sh
if [ -z "$CXX" ]; then
  if [ "$HOSTTYPE" = "FreeBSD" ]; then
    CXX=clang++37
  else
    CXX=g++-4.9
  fi
fi
HOSTOS=$(uname)
if [ "$HOSTOS" = "Linux" ]; then
  LIBATOMIC="-ldl -lrt"
  if [ "$CXX" != "${CXX#clang++}" ] && [ "$NODE_NAME" = "linux-gcc-clang" ]; then
    LIBATOMIC="$LIBATOMIC -latomic"
  fi
fi
if [ "$HOSTOS" = "FreeBSD" ]; then
  LIBATOMIC="-I/usr/local/include -L/usr/local/lib -lexecinfo"
fi
rm -rf fs_probe
$CXX -o fs_probe -g -O3 -DDEBUG -std=c++14 -rdynamic -fstrict-aliasing -Wstrict-aliasing -Wno-unused -fasynchronous-unwind-tables fs_probe.cpp -Iinclude -DBOOST_AFIO_RUNNING_IN_CI=1 -Wno-unused-value -lboost_filesystem -lboost_system -lpthread $LIBATOMIC
