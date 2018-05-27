#!/bin/sh

FILES="cmake_install.cmake CMakeCache.txt CMakeFiles install_manifest.txt Makefile"
DIRS="./ contrib/"

for dir in ${DIRS}; do
	for file in ${FILES}; do
		if [ ! -e ${dir}${file} ]; then
			continue
		fi

		rm -rf ${dir}${file}
	done
done

if [ -e ./contrib/modjpeg ]; then
	rm ./contrib/modjpeg
fi
