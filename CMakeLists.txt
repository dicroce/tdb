cmake_minimum_required(VERSION 3.22)
project(tdb)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set (CMAKE_BUILD_TYPE Debug)
endif()

add_library (tdb include/tdb/b_tree.h
                 source/b_tree.cpp
                 source/row_store.cpp
                 include/tdb/row_store.h
                 include/tdb/file_utils.h
                 source/file_utils.cpp
                 include/tdb/pager.h
                 source/pager.cpp)

target_include_directories (tdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory (ut)
