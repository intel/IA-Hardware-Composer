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


LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# TODO: We should specify this in device.mk
BUFFER_IMPORTER := gralloc

LOCAL_SHARED_LIBRARIES := \
	libdrm \
	libhardware \
	liblog \
	libstlport \
	libsync \
	libutils \

LOCAL_C_INCLUDES := \
	bionic \
	external/libdrm \
	external/libdrm/include/drm \
	external/stlport/stlport \
	system/core/include/utils \
	system/core/libsync/include \

LOCAL_CFLAGS :=
LOCAL_SRC_FILES := hwcomposer.cpp

ifeq ($(strip $(BUFFER_IMPORTER)),gralloc)
LOCAL_C_INCLUDES += external/drm_gralloc
LOCAL_SRC_FILES += hwcomposer_import_drm_gralloc.cpp
endif

LOCAL_MODULE := hwcomposer.drm
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)
