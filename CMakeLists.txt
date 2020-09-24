cmake_minimum_required(VERSION 2.8)
project(tdb)

include(FetchContent)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
endif()

set (CMAKE_CXX_STANDARD 14)

# Dependencies

FetchContent_Declare(
    cppkit
    GIT_REPOSITORY https://github.com/dicroce/cppkit
    GIT_TAG release-1.0.5
)

FetchContent_GetProperties(cppkit)
if(NOT cppkit_POPULATED)
  FetchContent_Populate(cppkit)
  add_subdirectory(${cppkit_SOURCE_DIR} ${cppkit_BINARY_DIR})
endif()

# tdb

add_library (tdb include/tdb/ptree.h
                 source/ptree.cpp)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tdb PUBLIC "-O0")
    target_compile_options(tdb PUBLIC "-ggdb")
else(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tdb PUBLIC "-O3")
endif(CMAKE_BUILD_TYPE STREQUAL "Debug")

target_include_directories (tdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(tdb cppkit)

add_subdirectory(ut)
