cmake_minimum_required(VERSION 3.0)
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
target_compile_options(modjpeg PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wno-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)
set_target_properties(modjpeg PROPERTIES VERSION ${libmodjpeg_VERSION_STRING} SOVERSION ${libmodjpeg_VERSION_MAJOR})

install(TARGETS modjpeg DESTINATION lib)
install(FILES libmodjpeg.h DESTINATION include)
