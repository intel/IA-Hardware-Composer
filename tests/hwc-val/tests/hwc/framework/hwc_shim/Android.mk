
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

## Manage software and hardware variants.
VAL_HWC_HWC_COMMON_INC_PATH:=$(LOCAL_PATH)/../common
include $(VAL_HWC_HWC_COMMON_INC_PATH)/Hwcval.mk

ifndef VAL_HWC_TOP
#$(error VAL_HWC_TOP not defined)
VAL_HWC_TOP=$(LOCAL_PATH)/../../../..
endif

VAL_HWC_DOXYGEN_SRC_DIR := ""

VAL_HWC_FEATURE := hwc

LOCAL_SRC_FILES:=\
    hwc_shim.cpp \
    HwcDrmShimCallback.cpp

LOCAL_CFLAGS += -rdynamic -O0  -DHWCVAL_LOG_$(HWCVAL_LOG_VERBOSITY) \
    -DHWCVAL_LOG_$(HWCVAL_LOG_DESTINATION) \

LOCAL_C_INCLUDES += \
    $(VAL_HWC_HWC_COMMON_INC_PATH) \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../common/ \
    $(LOCAL_PATH)/../drm_shim/ \
    $(VAL_HWC_TOP)/hwcharness \
    $(VAL_HWC_HARDWARE_COMPOSER_PATH)/os/android/libhwcservice \

LOCAL_SHARED_LIBRARIES += \
    libdrm \
    libdrm_intel \
    libdl \
    libcutils \
    libutils \
    libhardware \
    libhwcservice \
    libvalhwccommon \
    libbinder \
    libsync \
    liblog


LOCAL_STATIC_LIBRARIES +=

LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= valhwc_composershim

include $(BUILD_SHARED_LIBRARY)

