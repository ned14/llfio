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

cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
# If necessary bring in the quickcpplib cmake tooling
set(quickcpplib_done OFF)
foreach(item ${CMAKE_MODULE_PATH})
  if(item MATCHES "quickcpplib/cmakelib")
    set(quickcpplib_done ON)
  endif()
endforeach()
if(NOT quickcpplib_done)
  find_package(quickcpplib QUIET CONFIG)
  if(quickcpplib_FOUND)
    if(DEFINED quickcpplib_CMAKELIB_DIR AND DEFINED quickcpplib_SCRIPTS_DIR)
      set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${quickcpplib_CMAKELIB_DIR}")
      set(CTEST_QUICKCPPLIB_SCRIPTS "${quickcpplib_SCRIPTS_DIR}")
      set(quickcpplib_done ON)
    elseif(EXISTS "${quickcpplib_DIR}/share/cmakelib")
      set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${quickcpplib_DIR}/share/cmakelib")
      set(CTEST_QUICKCPPLIB_SCRIPTS "${quickcpplib_DIR}/share/scripts")
      set(quickcpplib_done ON)
    elseif(EXISTS "${quickcpplib_DIR}/../../../share/cmakelib")
      set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${quickcpplib_DIR}/../../../share/cmakelib")
      set(CTEST_QUICKCPPLIB_SCRIPTS "${quickcpplib_DIR}/../../../share/scripts")
      set(quickcpplib_done ON)
    endif()
  endif()
endif()
#message("*** ${CMAKE_MODULE_PATH}")
if(NOT quickcpplib_done)
  # CMAKE_SOURCE_DIR is the very topmost parent cmake project
  # CMAKE_CURRENT_SOURCE_DIR is the current cmake subproject
  if(NOT DEFINED CTEST_QUICKCPPLIB_CLONE_DIR)
    if(EXISTS "${CMAKE_SOURCE_DIR}/../.quickcpplib_use_siblings" AND NOT QUICKCPPLIB_DISABLE_SIBLINGS)
      # If there is a magic .quickcpplib_use_siblings directory above the topmost project, use sibling edition
      set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../quickcpplib/cmakelib")
      set(CTEST_QUICKCPPLIB_SCRIPTS "${CMAKE_SOURCE_DIR}/../quickcpplib/scripts")
      # Copy latest version of myself into end user
      file(COPY "${CTEST_QUICKCPPLIB_SCRIPTS}/../cmake/QuickCppLibBootstrap.cmake" DESTINATION "${CMAKE_SOURCE_DIR}/cmake/")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.quickcpplib_use_siblings" AND NOT QUICKCPPLIB_DISABLE_SIBLINGS)
      set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../quickcpplib/cmakelib")
      set(CTEST_QUICKCPPLIB_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/../quickcpplib/scripts")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/quickcpplib/repo/cmakelib")
      set(CTEST_QUICKCPPLIB_CLONE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/quickcpplib")
    elseif(CMAKE_BINARY_DIR)
      # Place into root binary directory, same place as where find_quickcpplib_library() puts dependencies.
      set(CTEST_QUICKCPPLIB_CLONE_DIR "${CMAKE_BINARY_DIR}/quickcpplib")
    else()
      # We must be being called from a ctest script. No way of knowing what the build directory
      # will be, so simply clone into the current directory
      set(CTEST_QUICKCPPLIB_CLONE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/quickcpplib")
    endif()
  endif()
  if(CTEST_QUICKCPPLIB_CLONE_DIR)
    if(NOT EXISTS "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/cmakelib")
      file(MAKE_DIRECTORY "${CTEST_QUICKCPPLIB_CLONE_DIR}")
      message(STATUS "quickcpplib not found, cloning git repository and installing into ${CTEST_QUICKCPPLIB_CLONE_DIR} ...")
      include(FindGit)
      execute_process(COMMAND "${GIT_EXECUTABLE}" clone --recurse-submodules --depth 1 --jobs 8 --shallow-submodules "https://github.com/ned14/quickcpplib.git" repo
        WORKING_DIRECTORY "${CTEST_QUICKCPPLIB_CLONE_DIR}"
        OUTPUT_VARIABLE cloneout
        ERROR_VARIABLE errout
      )
      if(NOT EXISTS "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/cmakelib")
        message(FATAL_ERROR "FATAL: Failed to git clone quickcpplib!\n\nstdout was: ${cloneout}\n\nstderr was: ${errout}\n\nIf you are in a build environment which prevents use of the internet during superbuild, please place a clone of quickcpplib into '${CTEST_QUICKCPPLIB_CLONE_DIR}/repo' i.e. as if 'cd \"${CTEST_QUICKCPPLIB_CLONE_DIR}\" && git clone --recursive \"https://github.com/ned14/quickcpplib.git\" repo'. You can also predefine CTEST_QUICKCPPLIB_CLONE_DIR to point at a copy of quickcpplib during cmake configuration.")
      endif()
    endif()
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/cmakelib")
    set(CTEST_QUICKCPPLIB_SCRIPTS "${CTEST_QUICKCPPLIB_CLONE_DIR}/repo/scripts")
  endif()
endif()
