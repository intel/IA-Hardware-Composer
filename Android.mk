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


ifeq ($(strip $(BOARD_USES_IA_HWCOMPOSER)), true)
# Obtain root HWC source path
HWC_PATH := $(call my-dir)

HWC_VERSION_GIT_BRANCH := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse --abbrev-ref HEAD; popd > /dev/null)
HWC_VERSION_GIT_SHA := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse HEAD; popd > /dev/null)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libhwcomposer_common \
        libhwcomposer_wsi

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
	$(LOCAL_PATH)/public \
	$(LOCAL_PATH)/common/core \
	$(LOCAL_PATH)/common/compositor \
	$(LOCAL_PATH)/common/compositor/gl \
	$(LOCAL_PATH)/common/display \
	$(LOCAL_PATH)/common/utils \
	$(LOCAL_PATH)/os \
	$(LOCAL_PATH)/os/android \
	$(LOCAL_PATH)/wsi \
	$(LOCAL_PATH)/wsi/drm

LOCAL_SRC_FILES := \
	os/android/platformdefines.cpp

ifeq ($(strip $(TARGET_USES_HWC2)), true)
LOCAL_SRC_FILES += os/android/iahwc2.cpp \
                   os/android/hwcservice.cpp
else
LOCAL_SRC_FILES += os/android/iahwc1.cpp
LOCAL_C_INCLUDES += \
        system/core/libsync \
        system/core/libsync/include

LOCAL_SHARED_LIBRARIES += \
        libsync

LOCAL_CPPFLAGS += -DENABLE_DOUBLE_BUFFERING
endif

ifeq ($(strip $(BOARD_USES_GRALLOC1)), true)
LOCAL_SRC_FILES += os/android/gralloc1bufferhandler.cpp
else
LOCAL_SRC_FILES += os/android/grallocbufferhandler.cpp
endif

LOCAL_CPPFLAGS += \
	-DHWC_VERSION_GIT_BRANCH="\"$(HWC_VERSION_GIT_BRANCH)\"" \
	-DHWC_VERSION_GIT_SHA="\"$(HWC_VERSION_GIT_SHA)\"" \
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

ifeq ($(strip $(BOARD_DISABLE_NATIVE_COLOR_MODES)), true)
LOCAL_CPPFLAGS += -DDISABLE_NATIVE_COLOR_MODES
endif

ifeq ($(strip $(BOARD_USES_VULKAN)), true)
LOCAL_SHARED_LIBRARIES += \
        libvulkan

LOCAL_CPPFLAGS += \
        -DUSE_VK \
        -DDISABLE_EXPLICIT_SYNC

LOCAL_C_INCLUDES += \
        $(LOCAL_PATH)/common/compositor/vk \
        $(LOCAL_PATH)/../mesa/include
else
LOCAL_CPPFLAGS += \
        -DUSE_GL
endif

ifeq ($(strip $(BOARD_USES_LIBVA)), true)
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/common/compositor/va

LOCAL_SHARED_LIBRARIES += \
	libva \
	libva-android
endif

ifeq ($(strip $(BOARD_USES_MINIGBM)), true)
LOCAL_CPPFLAGS += -DUSE_MINIGBM
LOCAL_C_INCLUDES += \
	$(INTEL_MINIGBM)/cros_gralloc/
else
LOCAL_C_INCLUDES += \
	$(INTEL_DRM_GRALLOC)
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -gt 26; echo $$?), 0)
LOCAL_SHARED_LIBRARIES += libnativewindow
LOCAL_STATIC_LIBRARIES += libarect
LOCAL_HEADER_LIBRARIES += libnativebase_headers
endif

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
# Preffered paths for all vendor hals /vendor/lib/hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

# Static lib: libhwcomposer_common and libhwcomposer_wsi
include $(HWC_PATH)/common/Android.mk
include $(HWC_PATH)/wsi/Android.mk

ifeq ($(strip $(TARGET_USES_HWC2)), true)
# libhwcservice
HWC_BUILD_DIRS := \
$(HWC_PATH)/os/android/libhwcservice/Android.mk \
$(HWC_PATH)/os/android/libhwcservice/utils/Android.mk

include $(HWC_BUILD_DIRS)

# Include tests only if eng build
ifneq (,$(filter eng,$(TARGET_BUILD_VARIANT)))
#include $(HWC_PATH)/tests/hwc-val/tests/hwc/Android.mk
endif

endif

# Include tests only if eng build
ifneq (,$(filter eng,$(TARGET_BUILD_VARIANT)))
# Commenting for now include when ld issue is resolved
#include $(HWC_PATH)/tests/third_party/json-c/Android.mk
endif

endif
