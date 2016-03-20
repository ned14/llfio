if not exist asio git clone https://github.com/chriskohlhoff/asio.git
cd asio
git pull
cd ..

IF "%VisualStudioVersion%" == "14.0" (
cl /Zi /EHsc /O2 /arch:SSE2 /MD /GF /GR /Gy /bigobj /wd4503 test\test_all_multiabi.cpp detail\SpookyV2.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /DBOOST_THREAD_VERSION=3 /Iinclude /Itest /Iasio/asio/include /I..\.. /link /LIBPATH:..\..\stage\lib
) ELSE (
echo Sorry need inline namespace support for this
)
