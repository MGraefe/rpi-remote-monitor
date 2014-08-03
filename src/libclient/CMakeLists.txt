cmake_minimum_required (VERSION 2.6)

include_directories ("${PROJECT_SOURCE_DIR}")

add_library(librprm-client SHARED client.c ${PROJECT_SOURCE_DIR}/shared/msgbuf.c)
add_definitions(-DLIBCLIENT_EXPORT_SYMBOLS)
