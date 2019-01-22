LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CPPFLAGS += \
        -O0 -g \
        -Wall -Wextra

ANDROID_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))

LOCAL_SRC_FILES += kmswrapper.cpp

LOCAL_MODULE := libkmscubewrapper
LOCAL_CFLAGS += -fvisibility=default
LOCAL_SHARED_LIBRARIES := libutils libbinder liblog
LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_HEADER_LIBRARIES += libcutils_headers
include $(BUILD_SHARED_LIBRARY)
