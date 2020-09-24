cmake_minimum_required (VERSION 2.8.12)

add_executable (tdb_ut include/framework.h
                       include/test_tdb.h
                       source/framework.cpp
                       source/test_tdb.cpp)
                      
target_include_directories (tdb_ut PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(tdb_ut tdb cppkit bzip2 pthread)
