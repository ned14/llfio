set(CMAKE_C_COMPILER clang-11)
set(CMAKE_CXX_COMPILER clang++-11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-stdlib=libc++ -fchar8_t")
set(CMAKE_EXE_LINKER_FLAGS -stdlib=libc++)

