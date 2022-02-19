cmake_minimum_required(VERSION 3.16.3)
option(WITH_ASAN "Use address sanitizer" OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Show compiler commands" FORCE)  # For clang-tidy and friends

# hack to make this happy on windows
set(CMAKE_THREAD_LIBS_INIT "-lpthread")
set(CMAKE_HAVE_THREADS_LIBRARY 1)
set(CMAKE_USE_WIN32_THREADS_INIT 0)
set(CMAKE_USE_PTHREADS_INIT 1)
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Extra modules - this is apparently one of the standard locations to keep them
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")
include(Compiler)
project(cryptobase)

include(Flags)  # Must come after project (project causes some magic to happen)
include(Utils)  # comes after external libraries so that we can overwrite stuffs

# external libraries
add_definitions(-DBOOST_ALL_DYN_LINK)
find_package(Boost COMPONENTS
  log
  program_options
  REQUIRED
  )

# vendors stuffs
include(FetchContent)
FetchContent_Declare(
  simdjson
  GIT_REPOSITORY https://github.com/simdjson/simdjson.git
  GIT_TAG  tags/v1.0.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(simdjson)

FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG  tags/v2.7.0
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(pybind11)


add_subdirectory(cc)
set (CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/_run" CACHE PATH "default install path" FORCE)

message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
