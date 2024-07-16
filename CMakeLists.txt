cmake_minimum_required(VERSION 3.14)

project(OTR, VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(OTR src/main.cpp)
target_compile_options(OTR PRIVATE -Wall -Wextra -pedantic)
