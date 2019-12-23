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

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libdrm_pri \
        libhardware \
        liblog \
        libsync \
        libui \
	libutils \
	libva \
	libva-android

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
        system/core/include/utils \
	system/core/libcutils/include \
	external/minigbm/cros_gralloc \
	hardware/intel/external/libva \
	hardware/intel/external/drm-intel/android
	#external/libdrm/android 

LOCAL_SRC_FILES := \
	 drmhwctwo.cpp \
        drmdisplaycomposition.cpp \
        drmdisplaycompositor.cpp \
        drmconnector.cpp \
        drmcrtc.cpp \
        drmdevice.cpp \
        drmencoder.cpp \
        drmeventlistener.cpp \
        drmmode.cpp \
        drmplane.cpp \
        drmproperty.cpp \
        resourcemanager.cpp \
        vsyncworker.cpp \
        platform.cpp \
        autolock.cpp \
        hwcutils.cpp \
	worker.cpp \
	platformdrmgeneric.cpp \
        platformminigbm.cpp \
	varenderer.cpp \
	vautils.cpp \
	gralloc1bufferhandler.cpp



LOCAL_C_INCLUDES += \
        system/core/libsync \
        system/core/libsync/include


LOCAL_CPPFLAGS += -DENABLE_DOUBLE_BUFFERING
LOCAL_CPPFLAGS += -DUSE_GRALLOC1
LOCAL_CPPFLAGS += -DMODIFICATOR_WA
LOCAL_CPPFLAGS += -DENABLE_RBC

LOCAL_CPPFLAGS += \
        -DHWC2_INCLUDE_STRINGIFICATION \
        -DHWC2_USE_CPP11 \
        -Wno-date-time \
        -DUSE_ANDROID_SHIM \
        -D_FORTIFY_SOURCE=2 \
        -fstack-protector-strong \
        -Wformat -Wformat-security \
        -std=c++14 -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
        -Wall -Wsign-compare -Wpointer-arith \
        -D_GNU_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
        -Wno-unused-parameter \
        -O3 \
	-Werror 





LOCAL_CPPFLAGS += -DENABLE_ANDROID_WA -DVA_SUPPORT_COLOR_RANGE
#LOCAL_CPPFLAGS += -DENABLE_DUMP_YUV_DATA
LOCAL_VENDOR_MODULE := true
LOCAL_MODULE := hwcomposer.drm_minigbm
LOCAL_CFLAGS += -fvisibility=default
LOCAL_PROPRIETARY_MODULE := true
include $(BUILD_SHARED_LIBRARY)
