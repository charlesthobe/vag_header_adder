cmake_minimum_required(VERSION 3.25)
project(vag_header_adder)

set(CMAKE_CXX_STANDARD 23)

add_compile_options(-Wall -Wextra)

add_executable(vag_header_adder main.cpp)
