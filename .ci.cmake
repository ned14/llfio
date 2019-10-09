# CTest script for a CI to submit to CDash a run of configuration,
# building and testing
cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
include(cmake/QuickCppLibBootstrap.cmake)
include(QuickCppLibUtils)
message("*** CTEST_CONFIGURE_OPTIONS = ${CTEST_CONFIGURE_OPTIONS}")

CONFIGURE_CTEST_SCRIPT_FOR_CDASH("llfio" "prebuilt")
list(APPEND CTEST_CONFIGURE_OPTIONS -DCMAKE_BUILD_TYPE=${CTEST_CONFIGURATION_TYPE})
message("*** CTEST_CONFIGURE_OPTIONS = ${CTEST_CONFIGURE_OPTIONS}")
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
include(FindGit)
set(CTEST_GIT_COMMAND "${GIT_EXECUTABLE}")

ctest_start("Experimental")
ctest_update()
ctest_configure(OPTIONS ${CTEST_CONFIGURE_OPTIONS})
ctest_build(TARGET _dl)
ctest_build(TARGET _sl)
set(retval 0)
if(NOT CTEST_DISABLE_TESTING)
  if(WIN32)
    # Appveyor's Windows version doesn't permit unprivileged creation of symbolic links
    ctest_test(RETURN_VALUE retval EXCLUDE "llfio_hl|shared_fs_mutex|symlink")
  else()
    ctest_test(RETURN_VALUE retval EXCLUDE "llfio_hl|shared_fs_mutex")
  endif()
endif()
if(WIN32)
  if(EXISTS "prebuilt/bin/Release/llfio_dl-2.0-Windows-x64-Release.dll")
    checked_execute_process("Tarring up binaries 1"
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/bin/Release
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/lib/Release
      COMMAND xcopy doc llfio\\doc\\ /s /q
      COMMAND xcopy include llfio\\include\\ /s /q
    )
    checked_execute_process("Tarring up binaries 2"
      COMMAND "${CMAKE_COMMAND}" -E copy Readme.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy release_notes.md llfio/
    )
    checked_execute_process("Tarring up binaries 3"
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/llfio_sl-2.0-Windows-x64-Release.lib llfio/prebuilt/lib/Release/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/ntkernel-error-category_sl.lib llfio/prebuilt/lib/Release/
    )
    checked_execute_process("Tarring up binaries 4"
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/llfio_dl-2.0-Windows-x64-Release.lib llfio/prebuilt/lib/Release/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/ntkernel-error-category_dl.lib llfio/prebuilt/lib/Release/
    )
    checked_execute_process("Tarring up binaries 5"
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/bin/Release/llfio_dl-2.0-Windows-x64-Release.dll llfio/prebuilt/bin/Release/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/bin/Release/ntkernel-error-category_dl.dll llfio/prebuilt/bin/Release/
    )
    checked_execute_process("Tarring up binaries final"
      COMMAND 7z a -tzip llfio-v2.0-binaries-win64.zip llfio\\
    )
    get_filename_component(toupload llfio-v2.0-binaries-win64.zip ABSOLUTE)
  endif()
else()
  if(EXISTS "prebuilt/lib/libllfio_dl-2.0-Linux-x86_64-Release.so")
    checked_execute_process("Tarring up binaries"
      COMMAND mkdir llfio
      COMMAND cp -a doc llfio/
      COMMAND cp -a include llfio/
      COMMAND cp -a Readme.md llfio/
      COMMAND cp -a release_notes.md llfio/
      COMMAND cp -a --parents prebuilt/lib/libllfio_sl-2.0-Linux-x86_64-Release.a llfio/
      COMMAND cp -a --parents prebuilt/lib/libllfio_dl-2.0-Linux-x86_64-Release.so llfio/
      COMMAND "${CMAKE_COMMAND}" -E tar cfz llfio-v2.0-binaries-linux-x64.tgz llfio
    )
    get_filename_component(toupload llfio-v2.0-binaries-linux-x64.tgz ABSOLUTE)
  endif()
  if(EXISTS "prebuilt/lib/libllfio_dl-2.0-Linux-armhf-Release.so")
    checked_execute_process("Tarring up binaries"
      COMMAND mkdir llfio
      COMMAND cp -a doc llfio/
      COMMAND cp -a include llfio/
      COMMAND cp -a Readme.md llfio/
      COMMAND cp -a release_notes.md llfio/
      COMMAND cp -a --parents prebuilt/lib/libllfio_sl-2.0-Linux-armhf-Release.a llfio/
      COMMAND cp -a --parents prebuilt/lib/libllfio_dl-2.0-Linux-armhf-Release.so llfio/
      COMMAND "${CMAKE_COMMAND}" -E tar cfz llfio-v2.0-binaries-linux-armhf.tgz llfio
    )
    get_filename_component(toupload llfio-v2.0-binaries-linux-armhf.tgz ABSOLUTE)
  endif()
  if(EXISTS "prebuilt/lib/libllfio_dl-2.0-Darwin-x86_64-Release.so")
    checked_execute_process("Tarring up binaries"
      COMMAND mkdir llfio
      COMMAND cp -a doc llfio/
      COMMAND cp -a include llfio/
      COMMAND cp -a Readme.md llfio/
      COMMAND cp -a release_notes.md llfio/
      COMMAND cp -a --parents prebuilt/lib/libllfio_sl-2.0-Darwin-x86_64-Release.a llfio/
      COMMAND cp -a --parents prebuilt/lib/libllfio_dl-2.0-Darwin-x86_64-Release.dylib llfio/
      COMMAND "${CMAKE_COMMAND}" -E tar cfz llfio-v2.0-binaries-darwin-x64.tgz llfio
    )
    get_filename_component(toupload llfio-v2.0-binaries-darwin-x64.tgz ABSOLUTE)
  endif()
endif()
set(retval2 0)
set(retval3 0)
if(NOT CTEST_DISABLE_TESTING)
  if(("$ENV{CXX}" MATCHES "clang") OR ("$ENV{CXX}" MATCHES "g\\+\\+"))
    if("$ENV{CXX}" MATCHES "clang")
      ctest_build(TARGET _sl-asan)
      set(CTEST_CONFIGURATION_TYPE "asan")
      ctest_test(RETURN_VALUE retval2 INCLUDE "llfio_sl" EXCLUDE "shared_fs_mutex")
    else()
      set(retval2 0)
    endif()
    ctest_build(TARGET _sl-ubsan)
    set(CTEST_CONFIGURATION_TYPE "ubsan")
    ctest_test(RETURN_VALUE retval3 INCLUDE "llfio_sl" EXCLUDE "shared_fs_mutex")
  endif()
  merge_junit_results_into_ctest_xml()
endif()
if(EXISTS "${toupload}")
  ctest_upload(FILES "${toupload}")
endif()
ctest_submit()
if(NOT retval EQUAL 0 OR NOT retval2 EQUAL 0 OR NOT retval3 EQUAL 0)
  message(FATAL_ERROR "FATAL: Running tests exited with ${retval} ${retval2} ${retval3}")
endif()
