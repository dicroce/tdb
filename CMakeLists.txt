cmake_minimum_required(VERSION 3.22)
project(tdb)

set (CMAKE_CXX_STANDARD 14)

add_library (tdb include/tdb/b_tree.h
                 include/tdb/b_tree_node.h
                 include/tdb/file_utils.h
                 include/tdb/pager.h
                 source/b_tree.cpp
                 source/b_tree_node.cpp
                 source/file_utils.cpp
                 source/pager.cpp)

target_include_directories (tdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_subdirectory (ut)
