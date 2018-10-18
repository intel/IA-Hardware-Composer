LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

## Manage software and hardware variants.
VAL_HWC_HWC_COMMON_INC_PATH:=$(LOCAL_PATH)/../common
include $(VAL_HWC_HWC_COMMON_INC_PATH)/Hwcval.mk

ifndef VAL_HWC_TOP
$(warning VAL_HWC_TOP not defined)
VAL_HWC_TOP=$(LOCAL_PATH)/../../../..
endif

VAL_HWC_DOXYGEN_SRC_DIR := ""

VAL_HWC_FEATURE := hwc

LOCAL_SRC_FILES := \
    drm_shim.cpp \
    DrmShimEventHandler.cpp \
    DrmShimPropertyManager.cpp

LOCAL_SHARED_LIBRARIES += \
    libdl \
    libcutils \
    libutils \
    libhardware \
    libvalhwccommon \
    liblog

LOCAL_STATIC_LIBRARIES +=

LOCAL_C_INCLUDES += \
    $(VAL_HWC_HWC_COMMON_INC_PATH) \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../hwc_shim/ \
    $(VAL_HWC_TOP)/hwcharness \
    $(VAL_HWC_HARDWARE_COMPOSER_PATH)/../libdrm \
    $(VAL_HWC_HARDWARE_COMPOSER_PATH)/os/android/libhwcservice \

LOCAL_LDLIBS += -ldl

LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= libvalhwc_drmshim

include $(BUILD_SHARED_LIBRARY)

