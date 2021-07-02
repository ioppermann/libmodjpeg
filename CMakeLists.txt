cmake_minimum_required(VERSION 2.9)
project(libmodjpeg)

cmake_policy(SET CMP0042 NEW)

set(libmodjpeg_VERSION_MAJOR 1)
set(libmodjpeg_VERSION_MINOR 0)
set(libmodjpeg_VERSION_PATCH 2)
set(libmodjpeg_VERSION_STRING ${libmodjpeg_VERSION_MAJOR}.${libmodjpeg_VERSION_MINOR}.${libmodjpeg_VERSION_PATCH})

set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(JPEG REQUIRED)

include_directories(${JPEG_INCLUDE_DIR})
link_libraries(${JPEG_LIBRARIES})

include(FindPkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBPNG libpng16)

    if(LIBPNG_FOUND)
        add_definitions(-DWITH_LIBPNG)
        include_directories(${LIBPNG_INCLUDE_DIRS})
        link_directories(${LIBPNG_LIBRARY_DIRS})
            link_libraries(${LIBPNG_LIBRARIES})
    endif()
endif()

add_library(modjpeg SHARED src/compose.c src/convolve.c src/dropon.c src/effect.c src/image.c src/jpeg.c)
target_compile_options(modjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wno-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
set_target_properties(modjpeg PROPERTIES VERSION ${libmodjpeg_VERSION_STRING} SOVERSION ${libmodjpeg_VERSION_MAJOR})

add_executable(modjpeg-dynamic src/contrib/modjpeg.c)
target_link_libraries(modjpeg-dynamic modjpeg)

install(TARGETS modjpeg DESTINATION lib)
install(PROGRAMS modjpeg-dynamic DESTINATION bin RENAME modjpeg)
install(FILES man/man1/modjpeg.1 DESTINATION share/man/man1)
install(FILES man/man3/libmodjpeg.3 DESTINATION share/man/man3)
install(FILES src/libmodjpeg.h DESTINATION include)
install(FILES LICENSE README.md VERSION DESTINATION share/doc/libmodjpeg)
