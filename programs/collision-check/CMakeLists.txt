cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if(WIN32)
  message(FATAL_ERROR "FATAL: Not supported on Windows")
endif()

if(NOT TARGET llfio::hl)
  add_subdirectory(../.. llfio EXCLUDE_FROM_ALL)
endif()

project(llfio-collision-check VERSION 1.0 LANGUAGES CXX)

add_library(llfio-collision-check SHARED lib.cpp)
target_compile_definitions(llfio-collision-check PRIVATE LIBNO=1)
target_link_libraries(llfio-collision-check PUBLIC llfio::hl)

# Hack for sibling config
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../.quickcpplib_use_siblings")
  foreach(target llfio-collision-check)
    target_include_directories(${target} PRIVATE "../../..")
    target_include_directories(${target} PRIVATE "../../../.quickcpplib_use_siblings")
    target_include_directories(${target} PRIVATE "../../../.quickcpplib_use_siblings/a")
    target_include_directories(${target} PRIVATE "../../../.quickcpplib_use_siblings/a/a")
  endforeach()
endif()

# Add nm -D libllfio-collision-check-lib1.so | grep "T _ZN17llfio_v2"
# which checks for any non-inline symbols. If it returns anything, fail the build.
add_custom_command(TARGET llfio-collision-check POST_BUILD
  COMMAND nm -D $<TARGET_FILE:llfio-collision-check> > exported-symbols.txt
  COMMAND grep 'T _ZN17llfio_v2' exported-symbols.txt > non-inline-symbols.txt || true
  COMMAND if [ -s non-inline-symbols.txt ]\; then echo '\\nFATAL: Found non-inline LLFIO symbols!'\; cat non-inline-symbols.txt\; echo ''\; exit 1\; fi
  COMMENT "Checking shared object for non-inline LLFIO symbols ..."
)
