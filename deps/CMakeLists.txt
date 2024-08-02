project("Dependencies")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Dependencies INTERFACE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/raylib)

target_link_libraries(Dependencies INTERFACE
					raylib
)
