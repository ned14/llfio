cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if(NOT TARGET afio::hl)
  add_subdirectory(.. afio EXCLUDE_FROM_ALL)
endif()

project(afio-programs VERSION 1.0 LANGUAGES CXX)

function(make_program program)
  add_executable(${program} "${program}/main.cpp")
  target_include_directories(${program} PRIVATE ..)
  if(WIN32)
    target_compile_definitions(${program} PRIVATE _UNICODE UNICODE)
  endif()
  target_link_libraries(${program} ${ARGN})
  set_target_properties(${program} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    POSITION_INDEPENDENT_CODE ON
  )
endfunction()

make_program(benchmark-locking afio::hl)
make_program(fs-probe afio::hl)
