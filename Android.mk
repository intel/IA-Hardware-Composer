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

ifeq ($(strip $(BOARD_USES_DRM_HWCOMPOSER)),true)

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
	external/libdrm \
	external/libdrm/include/drm \
	system/core/include/utils \
	system/core/libsync \
	system/core/libsync/include \

LOCAL_SRC_FILES := \
	drmresources.cpp \
        drmcomposition.cpp \
        drmcompositor.cpp \
        drmcompositorworker.cpp \
	drmconnector.cpp \
	drmcrtc.cpp \
        drmdisplaycomposition.cpp \
        drmdisplaycompositor.cpp \
	drmencoder.cpp \
	drmmode.cpp \
	drmplane.cpp \
	drmproperty.cpp \
	glworker.cpp \
	hwcomposer.cpp \
	seperate_rects.cpp \
	virtualcompositorworker.cpp \
	vsyncworker.cpp \
	worker.cpp

ifeq ($(strip $(BOARD_DRM_HWCOMPOSER_BUFFER_IMPORTER)),nvidia-gralloc)
LOCAL_SRC_FILES += nvimporter.cpp
LOCAL_CPPFLAGS += -DUSE_NVIDIA_IMPORTER
else
LOCAL_C_INCLUDES += external/drm_gralloc
LOCAL_SRC_FILES += drmgenericimporter.cpp
LOCAL_CPPFLAGS += -DUSE_DRM_GENERIC_IMPORTER
endif

LOCAL_MODULE := hwcomposer.drm
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)

endif
