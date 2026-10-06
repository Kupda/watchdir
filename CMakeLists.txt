cmake_minimum_required(VERSION 3.16)
project(watchdir LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)

add_executable(watchdir src/main.cpp)