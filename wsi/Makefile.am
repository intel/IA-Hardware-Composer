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

AM_CPP_INCLUDES = -Idrm -I../os/ -I../os/linux/ -I../public/ -I../common/display/ -I../common/core/ -I../common/utils/ -I../common/compositor/ -I../common/compositor/va
AM_CPPFLAGS = -std=c++11 -fPIC -O2 -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fPIE -DENABLE_DOUBLE_BUFFERING
AM_CPPFLAGS += $(AM_CPP_INCLUDES) $(CWARNFLAGS) $(DRM_CFLAGS) $(DEBUG_CFLAGS) -Wformat -Wformat-security

if DISABLE_HOTPLUG_SUPPORT
AM_CPPFLAGS += -DDISABLE_HOTPLUG_NOTIFICATION
endif

libhwcomposer_wsi_la_LIBADD = \
	$(DRM_LIBS) \
	$(GBM_LIBS) \
	$(EGL_LIBS) \
	-lm

noinst_LTLIBRARIES = libhwcomposer_wsi.la
libhwcomposer_wsi_la_SOURCES = $(wsi_SOURCES)
libhwcomposer_wsi_ladir = $(libdir)
libhwcomposer_wsi_la_LDFLAGS = -version-number 0:0:1 -no-undefined

if ENABLE_VULKAN
AM_CPP_INCLUDES += -I../common/compositor/vk
AM_CPPFLAGS += -I../common/compositor/vk -DUSE_VK -DDISABLE_EXPLICIT_SYNC
libhwcomposer_wsi_la_LIBADD += -lvulkan
else
AM_CPP_INCLUDES += -I../common/compositor/gl
AM_CPPFLAGS += -DUSE_GL
libhwcomposer_wsi_la_LIBADD += $(GLES2_LIBS)
endif

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL
