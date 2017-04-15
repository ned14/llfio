# CTest script for a CI to submit to CDash a run of configuration,
# building and testing
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
# Bring in the Boost lite cmake tooling
list(FIND CMAKE_MODULE_PATH "boost-lite" boost_lite_idx)
if(${boost_lite_idx} EQUAL -1)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../boost-lite/cmake")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../boost-lite/cmake")
    set(CTEST_BOOSTLITE_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/../boost-lite/scripts")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
    set(CTEST_BOOSTLITE_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/scripts")
  else()
    message(FATAL_ERROR "FATAL: A copy of boost-lite cannot be found. Try running 'git submodule update --init --recursive'")
  endif()
endif()
include(BoostLiteUtils)


CONFIGURE_CTEST_SCRIPT_FOR_CDASH("afio" "prebuilt")
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
include(FindGit)
set(CTEST_GIT_COMMAND "${GIT_EXECUTABLE}")

ctest_start("Experimental")
ctest_update()
ctest_configure()
ctest_build(TARGET _dl)
ctest_build(TARGET _sl)
ctest_test(RETURN_VALUE retval EXCLUDE afio_hl)
merge_junit_results_into_ctest_xml()
if(WIN32)
  if(EXISTS prebuilt/bin/Release/afio_dl-2.0-Windows-x64-Release.dll)
    checked_execute_process("Tarring up binaries"
      COMMAND mkdir afio
      COMMAND "${CMAKE_COMMAND}" -E copy
      doc
      include
      Readme.md
      release_notes.md
      prebuilt/lib/Release/afio_sl-2.0-Windows-x64-Release.lib
      prebuilt/lib/Release/afio_dl-2.0-Windows-x64-Release.lib
      prebuilt/bin/Release/afio_dl-2.0-Windows-x64-Release.dll
      afio/
      COMMAND "${CMAKE_COMMAND}" -E tar cfz afio_v2_binaries_win64.tar.gz afio
    )
    get_filename_component(toupload afio_v2_binaries_win64.tar.gz ABSOLUTE)
  endif()
else()
  if(EXISTS prebuilt/lib/libafio_dl-2.0-Linux-x86_64-Release.so)
    checked_execute_process("Tarring up binaries"
      COMMAND mkdir afio
      COMMAND "${CMAKE_COMMAND}" -E copy
      doc
      include
      Readme.md
      release_notes.md
      prebuilt/lib/libafio_sl-2.0-Linux-x86_64-Release.a
      prebuilt/lib/libafio_dl-2.0-Linux-x86_64-Release.so
      afio/
      COMMAND "${CMAKE_COMMAND}" -E tar cfz afio_v2_binaries_linux64.tgz afio
    )
    get_filename_component(toupload afio_v2_binaries_linux64.tgz ABSOLUTE)
  endif()
endif()
if(EXISTS "${toupload}")
  ctest_upload(FILES "${toupload}")
endif()
ctest_submit()
if(NOT retval EQUAL 0)
  message(FATAL_ERROR "FATAL: Running tests exited with ${retval}")
endif()
