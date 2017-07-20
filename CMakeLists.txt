cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
include(cmake/QuickCppLibBootstrap.cmake)
include(QuickCppLibRequireOutOfSourceBuild)
include(QuickCppLibUtils)
include(QuickCppLibPolicies)

# Parse the version we tell cmake directly from the version header file
ParseProjectVersionFromHpp("${CMAKE_CURRENT_SOURCE_DIR}/include/afio/version.hpp" VERSIONSTRING)
# Sets the usual PROJECT_NAME etc
project(afio VERSION ${VERSIONSTRING} LANGUAGES C CXX)
# Also set a *cmake* namespace for this project
set(PROJECT_NAMESPACE)
# Setup this cmake environment for this project
include(QuickCppLibSetupProject)
if(NOT PROJECT_IS_DEPENDENCY)
  # This file should be updated with the last git SHA next commit
  UpdateRevisionHppFromGit("${CMAKE_CURRENT_SOURCE_DIR}/include/afio/revision.hpp")
endif()
# Find my library dependencies
find_quickcpplib_library(quickcpplib 1.0 REQUIRED)
find_quickcpplib_library(outcome 2.0 REQUIRED)
find_quickcpplib_library(kerneltest 1.0 REQUIRED)
if(WIN32)
  add_subdirectory("include/afio/ntkernel-error-category" EXCLUDE_FROM_ALL)
endif()

# Make the standard static and shared libraries, and if supported by this compiler, C++ modules
# for both static and shared libraries as well. For the non-C++ module variants, makes the
# interface headers into precompiled headers. Only builds all of them if this is the topmost
# CMakeLists, else built only if something upstream is dependent on one of them.
include(QuickCppLibMakeLibrary)
# Make an interface only library so dependent CMakeLists can bring in this header-only library
include(QuickCppLibMakeHeaderOnlyLibrary)

# Create a custom doxygen generation target
include(QuickCppLibMakeDoxygen)
# Set the standard definitions for these libraries and bring in the all_* helper functions
include(QuickCppLibApplyDefaultDefinitions)
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
all_link_libraries(PUBLIC quickcpplib::hl outcome::hl kerneltest::hl)
# Set the system dependencies this library has
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  all_link_libraries(PUBLIC stdc++fs rt)
endif()
# Set any macros this library requires
if(WIN32)
  all_compile_definitions(PRIVATE _WIN32_WINNT=0x600)  ## Target WinVista
  target_link_libraries(afio_dl PUBLIC ntkernel-error-category::dl)
  target_link_libraries(afio_sl PUBLIC ntkernel-error-category::sl)
endif()
# Anyone using the static or dynamic libraries is not using the header only variant
foreach(lib afio_sl afio_dl)
  target_compile_definitions(${lib} INTERFACE AFIO_HEADERS_ONLY=0) 
  target_compile_definitions(${lib} PRIVATE AFIO_SOURCE=1) 
endforeach()

# For all possible configurations of this library, add each test
include(QuickCppLibMakeStandardTests)
# For each test target, set compile options
if(MSVC)
  foreach(test_target ${afio_TEST_TARGETS})
    target_compile_options(${test_target} PRIVATE /wd4503)  ## decorated name length exceeded
  endforeach()
endif()

# Cache this library's auto scanned sources for later reuse
include(QuickCppLibCacheLibrarySources)

# Make available this library for install and export
include(QuickCppLibMakeInstall)
include(QuickCppLibMakeExport)
