cmake_minimum_required (VERSION 2.6)

include_directories ("${PROJECT_SOURCE_DIR}")

add_executable (rprm-server main.c readstat.c ${PROJECT_SOURCE_DIR}/shared/msgbuf.c)