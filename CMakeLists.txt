cmake_minimum_required(VERSION 3.10)
project(srclang
        VERSION "0.1"
        DESCRIPTION "Source Programming Language"
        LANGUAGES C)

find_library(READLINE readline REQUIRED)
add_executable(${PROJECT_NAME}
        src/main.c)
target_link_libraries(${PROJECT_NAME} PRIVATE
        ${READLINE})