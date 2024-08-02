cmake_minimum_required(VERSION 3.14)

project(Net, VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps)

add_executable(Net src/main.cc src/game.cc src/net.cc)
target_compile_options(Net PRIVATE -Wall -Wextra -pedantic)

target_link_libraries(Net Dependencies)
