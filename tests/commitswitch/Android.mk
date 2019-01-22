LOCAL_PATH := $(call my-dir)
#include $(LOCAL_PATH)/Android.common.mk
include $(CLEAR_VARS)
LOCAL_CPPFLAGS += \
        -std=c99 \
        -O0 -g \
        -Wall -Wextra \
        -Wunused-parameter \
        -Wunused-variable \
        -Wunused-function \
        -Wno-error \
        -Wformat \
        $(DRM_CFLAGS) \
        $(GBM_CFLAGS) \
        $(EGL_CFLAGS) \
        $(GLES2_CFLAGS)

LOCAL_MODULE := kmscube

LOCAL_SHARED_LIBRARIES := \
        libcutils \
        libdrm_pri \
        libEGL \
        libGLESv3 \
        liblog \
        libsync \
        libui \
        libminigbm \
        libutils \
        libkmscubewrapper \
        libhwcservice

LOCAL_C_INCLUDES := \
        system/core/include/utils \
        system/core/libsync \
        system/core/libsync/include \
        frameworks/native/libs/nativewindow/include \
        frameworks/native/libs/nativewindow \
        hardware/intel/external/minigbm-intel \
        hardware/intel/external/drm-intel/include/drm/ \
        hardware/intel/external/drm-intel \
        $(LOCAL_PATH)/wrap \
        $(LOCAL_PATH)

LOCAL_SRC_FILES := \
        common.c \
        cube-smooth.c \
        cube-tex.c \
        drm-atomic.c \
        drm-common.c \
        drm-legacy.c \
        esTransform.c \
        frame-512x512-NV12.c \
        frame-512x512-RGBA.c \
        kmscube.c

LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)
