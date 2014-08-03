cmake_minimum_required (VERSION 2.6)

include_directories ("${PROJECT_SOURCE_DIR}")

add_executable(rprm-console-client consoleclient.c)
target_link_libraries (rprm-console-client librprm-client)