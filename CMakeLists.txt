cmake_minimum_required(VERSION 3.23)

project(BEJ-to-JSON LANGUAGES C)

add_executable(BEJ-to-JSON main.c decode.c)

target_include_directories(BEJ-to-JSON PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)