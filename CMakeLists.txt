cmake_minimum_required(VERSION 2.8)
project(S3MPlayer)

add_executable(s3mplay src/s3m.c src/s3mload.c src/s3mplay.c)
target_link_libraries(s3mplay portaudio)
