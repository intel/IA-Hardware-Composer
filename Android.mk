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


ifeq ($(strip $(BOARD_USES_IA_HWCOMPOSER)),true)
# Obtain root HWC source path
HWC_PATH := $(call my-dir)

HWC_VERSION_GIT_BRANCH := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse --abbrev-ref HEAD; popd > /dev/null)
HWC_VERSION_GIT_SHA := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse HEAD; popd > /dev/null)

LOCAL_PATH := $(call my-dir)
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
	common/compositor/compositor.cpp \
	common/compositor/factory.cpp \
	common/compositor/nativesurface.cpp \
	common/compositor/renderstate.cpp \
	common/core/gpudevice.cpp \
	common/core/hwclayer.cpp \
        common/core/hwclock.cpp \
	common/core/overlaylayer.cpp \
	common/display/displayplanemanager.cpp \
	common/display/displayqueue.cpp \
	common/display/headless.cpp \
	common/display/vblankeventhandler.cpp \
	common/display/virtualdisplay.cpp \
	common/utils/fdhandler.cpp \
	common/utils/hwcevent.cpp \
	common/utils/hwcthread.cpp \
	common/utils/hwcutils.cpp \
	common/utils/disjoint_layers.cpp \
	wsi/physicaldisplay.cpp \
	wsi/drm/drmdisplay.cpp \
	wsi/drm/drmbuffer.cpp \
	wsi/drm/drmplane.cpp \
	wsi/drm/drmdisplaymanager.cpp \
	wsi/drm/drmscopedtypes.cpp \
        os/android/iahwc2.cpp \
        os/android/hwcservice.cpp \
        os/android/multidisplaymanager.cpp

ifeq ($(strip $(BOARD_USES_GRALLOC1)), true)
LOCAL_SRC_FILES += os/android/gralloc1bufferhandler.cpp
else
LOCAL_SRC_FILES += os/android/grallocbufferhandler.cpp
endif

LOCAL_CFLAGS += -DHWC_VERSION_GIT_BRANCH="\"$(HWC_VERSION_GIT_BRANCH)\""
LOCAL_CFLAGS += -DHWC_VERSION_GIT_SHA="\"$(HWC_VERSION_GIT_SHA)\""
LOCAL_CFLAGS += -Wno-date-time
LOCAL_CPPFLAGS += \
	-DHWC2_USE_CPP11 \
	-DHWC2_INCLUDE_STRINGIFICATION \
	-DUSE_ANDROID_SYNC \
	-DUSE_ANDROID_SHIM \
	-O2 \
	-D_FORTIFY_SOURCE=2 \
	-fstack-protector-strong \
	-Wformat -Wformat-security

ifeq ($(strip $(BOARD_DISABLE_NATIVE_COLOR_MODES)),true)
LOCAL_CPPFLAGS += -DDISABLE_NATIVE_COLOR_MODES
endif

ifeq ($(strip $(BOARD_USES_VULKAN)),)
LOCAL_CPPFLAGS += \
	-DUSE_GL

LOCAL_SRC_FILES += \
	common/compositor/gl/glprogram.cpp \
	common/compositor/gl/glrenderer.cpp \
	common/compositor/gl/glsurface.cpp \
	common/compositor/gl/egloffscreencontext.cpp \
	common/compositor/gl/nativeglresource.cpp \
	common/compositor/gl/shim.cpp \
	common/compositor/scopedrendererstate.cpp
endif

ifeq ($(strip $(BOARD_USES_MINIGBM)),true)
LOCAL_CPPFLAGS += -DUSE_MINIGBM
LOCAL_C_INCLUDES += \
	$(INTEL_MINIGBM)/cros_gralloc/
else
LOCAL_C_INCLUDES += \
	$(INTEL_DRM_GRALLOC)
endif

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
#Preffered paths for all vendor hals /vendor/lib/hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

# libhwcservice
HWC_BUILD_DIRS := \
$(LOCAL_PATH)/os/android/libhwcservice/Android.mk \
$(LOCAL_PATH)/os/android/libhwcservice/utils/Android.mk

include $(HWC_BUILD_DIRS)

#Include tests only if eng build
ifneq (,$(filter eng,$(TARGET_BUILD_VARIANT)))
# Commenting for now include when ld issue is resolved
include $(HWC_PATH)/tests/hwc-val/tests/hwc/Android.mk
#include $(LOCAL_PATH)/tests/third_party/json-c/Android.mk
endif

endif
