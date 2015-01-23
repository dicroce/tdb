cmake_minimum_required(VERSION 2.8)
project(tdb)

set(SOURCES source/ptree.cpp)

set(COMMON_LIBS cppkit)

include(common.cmake NO_POLICY_SCOPE)
include(../devel_artifacts/build/base_lib.cmake NO_POLICY_SCOPE)
