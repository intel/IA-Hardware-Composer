# Copyright (c) 2016 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


if ENABLE_DUMMY_COMPOSITOR
    AM_CPPFLAGS = -DUSE_DC
else
bin_PROGRAMS = testlayers \
	       linux_test

testlayers_LDFLAGS = \
	-no-undefined

AM_CPP_INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/public -I../common/core -I../common/utils -I../common/compositor -I../common/display -I../os -I../os/linux -I./common -I./third_party/json-c -I../wsi/drm -I../wsi
AM_CPPFLAGS = -std=c++11 -fPIC -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE
AM_CPPFLAGS += $(AM_CPP_INCLUDES) $(CWARNFLAGS) $(DRM_CFLAGS) $(DEBUG_CFLAGS) -Wformat -Wformat-security

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
    ./common/esTransform.cpp \
    ./common/jsonhandlers.cpp \
    ./apps/jsonlayerstest.cpp

linux_test_LDFLAGS = \
	-no-undefined

linux_test_LDADD = \
	$(DRM_LIBS) \
	$(GBM_LIBS) \
	$(EGL_LIBS) \
	$(GLES2_LIBS) \
	-ldl	\
	$(top_builddir)/tests/third_party/json-c/libjson-c.la \
	$(top_builddir)/libhwcomposer.la

linux_test_CFLAGS = \
	-O0 -g -lm \
	$(DRM_CFLAGS) \
	$(GBM_CFLAGS) \
	$(EGL_CFLAGS) \
	$(GLES2_CFLAGS) \
        $(AM_CPPFLAGS)

linux_test_SOURCES = \
    ./common/layerrenderer.cpp \
    ./common/gllayerrenderer.cpp \
    ./common/glcubelayerrenderer.cpp \
    ./common/esTransform.cpp \
    ./common/jsonhandlers.cpp \
    ./apps/linux_frontend_test.cpp
endif
