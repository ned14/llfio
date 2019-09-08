cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if(NOT TARGET llfio::hl)
  add_subdirectory(.. llfio EXCLUDE_FROM_ALL)
endif()

project(llfio-programs VERSION 1.0 LANGUAGES CXX)

# Looks like cmake's toolset for LLVM-vs* has some serious problems
if(CMAKE_GENERATOR_TOOLSET MATCHES "LLVM-vs.*")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Z7")
endif()

function(make_program program)
  add_executable(${program} "${program}/main.cpp")
  # Hack for sibling config
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../.quickcpplib_use_siblings")
    target_include_directories(${program} PRIVATE "../..")
    target_include_directories(${program} PRIVATE "../../.quickcpplib_use_siblings")
    target_include_directories(${program} PRIVATE "../../.quickcpplib_use_siblings/a")
    target_include_directories(${program} PRIVATE "../../.quickcpplib_use_siblings/a/a")
  else()
    target_include_directories(${program} PRIVATE "../test")
  endif()
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

make_program(benchmark-iostreams llfio::hl)
make_program(benchmark-locking llfio::hl)
make_program(fs-probe llfio::hl)
make_program(illegal-codepoints llfio::hl)
make_program(key-value-store llfio::hl)

if(NOT WIN32)
  add_subdirectory(collision-check)
endif()
