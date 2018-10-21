#!/bin/sh

FILES="cmake_install.cmake CMakeCache.txt CMakeFiles install_manifest.txt Makefile modjpeg-dynamic src/contrib/modjpeg-static"
DIRS="./"

for dir in ${DIRS}; do
    for file in ${FILES}; do
        if [ ! -e ${dir}${file} ]; then
            continue
        fi

        rm -rf ${dir}${file}
    done
done

rm *.dylib
rm *.so