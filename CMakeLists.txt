cmake_minimum_required(VERSION 2.8)
project(S3MPlayer)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --std=c89 -Wall -Werror -Wextra -Wpedantic -g")
add_subdirectory(src)
