if not exist asio git clone https://github.com/chriskohlhoff/asio.git
cd asio
git pull
cd ..

IF "%VisualStudioVersion%" == "14.0" (
  rem Totally standalone edition (VS2015 required)
  if "%1" == "single_include" (
    include\boost\afio\bindlib\scripts\GenSingleHeader.py -DAFIO_STANDALONE=1 -DSPINLOCK_STANDALONE=1 -DASIO_STANDALONE=1 include/boost/afio/afio.hpp > test\single_include_test_all.cpp
    type test\test_all.cpp >> test\single_include_test_all.cpp
    cl /Zi /EHsc /O2 /DNDEBUG /arch:SSE2 /MD /GF /GR /Gy /bigobj /wd4503 test\single_include_test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /Iinclude /Itest /Iasio/asio/include /DBOOST_AFIO_RUNNING_IN_CI=1    
  ) else (
    cl /Zi /EHsc /O2 /DNDEBUG /arch:SSE2 /MD /GF /GR /Gy /bigobj /wd4503 test\test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /Iinclude /Itest /DAFIO_STANDALONE=1 /Iasio/asio/include /DSPINLOCK_STANDALONE=1 /DASIO_STANDALONE=1 /DBOOST_AFIO_RUNNING_IN_CI=1
  )
) ELSE (
  rem Needs filesystem
  cl /Zi /EHsc /O2 /DNDEBUG /MD /GF /GR /Gy /bigobj /wd4503 test\test_all.cpp detail\SpookyV2.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /Iinclude /Itest /DAFIO_STANDALONE=1 /Iasio/asio/include /DSPINLOCK_STANDALONE=1 /DASIO_STANDALONE=1 /DBOOST_AFIO_RUNNING_IN_CI=1 /I..\boost-release /link  ..\boost-release\stage\lib\libboost_filesystem-vc120-mt-1_57.lib ..\boost-release\stage\lib\libboost_system-vc120-mt-1_57.lib
)
