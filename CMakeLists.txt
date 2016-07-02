cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/include/boost/afio/boost-lite/cmake")
include(RequireOutOfSourceBuild)

project(afio VERSION 2.0.0 LANGUAGES CXX)
set(PROJECT_NAMESPACE boost::)
include(MakeBoostLiteLibrary)
include(MakeBoostLiteHeaderOnlyLibrary)
