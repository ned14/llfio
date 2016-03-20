<p align="center">
<a href="http://boostgsoc13.github.io/boost.afio/">Documentation can be found here</a>
</p>
<h3 align="center">
Boost.AFIO Jenkins CI status:
</h3>
<p align="center">Unit test code coverage is: <a href='https://coveralls.io/r/BoostGSoC13/boost.afio'><img src='https://coveralls.io/repos/BoostGSoC13/boost.afio/badge.png' alt='Coverage Status' /></a></p>
<p align="center">Appveyor: <a href='https://ci.appveyor.com/project/ned14/boost-afio'><img src='https://ci.appveyor.com/api/projects/status/f89cv89kj8c2nmvb/branch/master'/></a></p>

<center>
<table border="1" cellpadding="2">
<tr><th>OS</th><th>Compiler</th><th>STL</th><th>CPU</th><th>Build</th><th>Unit tests</th></tr>

<!-- static analysis clang -->
<tr align="center"><td rowspan="2">Static analysis</td><td>clang 3.7 tidy + static analyser + GCC 4.8</td><td></td><td></td><td>
<div><a href='https://ci.nedprod.com/job/Boost.AFIO%20Static%20Analysis%20clang/'><img src='https://ci.nedprod.com/buildStatus/icon?job=Boost.AFIO%20Static%20Analysis%20clang' /></a></div></td><td></td>
</tr>

<!-- static analysis MSVC -->
<tr align="center"><td>VS2015</td><td></td><td></td><td>
<div><a href='https://ci.nedprod.com/job/Boost.AFIO%20Static%20Analysis%20MSVC/'><img src='https://ci.nedprod.com/buildStatus/icon?job=Boost.AFIO%20Static%20Analysis%20MSVC' /></a></div></td><td></td>
</tr>

<!-- sanitiser -->
<tr align="center"><td>Thread Sanitiser</td><td>clang 3.4</td><td>libstdc++ 4.9</td><td>x64</td><td></td><td>
<div><a href='https://ci.nedprod.com/job/Boost.AFIO%20Sanitise%20Linux%20clang%203.4/'><img src='https://ci.nedprod.com/buildStatus/icon?job=Boost.AFIO%20Sanitise%20Linux%20clang%203.4' /></a></div></td>
</tr>

<!-- valgrind -->
<tr align="center"><td>Valgrind</td><td>GCC 4.8</td><td>libstdc++ 4.8</td><td>x64</td><td></td><td>
<div><a href='https://ci.nedprod.com/job/Boost.AFIO%20Valgrind%20Linux%20GCC%204.8/'><img src='https://ci.nedprod.com/buildStatus/icon?job=Boost.AFIO%20Valgrind%20Linux%20GCC%204.8' /></a></div></td>
</tr>

<!-- sep -->
<tr></tr>

<!-- os x -->
<tr align="center"><td>Apple Mac OS X 10.9</td><td>clang 3.5</td><td>libc++</td><td>x64</td><td>
<div><a href="https://travis-ci.org/BoostGSoC13/boost.afio"><img valign="middle" src="https://travis-ci.org/BoostGSoC13/boost.afio.png?branch=master"/></a></div></td><td>
<div><a href="https://travis-ci.org/BoostGSoC13/boost.afio"><img valign="middle" src="https://travis-ci.org/BoostGSoC13/boost.afio.png?branch=master"/></a></div></td>
</tr>

<tr align="center"><td rowspan="2">Android 5.0</td><td>clang 3.5</td><td>libc++</td><td>x86</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>GCC 4.8</td><td>libc++</td><td>x86</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=android-ndk/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td rowspan="1">FreeBSD 10.1 on ZFS</td><td>clang 3.4</td><td>libc++</td><td>x86</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=freebsd10-clang3.3/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=freebsd10-clang3.3/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=freebsd10-clang3.3/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=freebsd10-clang3.3/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=freebsd10-clang3.3/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=freebsd10-clang3.3/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=freebsd10-clang3.3/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=freebsd10-clang3.3/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=freebsd10-clang3.3/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=freebsd10-clang3.3/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=freebsd10-clang3.3/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=freebsd10-clang3.3/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td rowspan="2">Ubuntu Linux 12.04 LTS</td><td>clang 3.3</td><td>libstdc++ 4.8</td><td>x86</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=linux-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=static,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=shared,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.3,LINKTYPE=standalone,label=linux-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>clang 3.4</td><td>libstdc++ 4.8</td><td>x86</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=static,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=static,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=shared,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=shared,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=standalone,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=standalone,label=linux-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=static,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=static,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=shared,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=shared,label=linux-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=standalone,label=linux-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.4,LINKTYPE=standalone,label=linux-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td rowspan="8">Ubuntu Linux 14.04 LTS</td><td>clang 3.5</td><td>libstdc++ 4.9</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>clang 3.5</td><td>libstdc++ 4.9</td><td>ARMv7</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=arm-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=static,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=shared,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.5,LINKTYPE=standalone,label=arm-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>clang 3.6</td><td>libstdc++ 4.9</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.6,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>clang 3.7</td><td>libstdc++ 4.9</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=clang++-3.7,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>GCC 4.8</td><td>libstdc++ 4.8</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>GCC 4.9</td><td>libstdc++ 4.9</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>GCC 4.9</td><td>libstdc++ 4.9</td><td>ARMv7</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=static,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=shared,label=arm-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-4.9,LINKTYPE=standalone,label=arm-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>GCC 5.1</td><td>libstdc++ 5.1</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-5,LINKTYPE=static,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-5,LINKTYPE=shared,label=linux64-gcc-clang/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=g++-5,LINKTYPE=standalone,label=linux64-gcc-clang/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td rowspan="3">Microsoft Windows 8.1</td><td>Mingw-w64 GCC 4.8</td><td>libstdc++ 4.8</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=mingw64,LINKTYPE=static,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=mingw64,LINKTYPE=static,label=win8-msvc-mingw/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=mingw64,LINKTYPE=shared,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=mingw64,LINKTYPE=shared,label=win8-msvc-mingw/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=mingw64,LINKTYPE=static,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=mingw64,LINKTYPE=static,label=win8-msvc-mingw/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=mingw64,LINKTYPE=shared,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++11,CXX=mingw64,LINKTYPE=shared,label=win8-msvc-mingw/badge/icon" /></a></div>
</td></tr>
<tr align="center"><td>Visual Studio 2015</td><td>Dinkumware</td><td>x64</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=static,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=static,label=win8-msvc-mingw/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=shared,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=shared,label=win8-msvc-mingw/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=standalone,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=standalone,label=win8-msvc-mingw/badge/icon" /></a></div>
</td><td>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=static,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=static,label=win8-msvc-mingw/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=shared,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=shared,label=win8-msvc-mingw/badge/icon" /></a></div>
  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=standalone,label=win8-msvc-mingw/"><img src="https://ci.nedprod.com/job/Boost.AFIO%20Test/CPPSTD=c++14,CXX=msvc-14.0,LINKTYPE=standalone,label=win8-msvc-mingw/badge/icon" /></a></div>
</td></tr>
</table>

</center>

