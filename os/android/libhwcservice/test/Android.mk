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

LOCAL_PATH := $(call my-dir)

ANDROID_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))

include $(CLEAR_VARS)
LOCAL_MODULE:= hwcservice_test
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../../
LOCAL_SRC_FILES:= hwcservice_test.cpp
LOCAL_SHARED_LIBRARIES := libutils libbinder liblog libhwcservice
LOCAL_PROPRIETARY_MODULE := true
ifeq ($(shell test $(ANDROID_VERSION) -ge 8; echo $$?), 0)
LOCAL_CFLAGS += -DUSE_PROCESS_STATE
endif
ifeq ($(strip $(ENABLE_HYPER_DMABUF_SHARING)), true)
LOCAL_CFLAGS += -DENABLE_PANORAMA
endif
include $(BUILD_EXECUTABLE)
