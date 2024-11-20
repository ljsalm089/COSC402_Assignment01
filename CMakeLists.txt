cmake_minimum_required(VERSION 3.11)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_VERBOSE_MAKEFILE TRUE)

project(IM C CXX ASM)
message(STATUS "The value of CMAKE_VERBOSE_MAKEFILE is : ${CMAKE_VERBOSE_MAKEFILE}")

add_subdirectory(src)
