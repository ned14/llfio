# QuickCppLib cmake
# (C) 2016-2019 Niall Douglas <http://www.nedproductions.biz/>
# 
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License in the accompanying file
# Licence.txt or at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# 
# 
# Distributed under the Boost Software License, Version 1.0.
#     (See accompanying file Licence.txt or copy at
#           http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
# If necessary bring in the quickcpplib cmake tooling
set(quickcpplib_done OFF)
foreach(item ${CMAKE_MODULE_PATH})
  if(item MATCHES "quickcpplib/cmakelib")
    set(quickcpplib_done ON)
  endif()
endforeach()
if(NOT quickcpplib_done)
  # CMAKE_SOURCE_DIR is the very topmost parent cmake project
  # CMAKE_CURRENT_SOURCE_DIR is the current cmake subproject
  set(CTEST_QUICKCPPLIB_CLONE_DIR)
  
  # If there is a magic .quickcpplib_use_siblings directory above the topmost project, use sibling edition
  if(EXISTS "${CMAKE_SOURCE_DIR}/../.quickcpplib_use_siblings" AND NOT QUICKCPPLIB_DISABLE_SIBLINGS)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../quickcpplib/cmakelib")
    set(CTEST_QUICKCPPLIB_SCRIPTS "${CMAKE_SOURCE_DIR}/../quickcpplib/scripts")
    # Copy latest version of myself into end user
    file(COPY "${CTEST_QUICKCPPLIB_SCRIPTS}/../cmake/QuickCppLibBootstrap.cmake" DESTINATION "${CMAKE_SOURCE_DIR}/cmake/")
  elseif(CMAKE_BINARY_DIR)
    # Place into root binary directory, same place as where find_quickcpplib_library() puts dependencies.
    set(CTEST_QUICKCPPLIB_CLONE_DIR "${CMAKE_BINARY_DIR}/quickcpplib")
  else()
    # We must be being called from a ctest script. No way of knowing what the build directory
    # will be, so simply clone into the current directory
    set(CTEST_QUICKCPPLIB_CLONE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/quickcpplib")
  endif()
  if(CTEST_QUICKCPPLIB_CLONE_DIR)
    file(MAKE_DIRECTORY "${CTEST_QUICKCPPLIB_CLONE_DIR}")
    find_package(quickcpplib QUIET CONFIG NO_DEFAULT_PATH PATHS "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo")
    if(NOT quickcpplib_FOUND)
      message(STATUS "quickcpplib not found, cloning git repository and installing into ${CTEST_QUICKCPPLIB_CLONE_DIR} ...")
      include(FindGit)
      execute_process(COMMAND "${GIT_EXECUTABLE}" clone "https://github.com/ned14/quickcpplib.git" repo
        WORKING_DIRECTORY "${CTEST_QUICKCPPLIB_CLONE_DIR}"
        OUTPUT_VARIABLE cloneout
        ERROR_VARIABLE errout
      )
      if(NOT EXISTS "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/cmakelib")
        message(FATAL_ERROR "FATAL: Failed to git clone quickcpplib!\n\nstdout was: ${cloneout}\n\nstderr was: ${errout}")
      endif()
    endif()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/cmakelib")
    set(CTEST_QUICKCPPLIB_SCRIPTS "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/scripts")
  endif()
endif()
