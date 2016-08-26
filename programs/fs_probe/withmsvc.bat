cd ..
if not exist asio git clone https://github.com/chriskohlhoff/asio.git
cd asio
git pull
cd ..\fs_probe

cl /Zi /EHsc /O2 /DNDEBUG /arch:SSE2 /MD /GF /GR /Gy /bigobj /wd4503 fs_probe.cpp /DUNICODE=1 /DWIN32=1 /D_UNICODE=1 /D_WIN32=1 /I../include /DAFIO_STANDALONE=1 /I../asio/asio/include /DASIO_STANDALONE=1
