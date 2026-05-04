set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
## mingw doesn't support __try ... __except
set(CMAKE_CXX_FLAGS_INIT "-DLLFIO_DISABLE_SIGNAL_GUARD=1 -Wno-cast-function-type -Wno-maybe-uninitialized -Wno-unused-local-typedefs -Wno-unknown-pragmas -Wno-c++20-compat")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
