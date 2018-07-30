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


#------------------------------------------------------------------------
#  1. Evaluate ANDROID_VERSION from PLATFORM_VERSION
#  2. First letter may be the major version or the dessert letter
#------------------------------------------------------------------------
ANDROID_M := 6
ANDROID_N := 7
ANDROID_O := 8
ANDROID_VERSION_6 := $(ANDROID_M)
ANDROID_VERSION_7 := $(ANDROID_N)
ANDROID_VERSION_8 := $(ANDROID_O)
ANDROID_VERSION_O := $(ANDROID_O)
ANDROID_VERSION_OMR1 := $(ANDROID_O)
ANDROID_SUPPORTED_VERSIONS := 6 7 O 8 OMR1

ANDROID_MAJOR_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
$(info "PLATFORM_VERSION $(PLATFORM_VERSION)")
$(info "ANDROID_MAJOR_VERSION $(ANDROID_MAJOR_VERSION)")
$(foreach item, $(ANDROID_SUPPORTED_VERSIONS),\
$(if $(call streq,$(ANDROID_MAJOR_VERSION),$(item)),\
$(eval ANDROID_VERSION := $(ANDROID_VERSION_$(item))),))


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
        frameworks/native/libs/nativewindow/include \
        frameworks/native/libs/nativewindow \
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
	os/platformcommondrmdefines.cpp \
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

LOCAL_CPPFLAGS += -DUSE_GRALLOC1

ifeq ($(strip $(ENABLE_NESTED_DISPLAY_SUPPORT)), true)
LOCAL_CPPFLAGS += -DNESTED_DISPLAY_SUPPORT
endif

LOCAL_SRC_FILES += os/android/gralloc1bufferhandler.cpp

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
	-O3 \
	-Wno-unused-private-field \
	-Wno-unused-function \
	-Wno-unused-parameter \
	-Wno-unused-variable

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
	-DUSE_GL \
	-DENABLE_RBC
endif

ifneq ($(strip $(HWC_DISABLE_VA_DRIVER)), true)
LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/common/compositor/va

LOCAL_SHARED_LIBRARIES += \
	libva \
	libva-android
else
LOCAL_CPPFLAGS += -DDISABLE_VA
endif

LOCAL_C_INCLUDES += \
	$(INTEL_MINIGBM)/cros_gralloc/

ifeq ($(shell test $(ANDROID_VERSION) -ge 8; echo $$?), 0)
LOCAL_SHARED_LIBRARIES += libnativewindow
endif

ifeq ($(strip $(BOARD_CURSOR_WA)), true)
LOCAL_CPPFLAGS += \
	-DDISABLE_CURSOR_PLANE
endif

ifeq ($(strip $(BOARD_THREEDIS_UNDERRUN_WA)), true)
LOCAL_CPPFLAGS += \
        -DTHREEDIS_UNDERRUN_WA
endif

endif
