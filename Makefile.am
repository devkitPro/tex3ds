# Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = 3dstex

3dstex_SOURCES = source/encode.cpp \
                 source/huff.c \
                 source/lzss.c \
                 source/magick_compat.cpp \
                 source/main.cpp \
                 source/rg_etc1.cpp \
                 source/rle.c

AM_CFLAGS   = -Iinclude -D_GNU_SOURCE $(ImageMagick_CFLAGS)
AM_CXXFLAGS = -Iinclude -D_GNU_SOURCE $(ImageMagick_CFLAGS)
AM_LDFLAGS  = $(ImageMagick_LIBS)

EXTRA_DIST = autogen.sh
