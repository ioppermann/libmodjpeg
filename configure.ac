# https://autotools.io/index.html
AC_PREREQ([2.69])
AC_INIT([libmodjpeg], [1.0.0], [https://github.com/ioppermann/libmodjpeg/issues])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_PROG_AR
LT_INIT

AC_CONFIG_SRCDIR([libmodjpeg.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([jpeg], [jpeg_read_header])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stdlib.h string.h unistd.h jpeglib.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset strchr strtol])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
