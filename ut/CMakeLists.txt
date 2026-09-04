add_executable(
    tdb_ut
    include/framework.h
    source/framework.cpp
    include/test_b_tree.h
    source/test_b_tree.cpp
)

target_include_directories(
    tdb_ut PUBLIC 
    include
    ../include
)
target_link_libraries(
    tdb_ut LINK_PUBLIC 
    tdb
)
find_package(Threads REQUIRED)
target_link_libraries(tdb_ut PUBLIC Threads::Threads)
