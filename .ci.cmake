# CTest script for a CI to submit to CDash a run of configuration,
# building and testing
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
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


configure_ctest_script_for_cdash("afio" "build")
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})

ctest_start("Experimental")
ctest_configure()
ctest_build()
ctest_test()
merge_junit_results_into_ctest_xml()
if(WIN32)
  if(EXISTS build/bin/Release/afio_dl-2.0-Windows-x64-Release.dll)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cfz afio_v2_binaries_win64.tar.gz
      doc
      include
      Readme.md
      release_notes.md
      build/lib/Release/afio_sl-2.0-Windows-x64-Release.lib
      build/lib/Release/afio_dl-2.0-Windows-x64-Release.lib
      build/bin/Release/afio_dl-2.0-Windows-x64-Release.dll
      RESULT_VARIABLE retval
    )
    message(STATUS "Tarring up binaries returned with status ${retval}")
    execute_process(COMMAND dir /N)
    ctest_upload(FILES afio_v2_binaries_win64.tar.gz)
  endif()
else()
  if(EXISTS build/bin/Release/afio_dl-2.0-Linux-x86_64-Release.so)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E tar cfz afio_v2_binaries_linux64.tgz
      doc
      include
      Readme.md
      release_notes.md
      build/lib/Release/afio_sl-2.0-Linux-x86_64-Release.a
      build/bin/Release/afio_dl-2.0-Linux-x86_64-Release.so
      RESULT_VARIABLE retval
    )
    message(STATUS "Tarring up binaries returned with status ${retval}")
    ctest_upload(FILES afio_v2_binaries_linux64.tgz)
  endif()
endif()
ctest_submit()
