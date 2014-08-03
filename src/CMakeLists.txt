cmake_minimum_required (VERSION 2.6)
project (rpi-remote-monitor)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory ("${PROJECT_SOURCE_DIR}/server")
add_subdirectory ("${PROJECT_SOURCE_DIR}/libclient")
add_subdirectory ("${PROJECT_SOURCE_DIR}/consoleclient")
