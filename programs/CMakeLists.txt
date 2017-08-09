cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if(NOT TARGET afio::hl)
  add_subdirectory(.. afio EXCLUDE_FROM_ALL)
endif()

project(afio-programs VERSION 1.0 LANGUAGES CXX)

# Looks like cmake's toolset for LLVM-vs* has some serious problems
if(CMAKE_GENERATOR_TOOLSET MATCHES "LLVM-vs.*")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Z7")
endif()

function(make_program program)
  add_executable(${program} "${program}/main.cpp")
  target_include_directories(${program} PRIVATE ..)
  if(WIN32)
    target_compile_definitions(${program} PRIVATE _UNICODE UNICODE)
    # cmake's support for LLVM clang is shocking :(
    if(CMAKE_GENERATOR_TOOLSET MATCHES "LLVM-vs.*")
      target_compile_options(${program} PRIVATE /EHsc)
    endif()
  endif()
  target_link_libraries(${program} ${ARGN})
  set_target_properties(${program} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    POSITION_INDEPENDENT_CODE ON
  )
endfunction()

make_program(benchmark-locking afio::hl)
make_program(fs-probe afio::hl)
