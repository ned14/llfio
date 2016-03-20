#!/usr/bin/python3

inputs=[
  ((2, "Android 5.0", "clang 3.5", "libc++", "x86"), {"CPPSTD":["c++11"], "CXX":["g++-4.8"], "LINKTYPE":["standalone"], "label":"android-ndk"}),
  ((0, "Android 5.0", "GCC 4.8", "libc++", "x86"), {"CPPSTD":["c++11"], "CXX":["g++-4.8"], "LINKTYPE":["standalone"], "label":"android-ndk"}),
  ((1, "FreeBSD 10.1 on ZFS", "clang 3.4", "libc++", "x86"), {"CPPSTD":["c++11"], "CXX":["clang++-3.3"], "label":"freebsd10-clang3.3"}),

  ((2, "Ubuntu Linux 12.04 LTS", "clang 3.3", "libstdc++ 4.8", "x86"), {"CPPSTD":["c++11"], "CXX":["clang++-3.3"], "label":"linux-gcc-clang"}),
  ((0, "Ubuntu Linux 12.04 LTS", "clang 3.4", "libstdc++ 4.8", "x86"), {"CPPSTD":["c++11"], "CXX":["clang++-3.4"], "label":"linux-gcc-clang"}),

  ((8, "Ubuntu Linux 14.04 LTS", "clang 3.5", "libstdc++ 4.9", "x64"), {"CPPSTD":["c++11"], "CXX":["clang++-3.5"], "label":"linux64-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "clang 3.5", "libstdc++ 4.9", "ARMv7"), {"CPPSTD":["c++11"], "CXX":["clang++-3.5"], "label":"arm-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "clang 3.6", "libstdc++ 4.9", "x64"), {"CXX":["clang++-3.6"], "label":"linux64-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "clang 3.7", "libstdc++ 4.9", "x64"), {"CXX":["clang++-3.7"], "label":"linux64-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "GCC 4.8", "libstdc++ 4.8", "x64"), {"CPPSTD":["c++11"], "CXX":["g++-4.8"], "label":"linux64-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "GCC 4.9", "libstdc++ 4.9", "x64"), {"CXX":["g++-4.9"], "label":"linux64-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "GCC 4.9", "libstdc++ 4.9", "ARMv7"), {"CXX":["g++-4.9"], "label":"arm-gcc-clang"}),
  ((0, "Ubuntu Linux 14.04 LTS", "GCC 5.1", "libstdc++ 5.1", "x64"), {"CXX":["g++-5"], "label":"linux64-gcc-clang"}),

  ((3, "Microsoft Windows 8.1", "Mingw-w64 GCC 4.8", "libstdc++ 4.8", "x64"), {"CPPSTD":["c++11"], "CXX":["mingw64"], "LINKTYPE":["static", "shared"], "label":"win8-msvc-mingw"}),
  ((0, "Microsoft Windows 8.1", "Visual Studio 2015", "Dinkumware", "x64"), {"CPPSTD":["c++14"], "CXX":["msvc-14.0"], "label":"win8-msvc-mingw"}),
]
CPPSTDs=["c++11", "c++14"]
CXXs=["g++-4.8", "g++-4.9", "g++-5"]
LINKTYPEs=["static", "shared", "standalone"]

print('''<p align="center">
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
<tr align="center"><td>VS2013</td><td></td><td></td><td>
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
''')

# <a href='https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=static,label=android-ndk/'><img src='https://ci.nedprod.com/job/Boost.AFIO%20Build/CPPSTD=c++11,CXX=g++-4.8,LINKTYPE=static,label=android-ndk/badge/icon'></a>

for line, items in inputs:
  if line[0]==0:
    line=("",)+line[2:]
  else:
    line=('<td rowspan="'+str(line[0])+'">'+line[1]+'</td>',)+line[2:]
  print('<tr align="center">%s<td>%s</td><td>%s</td><td>%s</td><td>' % line)
  label=items["label"]
  cppstds=items["CPPSTD"] if "CPPSTD" in items else CPPSTDs
  cxxs=items["CXX"] if "CXX" in items else CXXs
  linktypes=items["LINKTYPE"] if "LINKTYPE" in items else LINKTYPEs
  for cppstd in cppstds:
    for cxx in cxxs:
      for linktype in linktypes:
        print('  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%%20Build/CPPSTD=%s,CXX=%s,LINKTYPE=%s,label=%s/"><img src="https://ci.nedprod.com/job/Boost.AFIO%%20Build/CPPSTD=%s,CXX=%s,LINKTYPE=%s,label=%s/badge/icon" /></a></div>' % (cppstd, cxx, linktype, label, cppstd, cxx, linktype, label))
  print('</td><td>')
  for cppstd in cppstds:
    for cxx in cxxs:
      for linktype in linktypes:
        print('  <div><a href="https://ci.nedprod.com/job/Boost.AFIO%%20Test/CPPSTD=%s,CXX=%s,LINKTYPE=%s,label=%s/"><img src="https://ci.nedprod.com/job/Boost.AFIO%%20Test/CPPSTD=%s,CXX=%s,LINKTYPE=%s,label=%s/badge/icon" /></a></div>' % (cppstd, cxx, linktype, label, cppstd, cxx, linktype, label))
  print('</td></tr>')
  
print('''</table>

</center>
''')
