cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
# Bring in the Boost lite cmake tooling
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
include(BoostLiteRequireOutOfSourceBuild)
include(BoostLiteHppUtils)  # for ParseProjectVersionFromHpp and UpdateRevisionHppFromGit

# Parse the version we tell cmake directly from the version header file
ParseProjectVersionFromHpp("${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/version.hpp" VERSIONSTRING)
# Sets the usual PROJECT_NAME etc
project(afio VERSION ${VERSIONSTRING} LANGUAGES CXX)
# Also set a *cmake* namespace for this project
set(PROJECT_NAMESPACE boost::)

# Update this header file with the current git SHA and last timestamp
UpdateRevisionHppFromGit("${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/revision.hpp")
# Make an interface only library so dependent CMakeLists can bring in this header-only library
include(BoostLiteMakeHeaderOnlyLibrary)
# Anyone using the static or dynamic libraries is not using the header only variant
set(afio_NON_HEADER_ONLY_COMPILE_DEFINITIONS INTERFACE BOOST_AFIO_HEADERS_ONLY=0) 
# Make the standard static and shared libraries, and if supported by this compiler, C++ modules
# for both static and shared libraries as well. For the non-C++ module variants, makes the
# interface headers into precompiled headers. Only builds all of them if this is the topmost
# CMakeLists, else built only if something upstream is dependent on one of them.
include(BoostLiteMakeLibrary)

# Set the C++ features this library requires
foreach(lib afio_sl afio_dl)
  if(${lib} STREQUAL afio_hl)
    set(PUBLIC INTERFACE)
  else()
    set(PUBLIC PUBLIC)
  endif()
  target_compile_features(${lib} ${PUBLIC}
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
  unset(PUBLIC)
endforeach()

