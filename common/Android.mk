# Copyright (c) 2017 Intel Corporation
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

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libdrm \
        libEGL \
        libGLESv2 \
        libhardware \
        liblog \
        libui \
        libutils \
        libhwcservice \
        libbinder

LOCAL_C_INCLUDES := \
        system/core/include/utils \
        $(LOCAL_PATH)/../public \
        $(LOCAL_PATH)/core \
        $(LOCAL_PATH)/compositor \
        $(LOCAL_PATH)/compositor/gl \
        $(LOCAL_PATH)/display \
        $(LOCAL_PATH)/utils \
        $(LOCAL_PATH)/../os \
        $(LOCAL_PATH)/../os/android \
        $(LOCAL_PATH)/../wsi \
        $(LOCAL_PATH)/../wsi/drm \
        $(TARGET_OUT_HEADERS)/libva

LOCAL_SHARED_LIBRARIES += \
	libva \
	libva-android

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 27; echo $$?), 0)
LOCAL_SHARED_LIBRARIES += \
	libnativewindow
LOCAL_STATIC_LIBRARIES += \
	libarect
LOCAL_HEADER_LIBRARIES += \
	libnativebase_headers
LOCAL_CFLAGS += \
	-DUSE_VNDK
endif

ifeq ($(strip $(BOARD_CURSOR_WA)), true)
LOCAL_CPPFLAGS += \
	-DDISABLE_CURSOR_PLANE
endif

#LOCAL_CPPFLAGS += \
#	-DENABLE_DOWNSCALING

LOCAL_SRC_FILES := \
        compositor/compositor.cpp \
        compositor/compositorthread.cpp \
        compositor/factory.cpp \
        compositor/nativesurface.cpp \
        compositor/renderstate.cpp \
	compositor/va/varenderer.cpp \
	compositor/va/vautils.cpp \
        core/gpudevice.cpp \
        core/hwclayer.cpp \
	core/resourcemanager.cpp \
	core/framebuffermanager.cpp \
	core/logicaldisplay.cpp \
	core/logicaldisplaymanager.cpp \
	core/mosaicdisplay.cpp \
        core/overlaylayer.cpp \
	core/nesteddisplay.cpp \
        display/displayplanemanager.cpp \
	display/displayplanestate.cpp \
        display/displayqueue.cpp \
        display/vblankeventhandler.cpp \
        display/virtualdisplay.cpp \
        utils/fdhandler.cpp \
        utils/hwcevent.cpp \
        utils/hwcthread.cpp \
        utils/hwcutils.cpp \
        utils/disjoint_layers.cpp

ifeq ($(strip $(ENABLE_NESTED_DISPLAY_SUPPORT)), true)
LOCAL_CPPFLAGS += -DNESTED_DISPLAY_SUPPORT
endif

ifeq ($(strip $(TARGET_USES_HWC2)), false)
LOCAL_C_INCLUDES += \
        system/core/libsync \
        system/core/libsync/include

LOCAL_SHARED_LIBRARIES += \
	libsync

LOCAL_CPPFLAGS += -DENABLE_DOUBLE_BUFFERING
endif

LOCAL_CPPFLAGS += \
        -DHWC2_INCLUDE_STRINGIFICATION \
        -DHWC2_USE_CPP11 \
        -Wno-date-time \
        -DUSE_ANDROID_SHIM \
        -D_FORTIFY_SOURCE=2 \
        -fstack-protector-strong \
        -Wformat -Wformat-security \
        -std=c++14 -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
        -Wall -Wsign-compare -Wpointer-arith \
        -Wcast-qual -Wcast-align \
        -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
        -O3

ifeq ($(strip $(BOARD_USES_VULKAN)), true)
LOCAL_SHARED_LIBRARIES += \
        libvulkan

LOCAL_CPPFLAGS += \
        -DUSE_VK \
        -DDISABLE_EXPLICIT_SYNC

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/compositor/vk \
        $(LOCAL_PATH)/../../mesa/include

LOCAL_SRC_FILES += \
        compositor/vk/vkprogram.cpp \
        compositor/vk/vkrenderer.cpp \
        compositor/vk/vksurface.cpp \
        compositor/vk/nativevkresource.cpp \
        compositor/vk/vkshim.cpp
else
LOCAL_CPPFLAGS += \
        -DUSE_GL

LOCAL_SRC_FILES += \
        compositor/gl/glprogram.cpp \
        compositor/gl/glrenderer.cpp \
        compositor/gl/glsurface.cpp \
        compositor/gl/egloffscreencontext.cpp \
        compositor/gl/nativeglresource.cpp \
        compositor/gl/shim.cpp
endif

LOCAL_C_INCLUDES += \
        $(INTEL_MINIGBM)/cros_gralloc/

LOCAL_MODULE := libhwcomposer_common
LOCAL_CFLAGS += -fvisibility=default
LOCAL_LDFLAGS += -no-undefined
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_STATIC_LIBRARY)
