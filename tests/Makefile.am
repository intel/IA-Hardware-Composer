#
#  Copyright (c) 2012 Rob Clark <rob@ti.com>
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice (including the next
#  paragraph) shall be included in all copies or substantial portions of the
#  Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.
#

bin_PROGRAMS = testlayers

testlayers_LDFLAGS = \
	-no-undefined

AM_CPP_INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/public -I../common/core -I../common/utils -I../common/compositor -I../common/display  -I../os/linux -I./common -I./third_party/json-c
AM_CPPFLAGS = -std=c++11 -DUSE_MINIGBM
AM_CPPFLAGS += $(AM_CPP_INCLUDES) $(CWARNFLAGS) $(DRM_CFLAGS) $(DEBUG_CFLAGS)

testlayers_LDADD = \
	$(DRM_LIBS) \
	$(GBM_LIBS) \
	$(EGL_LIBS) \
	$(GLES2_LIBS) \
	$(top_builddir)/tests/third_party/json-c/libjson-c.la \
	$(top_builddir)/libhwcomposer.la

testlayers_CFLAGS = \
	-O0 -g -lm \
	$(DRM_CFLAGS) \
	$(GBM_CFLAGS) \
	$(EGL_CFLAGS) \
	$(GLES2_CFLAGS) \
        $(AM_CPPFLAGS)

testlayers_SOURCES = \
    ./common/layerrenderer.cpp \
    ./common/gllayerrenderer.cpp \
    ./common/glcubelayerrenderer.cpp \
    ./common/videolayerrenderer.cpp \
    ./common/imagelayerrenderer.cpp \
    ./common/esTransform.c \
    ./common/layerfromjson.cpp \
    ./apps/jsonlayerstest.cpp
