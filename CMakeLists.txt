cmake_minimum_required(VERSION 3.14)
project(loghelper VERSION 2.0.0 LANGUAGES CXX)

# --------------------------------------------------------------------------
# Options
# --------------------------------------------------------------------------
set(LOGHELPER_BACKEND "spdlog" CACHE STRING "Log backend: spdlog|zlog|fallback")
set_property(CACHE LOGHELPER_BACKEND PROPERTY STRINGS spdlog zlog fallback)

option(LOGHELPER_BUILD_TESTS    "Build unit tests"    ON)
option(LOGHELPER_BUILD_BENCH    "Build benchmarks"    ON)
option(LOGHELPER_BUILD_EXAMPLES "Build examples"      ON)

# --------------------------------------------------------------------------
# Backend compile definition
# --------------------------------------------------------------------------
if(LOGHELPER_BACKEND STREQUAL "spdlog")
  set(LOGHELPER_BACKEND_ID 1)
elseif(LOGHELPER_BACKEND STREQUAL "zlog")
  set(LOGHELPER_BACKEND_ID 2)
else()
  set(LOGHELPER_BACKEND_ID 3)
endif()

# --------------------------------------------------------------------------
# Header-only interface library
# --------------------------------------------------------------------------
add_library(loghelper INTERFACE)
target_include_directories(loghelper INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(loghelper INTERFACE cxx_std_14)
target_compile_definitions(loghelper INTERFACE
  LOGHELPER_BACKEND=${LOGHELPER_BACKEND_ID}
)

# --------------------------------------------------------------------------
# FetchContent setup
# --------------------------------------------------------------------------
include(FetchContent)

# --------------------------------------------------------------------------
# spdlog via FetchContent
# --------------------------------------------------------------------------
if(LOGHELPER_BACKEND STREQUAL "spdlog")
  FetchContent_Declare(
    spdlog
    URL https://ghfast.top/https://github.com/gabime/spdlog/archive/refs/tags/v1.15.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
  )
  set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(spdlog)
  target_link_libraries(loghelper INTERFACE spdlog::spdlog)
endif()

# --------------------------------------------------------------------------
# zlog (system-installed)
# --------------------------------------------------------------------------
if(LOGHELPER_BACKEND STREQUAL "zlog")
  find_library(ZLOG_LIB zlog REQUIRED)
  find_path(ZLOG_INCLUDE zlog.h)
  target_link_libraries(loghelper INTERFACE ${ZLOG_LIB})
  target_include_directories(loghelper INTERFACE ${ZLOG_INCLUDE})
endif()

# --------------------------------------------------------------------------
# Threads
# --------------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(loghelper INTERFACE Threads::Threads)

# --------------------------------------------------------------------------
# Tests (Catch2 v3)
# --------------------------------------------------------------------------
if(LOGHELPER_BUILD_TESTS)
  enable_testing()

  FetchContent_Declare(
    Catch2
    URL https://ghfast.top/https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(loghelper_tests tests/test_loghelper.cpp)
  target_link_libraries(loghelper_tests PRIVATE loghelper Catch2::Catch2WithMain)
  target_compile_features(loghelper_tests PRIVATE cxx_std_14)

  include(CTest)
  include(Catch)
  catch_discover_tests(loghelper_tests)
endif()

# --------------------------------------------------------------------------
# Benchmarks
# --------------------------------------------------------------------------
if(LOGHELPER_BUILD_BENCH)
  add_executable(loghelper_bench tests/bench_loghelper.cpp)
  target_link_libraries(loghelper_bench PRIVATE loghelper)
  target_compile_features(loghelper_bench PRIVATE cxx_std_14)
  target_compile_options(loghelper_bench PRIVATE -O3)
endif()

# --------------------------------------------------------------------------
# Examples
# --------------------------------------------------------------------------
if(LOGHELPER_BUILD_EXAMPLES)
  add_executable(loghelper_demo examples/demo.cpp)
  target_link_libraries(loghelper_demo PRIVATE loghelper)
  target_compile_features(loghelper_demo PRIVATE cxx_std_14)

  configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/conf/logger.cfg
    ${CMAKE_CURRENT_BINARY_DIR}/logger.cfg
    COPYONLY
  )
endif()

message(STATUS "[loghelper] Backend: ${LOGHELPER_BACKEND} (ID=${LOGHELPER_BACKEND_ID})")

