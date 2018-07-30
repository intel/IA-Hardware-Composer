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
        $(LOCAL_PATH)/../common/core \
        $(LOCAL_PATH)/../common/compositor \
        $(LOCAL_PATH)/../common/compositor/gl \
	$(LOCAL_PATH)/../common/compositor/va \
        $(LOCAL_PATH)/../common/display \
        $(LOCAL_PATH)/../common/utils \
        $(LOCAL_PATH)/../os \
        $(LOCAL_PATH)/../os/android \
        $(LOCAL_PATH)/../wsi \
	$(LOCAL_PATH)/../wsi/drm

ifeq ($(strip $(HWC_DISABLE_VA_DRIVER)), true)
LOCAL_CPPFLAGS += -DDISABLE_VA
else
LOCAL_SHARED_LIBRARIES += \
	libva
LOCAL_C_INCLUDES += \
	$(TARGET_OUT_HEADERS)/libva
endif

LOCAL_SRC_FILES := \
        physicaldisplay.cpp \
        drm/drmdisplay.cpp \
        drm/drmbuffer.cpp \
        drm/drmplane.cpp \
        drm/drmdisplaymanager.cpp \
	drm/drmscopedtypes.cpp

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
        $(LOCAL_PATH)/../common/compositor/vk \
        $(LOCAL_PATH)/../../mesa/include
else
LOCAL_CPPFLAGS += \
        -DUSE_GL
endif

LOCAL_C_INCLUDES += \
	$(INTEL_MINIGBM)/cros_gralloc/

ifeq ($(strip $(DISABLE_HOTPLUG_SUPPORT)), true)
LOCAL_CPPFLAGS += \
	-DDISABLE_HOTPLUG_NOTIFICATION
endif

LOCAL_CPPFLAGS += -DENABLE_ANDROID_WA

ifeq ($(strip $(BOARD_THREEDIS_UNDERRUN_WA)), true)
LOCAL_CPPFLAGS += \
        -DTHREEDIS_UNDERRUN_WA
endif

LOCAL_MODULE := libhwcomposer_wsi
LOCAL_CFLAGS += -fvisibility=default
LOCAL_LDFLAGS += -no-undefined
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_STATIC_LIBRARY)
