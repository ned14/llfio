# CTest script for a CI to submit to CDash a run of configuration,
# building and testing
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
include(cmake/QuickCppLibBootstrap.cmake)
include(QuickCppLibUtils)

CONFIGURE_CTEST_SCRIPT_FOR_CDASH("llfio" "prebuilt")
#list(APPEND CTEST_CONFIGURE_OPTIONS -DCMAKE_BUILD_TYPE=${CTEST_CONFIGURATION_TYPE} -DCXX_COROUTINES_FLAGS=)
ctest_empty_binary_directory(${CTEST_BINARY_DIRECTORY})
if(WIN32)
  find_program(BASH_COMMAND bash HINTS "C:/Program Files/Git/bin")
  checked_execute_process("Checking bash binary works"
    COMMAND "${BASH_COMMAND}" --version
  )
endif()
include(FindGit)
set(CTEST_GIT_COMMAND "${GIT_EXECUTABLE}")

ctest_start("Experimental")
#ctest_update()
message(STATUS "NOTE: CTEST_CONFIGURE_OPTIONS are '${CTEST_CONFIGURE_OPTIONS}'")
ctest_configure(OPTIONS "${CTEST_CONFIGURE_OPTIONS}")
ctest_build(TARGET _hl)
ctest_build(TARGET _dl)
ctest_build(TARGET _sl)
set(retval 0)
if(NOT CTEST_DISABLE_TESTING)
  # shared_fs_mutex takes too long on CI
  # tls_socket_handle is unstable
  # dynamic_thread_pool_group is unstable
  set(LLFIO_DISABLE_TESTS "shared_fs_mutex|tls_socket_handle|dynamic_thread_pool_group")
  if(WIN32)
    # Azure's Windows version doesn't permit unprivileged creation of symbolic links
    if(CTEST_CMAKE_GENERATOR MATCHES "Visual Studio 15 2017.*")
      ctest_test(RETURN_VALUE retval EXCLUDE "${LLFIO_DISABLE_TESTS}|symlink|process_handle")
    else()
      ctest_test(RETURN_VALUE retval EXCLUDE "${LLFIO_DISABLE_TESTS}|symlink")
    endif()
  elseif(APPLE)
    # Around Feb 2021 the Mac OS CI began failing pipe_handle and I don't have a Mac to diagnose
    ctest_test(RETURN_VALUE retval EXCLUDE "${LLFIO_DISABLE_TESTS}|pipe_handle")
  else()
    ctest_test(RETURN_VALUE retval EXCLUDE "${LLFIO_DISABLE_TESTS}")
  endif()
endif()
if(WIN32)
  if(EXISTS "prebuilt/bin/Release/llfio_dl-2.0-Windows-AMD64-Release.dll")
    file(DOWNLOAD "https://github.com/ned14/outcome/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/outcome.tgz")
    file(DOWNLOAD "https://github.com/ned14/quickcpplib/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/quickcpplib.tgz")
    checked_execute_process("Tarring up binaries 0"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "outcome.tgz"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "quickcpplib.tgz"
    )
    checked_execute_process("Tarring up binaries 1"
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/bin/Release
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/lib/Release
      COMMAND "${CMAKE_COMMAND}" -E copy_directory doc llfio/doc/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory example llfio/example/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory include llfio/include/
    )
    checked_execute_process("Tarring up binaries 2"
      COMMAND "${CMAKE_COMMAND}" -E copy Build.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy index.html llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Licence.txt llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Readme.md llfio/
    )
    checked_execute_process("Tarring up binaries 3"
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/llfio_sl-2.0-Windows-AMD64-Release.lib llfio/prebuilt/lib/Release/
    )
    checked_execute_process("Tarring up binaries 4"
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/llfio_dl-2.0-Windows-AMD64-Release.lib llfio/prebuilt/lib/Release/
    )
    checked_execute_process("Tarring up binaries 5"
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/bin/Release/llfio_dl-2.0-Windows-AMD64-Release.dll llfio/prebuilt/bin/Release/
    )
    if(EXISTS "prebuilt/bin/Release/ntkernel-error-category_dl-1.0-Windows-AMD64-Release.dll")
      checked_execute_process("Tarring up binaries 6"
        COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/ntkernel-error-category_sl-1.0-Windows-AMD64-Release.lib llfio/prebuilt/lib/Release/
      )
      checked_execute_process("Tarring up binaries 7"
        COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/Release/ntkernel-error-category_dl-1.0-Windows-AMD64-Release.lib llfio/prebuilt/lib/Release/
      )
      checked_execute_process("Tarring up binaries 8"
        COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/bin/Release/ntkernel-error-category_dl-1.0-Windows-AMD64-Release.dll llfio/prebuilt/bin/Release/
      )
    endif()
    checked_execute_process("Tarring up binaries 9"
      COMMAND "${BASH_COMMAND}" -c "mv ned14-outcome* llfio/include/outcome"
      COMMAND "${BASH_COMMAND}" -c "mv ned14-quickcpplib* llfio/include/quickcpplib"
    )
    checked_execute_process("Tarring up binaries final"
      COMMAND "${CMAKE_COMMAND}" -E tar cfv llfio-v2.0-binaries-win64.zip --format=zip llfio/
    )
    get_filename_component(toupload llfio-v2.0-binaries-win64.zip ABSOLUTE)
  endif()
else()
  if(EXISTS "prebuilt/lib/libllfio_dl-2.0-Linux-x86_64-Release.so")
    file(DOWNLOAD "https://github.com/ned14/outcome/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/outcome.tgz")
    file(DOWNLOAD "https://github.com/ned14/quickcpplib/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/quickcpplib.tgz")
    checked_execute_process("Tarring up binaries 0"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "outcome.tgz"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "quickcpplib.tgz"
    )
    checked_execute_process("Tarring up binaries 1"
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory doc llfio/doc/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory example llfio/example/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory include llfio/include/
      COMMAND "${CMAKE_COMMAND}" -E copy Build.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy index.html llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Licence.txt llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Readme.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/libllfio_sl-2.0-Linux-x86_64-Release.a llfio/prebuilt/lib/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/libllfio_dl-2.0-Linux-x86_64-Release.so llfio/prebuilt/lib/
    )
    checked_execute_process("Tarring up binaries 2"
      COMMAND bash -c "mv ned14-outcome* llfio/include/outcome"
      COMMAND bash -c "mv ned14-quickcpplib* llfio/include/quickcpplib"
    )
    checked_execute_process("Tarring up binaries 3"
      COMMAND "${CMAKE_COMMAND}" -E tar cfz llfio-v2.0-binaries-linux-x64.tgz llfio
    )
    get_filename_component(toupload llfio-v2.0-binaries-linux-x64.tgz ABSOLUTE)
  endif()
  if(EXISTS "prebuilt/lib/libllfio_dl-2.0-Linux-armhf-Release.so")
    file(DOWNLOAD "https://github.com/ned14/outcome/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/outcome.tgz")
    file(DOWNLOAD "https://github.com/ned14/quickcpplib/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/quickcpplib.tgz")
    checked_execute_process("Tarring up binaries 0"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "outcome.tgz"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "quickcpplib.tgz"
    )
    checked_execute_process("Tarring up binaries 1"
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory doc llfio/doc/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory example llfio/example/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory include llfio/include/
      COMMAND "${CMAKE_COMMAND}" -E copy Build.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy index.html llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Licence.txt llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Readme.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/libllfio_sl-2.0-Linux-armhf-Release.a llfio/prebuilt/lib/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/libllfio_dl-2.0-Linux-armhf-Release.so llfio/prebuilt/lib/
    )
    checked_execute_process("Tarring up binaries 2"
      COMMAND bash -c "mv ned14-outcome* llfio/include/outcome"
      COMMAND bash -c "mv ned14-quickcpplib* llfio/include/quickcpplib"
    )
    checked_execute_process("Tarring up binaries 3"
      COMMAND "${CMAKE_COMMAND}" -E tar cfz llfio-v2.0-binaries-linux-armhf.tgz llfio
    )
    get_filename_component(toupload llfio-v2.0-binaries-linux-armhf.tgz ABSOLUTE)
  endif()
  if(EXISTS "prebuilt/lib/libllfio_dl-2.0-Darwin-arm64-Release.dylib")
    file(DOWNLOAD "https://github.com/ned14/outcome/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/outcome.tgz")
    file(DOWNLOAD "https://github.com/ned14/quickcpplib/tarball/master" "${CMAKE_CURRENT_LIST_DIR}/quickcpplib.tgz")
    checked_execute_process("Tarring up binaries 0"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "outcome.tgz"
      COMMAND "${CMAKE_COMMAND}" -E tar xfz "quickcpplib.tgz"
    )
    checked_execute_process("Tarring up binaries 1"
      COMMAND "${CMAKE_COMMAND}" -E make_directory llfio/prebuilt/lib
      COMMAND "${CMAKE_COMMAND}" -E copy_directory doc llfio/doc/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory example llfio/example/
      COMMAND "${CMAKE_COMMAND}" -E copy_directory include llfio/include/
      COMMAND "${CMAKE_COMMAND}" -E copy Build.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy index.html llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Licence.txt llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy Readme.md llfio/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/libllfio_sl-2.0-Darwin-arm64-Release.a llfio/prebuilt/lib/
      COMMAND "${CMAKE_COMMAND}" -E copy prebuilt/lib/libllfio_dl-2.0-Darwin-arm64-Release.dylib llfio/prebuilt/lib/
    )
    checked_execute_process("Tarring up binaries 2"
      COMMAND bash -c "mv ned14-outcome* llfio/include/outcome"
      COMMAND bash -c "mv ned14-quickcpplib* llfio/include/quickcpplib"
    )
    checked_execute_process("Tarring up binaries 3"
      COMMAND "${CMAKE_COMMAND}" -E tar cfz llfio-v2.0-binaries-darwin-arm64.tgz llfio
    )
    get_filename_component(toupload llfio-v2.0-binaries-darwin-arm64.tgz ABSOLUTE)
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
else()
  message(STATUS "SUCCESS: Running tests exited with ${retval} ${retval2} ${retval3}")
endif()
