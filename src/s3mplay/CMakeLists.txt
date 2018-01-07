add_executable(s3mplay main.c)
target_link_libraries(s3mplay s3mlib portaudio)
include_directories(../s3mlib)
