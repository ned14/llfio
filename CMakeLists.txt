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

# Make preprocessed edition of this library target
if(NOT PROJECT_IS_DEPENDENCY)
  if(NOT PYTHONINTERP_FOUND)
    indented_message(WARNING "NOT rebuilding preprocessed edition of library due to python not being installed")
  else()
    # See if the ply package is installed so pcpp can run
    execute_process(COMMAND python -c "import ply" RESULT_VARIABLE python_has_ply)
    if(NOT python_has_ply EQUAL 0)
      indented_message(WARNING "NOT rebuilding preprocessed edition of library due to installed python not having the ply package installed. "
        "Do 'pip install ply' to fix. NOTE that doxygen docs will NOT build without the precompiled edition.")
    else()
      function(make_single_header target name)
        add_partial_preprocess(${target}
                              "${name}"
                              ${ARGN}
                              -I ..
                              --passthru-defines --passthru-unfound-includes --passthru-unknown-exprs
                              --passthru-comments --line-directive # --debug
                              -U QUICKCPPLIB_ENABLE_VALGRIND
                              -U DOXYGEN_SHOULD_SKIP_THIS -U DOXYGEN_IS_IN_THE_HOUSE
                              -U STANDARDESE_IS_IN_THE_HOUSE
                              -U __has_include -U __has_feature -U __has_cpp_attribute
                              -U __cpp_modules
                              WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                              )
        if(NOT CMAKE_VERSION VERSION_LESS 3.3)
          add_dependencies(outcome_hl ${target})
        endif()
      endfunction()
      make_single_header(afio_hl-pp-std
                         "${CMAKE_CURRENT_SOURCE_DIR}/single-header/afio.hpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/include/afio/v2.0/afio.hpp"
                         #-D QUICKCPPLIB_USE_STD_BYTE -D QUICKCPPLIB_USE_STD_OPTIONAL -D QUICKCPPLIB_USE_STD_SPAN
                         -D AFIO_LEAN_AND_MEAN
                         -D AFIO_EXPERIMENTAL_STATUS_CODE=1)
      make_single_header(afio_hl-pp-abi
                         "${CMAKE_CURRENT_SOURCE_DIR}/single-header/abi.hpp"
                         "${CMAKE_CURRENT_SOURCE_DIR}/include/afio/v2.0/afio.hpp"
                         -D AFIO_EXPERIMENTAL_STATUS_CODE=1
                         -D AFIO_DISABLE_ABI_PERMUTATION=1
                         -D OUTCOME_DISABLE_ABI_PERMUTATION=1
                         -D QUICKCPPLIB_DISABLE_ABI_PERMUTATION=1
                         -U AFIO_UNSTABLE_VERSION
                         -U OUTCOME_UNSTABLE_VERSION)
    endif()
  endif()
endif()

# Create a custom doxygen generation target
include(QuickCppLibMakeDoxygen)
# Set the standard definitions for these libraries and bring in the all_* helper functions
include(QuickCppLibApplyDefaultDefinitions)
# Do we have the Coroutines TS?
function(CheckCXXHasCoroutines iter flags)
  set(CMAKE_REQUIRED_FLAGS "${flags}")
  check_cxx_source_compiles("
#include <future>
std::future<int> g() { co_return 0; }
int main() { return g().get(); }
" CXX_HAS_COROUTINES${iter})
  set(CXX_HAS_COROUTINES${iter} ${CXX_HAS_COROUTINES${iter}} PARENT_SCOPE)
endfunction()
include(CheckCXXSourceCompiles)
if(MSVC)
  CheckCXXHasCoroutines(_MSVC "/await")
  if(CXX_HAS_COROUTINES_MSVC)
    all_compile_options(PUBLIC "/await")
    all_compile_definitions(PUBLIC "__cpp_coroutines")
  endif()
endif()
if(CLANG OR GCC)
  CheckCXXHasCoroutines(_CLANG_GCC "-std=c++14 -fcoroutines-ts")
  if(CXX_HAS_COROUTINES_CLANG_GCC)
    all_compile_options(PUBLIC "-fcoroutines-ts")
    all_compile_definitions(PUBLIC "__cpp_coroutines")
  endif()
endif()

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
all_link_libraries(PUBLIC quickcpplib::hl outcome::hl Threads::Threads)
# Set the system dependencies this library has
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  find_library(libstdcxx_stdcxxfs stdc++fs)
  if(libstdcxx_stdcxxfs MATCHES "NOTFOUND")
    set(libstdcxx_stdcxxfs -lstdc++fs)
  endif()
  all_link_libraries(PUBLIC ${libstdcxx_stdcxxfs} rt)
endif()
if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD" OR APPLE)
  find_library(libcxx_cxxexperimental c++experimental)
  all_link_libraries(PUBLIC ${libcxx_cxxexperimental})
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
# For each test target, link to kerneltest
foreach(test_target ${afio_TEST_TARGETS})
  target_link_libraries(${test_target} PRIVATE kerneltest::hl)
endforeach()
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
