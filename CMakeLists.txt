cmake_minimum_required(VERSION 3.0)
project(libmodjpeg)

find_package(JPEG REQUIRED)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(MYLIB_VERSION_MAJOR 1)
set(MYLIB_VERSION_MINOR 0)
set(MYLIB_VERSION_PATCH 0)
set(MYLIB_VERSION_STRING ${MYLIB_VERSION_MAJOR}.${MYLIB_VERSION_MINOR}.${MYLIB_VERSION_PATCH})

add_library(modjpeg SHARED compose.c convolve.c dropon.c effect.c image.c jpeg.c)
set_target_properties(modjpeg PROPERTIES VERSION ${MYLIB_VERSION_STRING} SOVERSION ${MYLIB_VERSION_MAJOR})
target_compile_options(modjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
target_link_libraries(modjpeg jpeg)

add_executable(mdjpeg modjpeg.c)
target_compile_options(mdjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
target_link_libraries(mdjpeg modjpeg)

install(TARGETS mdjpeg DESTINATION bin)
install(TARGETS modjpeg DESTINATION lib)
install(FILES libmodjpeg.h DESTINATION include)