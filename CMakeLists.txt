cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
# Bring in the Boost lite cmake tooling
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
include(BoostLiteRequireOutOfSourceBuild)
include(BoostLiteUtils)  # for ParseProjectVersionFromHpp and UpdateRevisionHppFromGit

# Parse the version we tell cmake directly from the version header file
ParseProjectVersionFromHpp("${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/version.hpp" VERSIONSTRING)
# Sets the usual PROJECT_NAME etc
project(afio VERSION ${VERSIONSTRING} LANGUAGES CXX)
# Also set a *cmake* namespace for this project
set(PROJECT_NAMESPACE boost::)

# Update this header file with the current git SHA and last timestamp
UpdateRevisionHppFromGit("${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/revision.hpp")
# Make the standard static and shared libraries, and if supported by this compiler, C++ modules
# for both static and shared libraries as well. For the non-C++ module variants, makes the
# interface headers into precompiled headers. Only builds all of them if this is the topmost
# CMakeLists, else built only if something upstream is dependent on one of them.
include(BoostLiteMakeLibrary)
# Make an interface only library so dependent CMakeLists can bring in this header-only library
include(BoostLiteMakeHeaderOnlyLibrary)

# Set the standard definitions for these libraries and bring in the all_* helper functions
include(BoostLiteApplyDefaultDefinitions)
# Set the C++ features this library requires
all_compile_features(PUBLIC
  # cxx_exceptions                        ## Annoyingly not supported by cmake 3.4
  cxx_alias_templates
  cxx_variadic_templates
  cxx_noexcept
  cxx_constexpr
  cxx_thread_local
  #cxx_init_captures                      ## Not supported yet by cmake 3.4
  cxx_attributes
  cxx_variable_templates
)
# Set any macros this library requires
if(WIN32)
  all_compile_definitions(PRIVATE _WIN32_WINNT=0x600)  ## Target WinVista
endif()
# Anyone using the static or dynamic libraries is not using the header only variant
foreach(lib afio_sl afio_dl)
  target_compile_definitions(${lib} INTERFACE BOOST_AFIO_HEADERS_ONLY=0) 
  target_compile_definitions(${lib} PRIVATE BOOST_AFIO_SOURCE=1) 
endforeach()
# Test C++ Modules (ICEs in VS2015 Update 3)
if(MSVC AND 0)
  target_compile_definitions(afio_dl PUBLIC -D__cpp_modules=1)
  target_compile_options(afio_dl PUBLIC /experimental:module)
  target_sources(afio_dl PRIVATE include/boost/afio/afio.ixx)
  set_source_files_properties(include/boost/afio/afio.ixx PROPERTIES
      LANGUAGE CXX
  )
endif()

# For all possible configurations of this library, add each test
include(BoostLiteMakeStandardTests)
