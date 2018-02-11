cmake_minimum_required(VERSION 3.0)
project(modjpeg)

find_package(JPEG REQUIRED)

add_executable(modjpeg modjpeg.c ../compose.c ../convolve.c ../dropon.c ../effect.c ../image.c ../jpeg.c)
target_compile_options(modjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
target_link_libraries(modjpeg jpeg)

install(TARGETS modjpeg DESTINATION bin)