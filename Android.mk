# Copyright (C) 2015 The Android Open Source Project
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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdrm \
	libEGL \
	libGLESv2 \
	libhardware \
	liblog \
	libsync \
	libui \
	libutils


LOCAL_C_INCLUDES := \
	system/core/include/utils \
	system/core/libsync \
	system/core/libsync/include \
	$(LOCAL_PATH)/public \
	$(LOCAL_PATH)/common/core \
	$(LOCAL_PATH)/common/compositor \
	$(LOCAL_PATH)/common/compositor/gl \
	$(LOCAL_PATH)/common/display \
	$(LOCAL_PATH)/common/utils \
	$(LOCAL_PATH)/common/watchers \
	$(LOCAL_PATH)/os/android

LOCAL_SRC_FILES := \
	common/compositor/compositor.cpp \
	common/compositor/factory.cpp \
	common/compositor/nativesurface.cpp \
	common/compositor/renderstate.cpp \
	common/core/hwclayer.cpp \
	common/core/gpudevice.cpp \
	common/core/nativesync.cpp \
	common/core/overlaybuffer.cpp \
	common/core/overlaybuffermanager.cpp \
	common/core/overlaylayer.cpp \
	common/display/display.cpp \
	common/display/displayplane.cpp \
	common/display/displayplanemanager.cpp \
	common/display/displayqueue.cpp \
	common/display/headless.cpp \
	common/display/vblankeventhandler.cpp \
        common/display/kmsfencehandler.cpp \
	common/display/virtualdisplay.cpp \
	common/utils/drmscopedtypes.cpp \
	common/utils/fdhandler.cpp \
	common/utils/hwcevent.cpp \
	common/utils/hwcthread.cpp \
	common/utils/hwcutils.cpp \
	common/utils/disjoint_layers.cpp \
	os/android/grallocbufferhandler.cpp \
	os/android/drmhwctwo.cpp

LOCAL_CPPFLAGS += \
	-DHWC2_USE_CPP11 \
	-DHWC2_INCLUDE_STRINGIFICATION \
	-DUSE_ANDROID_SYNC \
	-DUSE_ANDROID_SHIM \
	-fPIC -O2 \
	-D_FORTIFY_SOURCE=2 \
	-fstack-protector-strong \
	-fPIE -Wformat -Wformat-security

ifeq ($(strip $(BOARD_DISABLE_NATIVE_COLOR_MODES)),true)
LOCAL_CPPFLAGS += -DDISABLE_NATIVE_COLOR_MODES
endif

ifeq ($(strip $(BOARD_ENABLE_EXPLICIT_SYNC)),false)
LOCAL_CPPFLAGS += -DDISABLE_EXPLICIT_SYNC
LOCAL_CPPFLAGS += -DDISABLE_OVERLAY_USAGE
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
	external/minigbm/cros_gralloc/
else
LOCAL_C_INCLUDES += \
	external/drm_gralloc/
endif

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
#Preffered paths for all vendor hals /vendor/lib/hw
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

endif
