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

# Obtain root HWC source path
HWC_PATH := $(call my-dir)

# Add validation directory to the include path
LOCAL_C_INCLUDES += $(HWC_PATH)/val

# define IA_HWC_ANDROID_VERSION (e.g., 4.3.2 would be 432)
ifeq ($(PLATFORM_VERSION),O)
IA_HWC_ANDROID_VERSION := 800
else
major := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
minor := $(word 2, $(subst ., , $(PLATFORM_VERSION).0))
rev   := $(word 3, $(subst ., , $(PLATFORM_VERSION).0.0))
# cope with 5.1.51
rev   := $(subst 51,5,$(rev))
IA_HWC_ANDROID_VERSION := $(major)$(minor)$(rev)
endif

HWC_VERSION_GIT_BRANCH := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse --abbrev-ref HEAD; popd > /dev/null)
HWC_VERSION_GIT_SHA := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse HEAD; popd > /dev/null)

# Build in advanced debugging features if not a pure user build or not simulating one
ifeq ($(strip $(IA_HWC_SIMULATE_USER_BUILD)),true)
    IA_HWC_INTERNAL_BUILD = false
    IA_HWC_LOGVIEWER_BUILD = false
    IA_HWC_DEV_ASSERTS_BUILD = false
else
    ifeq ($(strip $(TARGET_BUILD_VARIANT)),userdebug)
        IA_HWC_INTERNAL_BUILD = false
        IA_HWC_LOGVIEWER_BUILD = true
        IA_HWC_DEV_ASSERTS_BUILD = false
    endif
endif

LOCAL_MODULE_TAGS := optional
LOCAL_CLANG := true
LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/Android.mk
LOCAL_CFLAGS += -DANDROID_VERSION=$(IA_HWC_ANDROID_VERSION)
LOCAL_CFLAGS += -DTARGET_PRODUCT_$(shell echo $(TARGET_PRODUCT) | tr '[:lower:]' '[:upper:]')
LOCAL_CFLAGS += -DTARGET_BOARD_PLATFORM_$(shell echo $(TARGET_BOARD_PLATFORM) | tr '[:lower:]' '[:upper:]')
LOCAL_CFLAGS += -Werror -Wall -Werror=unused-parameter -fvisibility-inlines-hidden -fvisibility=hidden -std=gnu++11
LOCAL_CFLAGS += -Wno-date-time
LOCAL_CFLAGS += -DLOG_TAG=\"hwc\"
LOCAL_CFLAGS += -DHWC_VERSION_GIT_BRANCH="\"$(HWC_VERSION_GIT_BRANCH)\""
LOCAL_CFLAGS += -DHWC_VERSION_GIT_SHA="\"$(HWC_VERSION_GIT_SHA)\""
LOCAL_CFLAGS += -std=c++14
LOCAL_MULTILIB := first
LOCAL_CPPFLAGS := -std=c++14
LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libcutils \
    libdrm \
    libhardware \
    libhwcservice \
    liblog \
    libsync \
    libui \
    libutils

# Compile in debug support if this is an engineering build
ifeq ($(strip $(IA_HWC_LOGVIEWER_BUILD)),true)
    LOCAL_CFLAGS += -DIA_HWC_LOGVIEWER_BUILD=1
endif

# Compile in developer asserts
ifeq ($(strip $(IA_HWC_DEV_ASSERTS_BUILD)),true)
    LOCAL_CFLAGS += -DIA_HWC_DEV_ASSERTS_BUILD=1
endif

# Enable building all binaries to the appropriate vendor subdirectory if required
LOCAL_PROPRIETARY_MODULE := true

# Uncomment for GDB debugging
#LOCAL_CFLAGS += -O0 -ggdb
