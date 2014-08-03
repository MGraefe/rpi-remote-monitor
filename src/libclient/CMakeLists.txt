cmake_minimum_required (VERSION 2.6)
project (librpirmclient)
add_library(librpirmclient SHARED client.c)
add_definitions(-DLIBCLIENT_EXPORT_SYMBOLS)
