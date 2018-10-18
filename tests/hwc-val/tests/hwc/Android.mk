
# This is a top level makefile to build the HWC test framework and related tests.
# Building from this level will build everything needed to run the tests.
ifeq ($(HWC_PATH),)
    HWC_PATH := $(call my-dir)/../../../..
endif

VAL_HWC_HARDWARE_COMPOSER_PATH:=$(HWC_PATH)

VAL_HWC_TOP=$(VAL_HWC_HARDWARE_COMPOSER_PATH)/tests/hwc-val/tests/hwc

HWCVAL_TOP_SRC_PATH:= $(call my-dir)

VAL_HWC_TARGET_TEST_PATH := $(TARGET_OUT_VENDOR)

include $(HWCVAL_TOP_SRC_PATH)/framework/common/DetermineAndroidVersion.mk
include $(HWCVAL_TOP_SRC_PATH)/../../regex-re2_temp/Android.mk

include $(HWCVAL_TOP_SRC_PATH)/framework/common/Android.mk
include $(HWCVAL_TOP_SRC_PATH)/framework/drm_shim/Android.mk
include $(HWCVAL_TOP_SRC_PATH)/framework/hwc_shim/Android.mk

include $(HWCVAL_TOP_SRC_PATH)/tests/Android.mk

include $(HWCVAL_TOP_SRC_PATH)/hwcharness/Android.mk
