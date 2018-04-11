ifndef VAL_HWC_TOP
$(error VAL_HWC_TOP not defined)
endif

LOCAL_PATH:= $(call my-dir)
VAL_HWC_DOXYGEN_SRC_DIR:= $(call my-dir)/../
include $(CLEAR_VARS)

VAL_HWC_FEATURE := hwc
VAL_HWC_HWC_COMMON_INC_PATH:=$(LOCAL_PATH)/../framework/common

# Path for binaries
ifeq ($(VAL_HWC_EXTERNAL_BUILD),)
    VAL_HWC_TARGET_TEST_PATH := $(TARGET_OUT_VENDOR)
endif

# LOCAL_C_INCLUDE is cleared between each build target

VAL_HWC_HWC_TEST_LOCAL_C_INCLUDES := \
        $(LOCAL_PATH) \
        $(LOCAL_PATH)/../framework/hwc_shim/ \
        $(VAL_HWC_HWC_COMMON_INC_PATH) \
        $(VAL_HWC_LIB_DRM_PATHS) \
        $(VAL_HWC_LIB_LOGGER) \
        $(VAL_HWC_TOP)/intel

VAL_HWC_HWC_TEST_LOCAL_SHARED_LIBRARIES := \
        libc \
        libcutils \
        libutils \
        libbinder \
        libui \
        libgui \
        libcutils \
        libsync \
        libhardware \
        liblog \
        libhwcservice

VAL_HWC_HWC_TEST_LOCAL_STATIC_LIBRARIES := \
    libvalhwc_test \
    libvalhwcstatic

VAL_HWC_HWC_TEST_CFLAGS += \
    -O0 -ggdb  -DHWCVAL_LOG_$(HWCVAL_LOG_VERBOSITY)
#    -DOGL_GEM \
#    -DLINUX \

LOCAL_SRC_FILES:= \
        display_info.cpp \
        SurfaceSender.cpp \
        test_base.cpp 

LOCAL_CFLAGS := $(VAL_HWC_HWC_TEST_CFLAGS)
LOCAL_C_INCLUDES += $(VAL_HWC_HWC_TEST_LOCAL_C_INCLUDES)

LOCAL_C_INCLUDES += \
	$(INTEL_MINIGBM)/cros_gralloc/


LOCAL_SHARED_LIBRARIES += $(VAL_HWC_HWC_TEST_LOCAL_SHARED_LIBRARIES)
LOCAL_STATIC_LIBRARIES += libvalhwcstatic
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= libvalhwc_test

include $(VAL_HWC_HWC_COMMON_INC_PATH)/Hwcval.mk
include $(BUILD_STATIC_LIBRARY)

# the following construct allows the inclusion of multiple prebuilt libraries
# into the project

define my_add_executable
    include $(CLEAR_VARS)
    LOCAL_CFLAGS := $(VAL_HWC_HWC_TEST_CFLAGS)
    LOCAL_C_INCLUDES += $(VAL_HWC_HWC_TEST_LOCAL_C_INCLUDES)
    LOCAL_SHARED_LIBRARIES += $(VAL_HWC_HWC_TEST_LOCAL_SHARED_LIBRARIES)
    LOCAL_STATIC_LIBRARIES += $(VAL_HWC_HWC_TEST_LOCAL_STATIC_LIBRARIES)
    LOCAL_MODULE_TAGS:= optional eng
    LOCAL_MODULE_PATH:=$(VAL_HWC_TARGET_TEST_PATH)/bin
    LOCAL_MODULE:= $1

    include $(VAL_HWC_HWC_COMMON_INC_PATH)/Hwcval.mk
    LOCAL_SRC_FILES:= $1.cpp
    include $(BUILD_EXECUTABLE)
endef

test_names := \
    valhwc_util \
    valhwc_monitor_test \
    valhwc_home_test \
    valhwc_lock_test \
    valhwc_notification_test \
    valhwc_recent_apps_test \
    valhwc_dialog_test \
    valhwc_game_test \
    valhwc_nv12_video_full_test \
    valhwc_nv12_video_part_test \
    valhwc_camera_test \
    valhwc_gallery_test \
    valhwc_crc_test
$(foreach item,$(test_names),$(eval $(call my_add_executable,$(item))))





