cmake_minimum_required(VERSION 2.8)
project(libmodjpeg)

set(libmodjpeg_VERSION_MAJOR 1)
set(libmodjpeg_VERSION_MINOR 0)
set(libmodjpeg_VERSION_PATCH 0)
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

add_library(modjpeg SHARED compose.c convolve.c dropon.c effect.c image.c jpeg.c)
#target_compile_options(modjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
set_target_properties(modjpeg PROPERTIES VERSION ${libmodjpeg_VERSION_STRING} SOVERSION ${libmodjpeg_VERSION_MAJOR})

add_executable(modjpeg-dynamic contrib/modjpeg.c)
target_link_libraries(modjpeg-dynamic modjpeg)

install(TARGETS modjpeg DESTINATION lib)
install(PROGRAMS modjpeg-dynamic DESTINATION bin RENAME modjpeg)
install(PROGRAMS ../scripts/bash/betterjpeg.sh DESTINATION bin RENAME betterjpeg)
install(FILES ../man/man1/betterjpeg.1 ../man/man1/modjpeg.1 DESTINATION share/man/man1)
install(FILES libmodjpeg.h DESTINATION include)
install(FILES ../LICENSE ../README.md ../VERSION DESTINATION share/doc/libmodjpeg)
