LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	separate_rects_test.cpp \
	worker_test.cpp

LOCAL_MODULE := hwc-drm-tests
LOCAL_STATIC_LIBRARIES := libdrmhwc_utils
LOCAL_SHARED_LIBRARIES := hwcomposer.drm
LOCAL_C_INCLUDES := external/drm_hwcomposer

include $(BUILD_NATIVE_TEST)
