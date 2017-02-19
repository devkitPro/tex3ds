#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([3dstex], [1.0.0], [pigman46@gmail.com])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([source/main.cpp])

AM_INIT_AUTOMAKE([3dstex], [1.0.0])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

AC_LANG_PUSH([C])
AX_CHECK_COMPILE_FLAG([-Wall],    [CFLAGS+=" -Wall"])
AX_CHECK_COMPILE_FLAG([-std=c99], [CFLAGS+=" -std=c99"])
AX_CHECK_COMPILE_FLAG([-pthread], [CFLAGS+=" -pthread"])
AX_CHECK_COMPILE_FLAG([-flto],    [CFLAGS+=" -flto"])
AC_LANG_POP()

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Wall],      [CXXFLAGS+=" -Wall"])
AX_CHECK_COMPILE_FLAG([-std=c++11], [CXXFLAGS+=" -std=c++11"])
AX_CHECK_COMPILE_FLAG([-pthread],   [CXXFLAGS+=" -pthread"])
AX_CHECK_COMPILE_FLAG([-flto],      [CXXFLAGS+=" -flto"])
AC_LANG_POP()

# Checks for libraries.
PKG_CHECK_MODULES(ImageMagick, [Magick++ >= 6.0.0])

# Checks for header files.
AC_CHECK_HEADERS([limits.h stddef.h stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memrchr memset pow strcasecmp])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
