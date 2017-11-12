#
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

include Makefile.sources

ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

AM_MAKEFLAGS = -s

SUBDIRS = .

MAINTAINERCLEANFILES = ChangeLog INSTALL

AM_CPP_INCLUDES = -Icore -Iutils -Icompositor -Idisplay -I../os/ -I../os/linux/ -I../public/ -I../wsi/
AM_CPPFLAGS = -std=c++11 -fPIC -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -DENABLE_DOUBLE_BUFFERING
AM_CPPFLAGS += $(AM_CPP_INCLUDES) $(CWARNFLAGS) $(DRM_CFLAGS) $(DEBUG_CFLAGS) -Wformat -Wformat-security

if !ENABLE_GBM
AM_CPPFLAGS += -DUSE_MINIGBM
endif

libhwcomposer_common_la_LIBADD = \
	$(DRM_LIBS) \
	$(GBM_LIBS) \
	$(EGL_LIBS) \
	-lm

libhwcomposer_common_la_LTLIBRARIES = libhwcomposer_common.la
libhwcomposer_common_la_SOURCES = $(common_SOURCES)
if ENABLE_VULKAN
libhwcomposer_common_la_SOURCES += $(vk_SOURCES)
AM_CPP_INCLUDES += -Icompositor/vk
AM_CPPFLAGS += -Icompositor/vk -DUSE_VK -DDISABLE_EXPLICIT_SYNC
libhwcomposer_common_la_LIBADD += -lvulkan
else
libhwcomposer_common_la_SOURCES += $(gl_SOURCES)
AM_CPP_INCLUDES += -Icompositor/gl
AM_CPPFLAGS += -DUSE_GL
libhwcomposer_common_la_LIBADD += $(GLES2_LIBS)
endif

libhwcomposer_common_la_SOURCES += $(va_SOURCES)
AM_CPP_INCLUDES += -Icompositor/va

libhwcomposer_common_ladir = $(libdir)
libhwcomposer_common_la_LDFLAGS = -version-number 0:0:1 -no-undefined -static

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
