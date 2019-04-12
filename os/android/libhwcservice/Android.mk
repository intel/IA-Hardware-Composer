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

# Obtain Android Version
ANDROID_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))

LOCAL_SRC_FILES += icontrols.cpp                \
                   idiagnostic.cpp              \
                   iservice.cpp                 \
                   hwcserviceapi.cpp

LOCAL_MODULE := libhwcservice
LOCAL_CFLAGS += -fvisibility=default
ifeq ($(strip $(ENABLE_HYPER_DMABUF_SHARING)), true)
LOCAL_CFLAGS += -DENABLE_PANORAMA
endif
LOCAL_SHARED_LIBRARIES := libutils libbinder liblog
LOCAL_MULTILIB := both
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_HEADER_LIBRARIES += libcutils_headers
include $(BUILD_SHARED_LIBRARY)

#static library for the device
include $(CLEAR_VARS)

# Obtain Android Version
ANDROID_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))

LOCAL_SRC_FILES += icontrols.cpp                \
                   idiagnostic.cpp              \
                   iservice.cpp                 \
                   hwcserviceapi.cpp

LOCAL_MODULE := libhwcservice
LOCAL_CFLAGS += -fvisibility=default
LOCAL_SHARED_LIBRARIES :=  libbinder liblog libutils
LOCAL_MULTILIB := both
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_HEADER_LIBRARIES += libcutils_headers
include $(BUILD_STATIC_LIBRARY)
