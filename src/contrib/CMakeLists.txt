cmake_minimum_required(VERSION 2.8)
project(modjpeg)

cmake_policy(SET CMP0042 NEW)

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

add_executable(modjpeg-static modjpeg.c ../compose.c ../convolve.c ../dropon.c ../effect.c ../image.c ../jpeg.c)
target_compile_options(modjpeg-static PRIVATE -O2 -Wall -Wextra -Wpointer-arith -Wconditional-uninitialized -Wno-unused-parameter -Wno-deprecated-declarations -Werror)

install(PROGRAMS modjpeg-static DESTINATION bin RENAME modjpeg)
