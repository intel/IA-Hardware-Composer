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

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(HWC_PATH)/Android.common.mk

LOCAL_WHOLE_STATIC_LIBRARIES := \
        libhwcomposer_common \
        libhwcomposer_wsi

LOCAL_MODULE := libhwcomposer.$(TARGET_BOARD_PLATFORM)
include $(BUILD_STATIC_LIBRARY)

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

endif
