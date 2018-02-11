cmake_minimum_required(VERSION 3.0)
project(libmodjpeg)

set(libmodjpeg_VERSION_MAJOR 1)
set(libmodjpeg_VERSION_MINOR 0)
set(libmodjpeg_VERSION_PATCH 0)
set(libmodjpeg_VERSION_STRING ${libmodjpeg_VERSION_MAJOR}.${libmodjpeg_VERSION_MINOR}.${libmodjpeg_VERSION_PATCH})

find_package(JPEG REQUIRED)

#set(CMAKE_VERBOSE_MAKEFILE ON)

add_library(modjpeg SHARED compose.c convolve.c dropon.c effect.c image.c jpeg.c)
set_target_properties(modjpeg PROPERTIES VERSION ${libmodjpeg_VERSION_STRING} SOVERSION ${libmodjpeg_VERSION_MAJOR})
target_compile_options(modjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
target_link_libraries(modjpeg jpeg)

install(TARGETS modjpeg DESTINATION lib)
install(FILES libmodjpeg.h DESTINATION include)