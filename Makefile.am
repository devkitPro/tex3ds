# Makefile.am -- Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = tex3ds mkbcfnt

tex3ds_SOURCES = source/atlas.cpp \
                 source/encode.cpp \
                 source/huff.cpp \
                 source/lzss.cpp \
                 source/magick_compat.cpp \
                 source/rg_etc1.cpp \
                 source/rle.cpp \
                 source/swizzle.cpp \
                 source/tex3ds.cpp \
                 include/atlas.h \
                 include/compress.h \
                 include/encode.h \
                 include/magick_compat.h \
                 include/quantum.h \
                 include/rg_etc1.h \
                 include/subimage.h \
                 include/swizzle.h

mkbcfnt_SOURCES = source/bcfnt.cpp \
                  source/ft_error.cpp \
                  source/mkbcfnt.cpp \
                  source/swizzle.cpp \
                  include/bcfnt.h \
                  include/ft_error.h \
                  include/swizzle.h

LDADD = $(ImageMagick_LIBS)
AM_CXXFLAGS = -I$(srcdir)/include -D_GNU_SOURCE $(ImageMagick_CFLAGS)

mkbcfnt_LDADD = $(FreeType_LIBS) $(LDADD)
mkbcfnt_CPPFLAGS = $(FreeType_CFLAGS)

EXTRA_DIST = autogen.sh
