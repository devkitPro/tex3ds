# Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = tex3ds

tex3ds_SOURCES = source/encode.cpp \
                 source/huff.c \
                 source/lzss.c \
                 source/magick_compat.cpp \
                 source/main.cpp \
                 source/rg_etc1.cpp \
                 source/rle.c

tex3ds_LDADD = $(ImageMagick_LIBS)
AM_CPPFLAGS   = -I$(srcdir)/include -D_GNU_SOURCE $(ImageMagick_CFLAGS)
EXTRA_DIST = autogen.sh
