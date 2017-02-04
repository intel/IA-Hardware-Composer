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

SUBDIRS = \
	. \
	tests/third_party/json-c \
	tests

MAINTAINERCLEANFILES = ChangeLog INSTALL

AM_CPP_INCLUDES = -I$(top_srcdir) -Ipublic -Icommon/core -Icommon/utils -Icommon/compositor -Icommon/display  -Ios/linux -Icommon/compositor/gl -Itests/common
AM_CPPFLAGS = -std=c++11 -DUSE_GL -DUDEV_SUPPORT -DUSE_MINIGBM
AM_CPPFLAGS += $(AM_CPP_INCLUDES) $(CWARNFLAGS) $(DRM_CFLAGS) $(DEBUG_CFLAGS)
AM_CFLAGS = $(CWARNFLAGS) $(DRM_CFLAGS) $(DEBUG_CFLAGS)

libhwcomposer_la_LIBADD = \
	$(DRM_LIBS) \
	$(GBM_LIBS) \
	$(EGL_LIBS) \
	$(GLES2_LIBS) \
	-ludev \
	-lm

libhwcomposer_la_LTLIBRARIES = libhwcomposer.la
libhwcomposer_la_SOURCES = $(common_SOURCES) $(gl_SOURCES)
libhwcomposer_ladir = $(libdir)
libhwcomposer_la_LDFLAGS = -version-number 0:0:1 -no-undefined -static

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
