LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	worker_test.cpp

LOCAL_MODULE := hwc-drm-tests
LOCAL_VENDOR_MODULE := true
LOCAL_HEADER_LIBRARIES := libhardware_headers
LOCAL_STATIC_LIBRARIES := libdrmhwc_utils
LOCAL_SHARED_LIBRARIES := hwcomposer.drm
LOCAL_C_INCLUDES := external/drm_hwcomposer

include $(BUILD_NATIVE_TEST)
