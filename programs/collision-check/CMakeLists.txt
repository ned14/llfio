cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

if(WIN32 OR APPLE)
  message(FATAL_ERROR "FATAL: Not supported on Windows or Mac OS")
endif()

if(NOT TARGET llfio::hl)
  add_subdirectory(../.. llfio EXCLUDE_FROM_ALL)
endif()

project(llfio-collision-check VERSION 1.0 LANGUAGES CXX)

# Two independent shared objects, each of which includes the LLFIO headers
# in header only mode in complete isolation from the other. If header only
# LLFIO ever leaks a non-inline (i.e. externally visible, non-weak) symbol,
# linking both of these into the same executable will either fail outright
# (duplicate symbol at link time) or silently produce an ODR violation
# (two definitions of the same symbol bound together at load time).
foreach(libno 1 2)
  add_library(llfio-collision-check-lib${libno} SHARED lib${libno}.cpp)
  target_compile_definitions(llfio-collision-check-lib${libno} PRIVATE LIBNO=${libno})
  target_link_libraries(llfio-collision-check-lib${libno} PUBLIC llfio::hl)
endforeach()

# Links both shared objects together into one process and calls into both,
# so any collision between them shows up as a link or runtime failure.
add_executable(llfio-collision-check main.cpp)
target_link_libraries(llfio-collision-check PRIVATE llfio-collision-check-lib1 llfio-collision-check-lib2)
add_test(NAME llfio-collision-check COMMAND llfio-collision-check)

# Hack for sibling config
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../.quickcpplib_use_siblings")
  foreach(target llfio-collision-check-lib1 llfio-collision-check-lib2 llfio-collision-check)
    target_include_directories(${target} PRIVATE "../../..")
    target_include_directories(${target} PRIVATE "../../../.quickcpplib_use_siblings")
    target_include_directories(${target} PRIVATE "../../../.quickcpplib_use_siblings/a")
    target_include_directories(${target} PRIVATE "../../../.quickcpplib_use_siblings/a/a")
  endforeach()
endif()

# Add nm -D libllfio-collision-check-lib*.so | grep "T _ZN17llfio_v2"
# which checks for any non-inline (strongly bound, externally visible)
# symbols. If it returns anything for either shared object, fail the build.
foreach(libno 1 2)
  add_custom_command(TARGET llfio-collision-check-lib${libno} POST_BUILD
    COMMAND nm -D $<TARGET_FILE:llfio-collision-check-lib${libno}> > exported-symbols-${libno}.txt
    COMMAND grep 'T _ZN17llfio_v2' exported-symbols-${libno}.txt > non-inline-symbols-${libno}.txt || true
    COMMAND if [ -s non-inline-symbols-${libno}.txt ]\; then echo '\nFATAL: Found non-inline LLFIO symbols in llfio-collision-check-lib${libno}!'\; cat non-inline-symbols-${libno}.txt\; echo ''\; exit 1\; fi
    COMMENT "Checking shared object ${libno} for non-inline LLFIO symbols ..."
  )
endforeach()

