cmake_minimum_required(VERSION 3.22)
project(linx)


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -Wall -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c++17")


add_subdirectory(linxsdk)
add_subdirectory(demo)


