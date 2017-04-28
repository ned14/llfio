cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
# If necessary bring in the Boost lite cmake tooling
list(FIND CMAKE_MODULE_PATH "boost-lite" boost_lite_idx)
if(${boost_lite_idx} EQUAL -1)
  if(EXISTS "${CMAKE_SOURCE_DIR}/../.use_boostish_siblings")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../boost-lite/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../.use_boostish_siblings")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../boost-lite/cmake")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
  else()
    message(FATAL_ERROR "FATAL: A copy of boost-lite cannot be found. Try running 'git submodule update --init --recursive'")
  endif()
endif()
include(BoostLiteRequireOutOfSourceBuild)
include(BoostLiteUtils)
include(BoostLitePolicies)

# Parse the version we tell cmake directly from the version header file
ParseProjectVersionFromHpp("${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/version.hpp" VERSIONSTRING)
# Sets the usual PROJECT_NAME etc
project(afio VERSION ${VERSIONSTRING} LANGUAGES C CXX)
# Also set a *cmake* namespace for this project
set(PROJECT_NAMESPACE boost::)
# Setup this cmake environment for this project
include(BoostLiteSetupProject)
if(NOT PROJECT_IS_DEPENDENCY)
  # This file should be updated with the last git SHA next commit
  UpdateRevisionHppFromGit("${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/revision.hpp")
endif()
# Find my library dependencies
find_boostish_library(boost-lite "include/boost/afio" 1.0 REQUIRED)
find_boostish_library(outcome    "include/boost/afio" 1.0 REQUIRED)
find_boostish_library(kerneltest "test"               1.0 REQUIRED)

# Make the standard static and shared libraries, and if supported by this compiler, C++ modules
# for both static and shared libraries as well. For the non-C++ module variants, makes the
# interface headers into precompiled headers. Only builds all of them if this is the topmost
# CMakeLists, else built only if something upstream is dependent on one of them.
include(BoostLiteMakeLibrary)
# Make an interface only library so dependent CMakeLists can bring in this header-only library
include(BoostLiteMakeHeaderOnlyLibrary)

# Create a custom doxygen generation target
include(BoostLiteMakeDoxygen)
# Set the standard definitions for these libraries and bring in the all_* helper functions
include(BoostLiteApplyDefaultDefinitions)
# Set the C++ features this library requires
all_compile_features(PUBLIC
  # cxx_exceptions                        ## Annoyingly not supported by cmake 3.6
  cxx_alias_templates
  cxx_variadic_templates
  cxx_noexcept
  cxx_constexpr
  cxx_lambda_init_captures
  cxx_attributes
  cxx_generic_lambdas
)
if(NOT MSVC OR CMAKE_VERSION VERSION_GREATER 3.59)
  all_compile_features(PUBLIC
    cxx_variable_templates
  )
endif()
# Set the library dependencies this library has
all_link_libraries(PUBLIC boost-lite::hl boost::outcome::hl boost::kerneltest::hl)
# Set the system dependencies this library has
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  all_link_libraries(PUBLIC stdc++fs rt)
endif()
# Set any macros this library requires
if(WIN32)
  all_compile_definitions(PRIVATE _WIN32_WINNT=0x600)  ## Target WinVista
endif()
# Anyone using the static or dynamic libraries is not using the header only variant
foreach(lib afio_sl afio_dl)
  target_compile_definitions(${lib} INTERFACE BOOST_AFIO_HEADERS_ONLY=0) 
  target_compile_definitions(${lib} PRIVATE BOOST_AFIO_SOURCE=1) 
endforeach()

# For all possible configurations of this library, add each test
include(BoostLiteMakeStandardTests)
# For each test target, set compile options
if(MSVC)
  foreach(test_target ${afio_TEST_TARGETS})
    target_compile_options(${test_target} PRIVATE /wd4503)  ## decorated name length exceeded
  endforeach()
endif()

# Make available this library for install and export
include(BoostLiteMakeInstall)
include(BoostLiteMakeExport)
