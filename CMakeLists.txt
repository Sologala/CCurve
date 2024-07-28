cmake_minimum_required(VERSION 3.12.0)
project(CCurve)

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 14)
add_library(CCurve ./src/CCurve.cpp)
target_include_directories(CCurve PUBLIC ./include/)

add_subdirectory(test)

