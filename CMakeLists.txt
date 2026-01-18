cmake_minimum_required(VERSION 4.1)
project(WyrmEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(WyrmEngine main.cpp)

add_subdirectory(WyrmRenderer)
add_subdirectory(include)
add_subdirectory(src)

target_link_libraries(WyrmEngine PRIVATE WyrmRenderer)
