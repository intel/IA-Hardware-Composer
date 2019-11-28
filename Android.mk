#
#
#

ifeq ($(strip $(BOARD_USES_IA_HWCOMPOSER)), true)

HWC_PATH := $(call my-dir)
include $(CLEAR_VARS)
include $(HWC_PATH)/Android.common.mk

# comments platformhisi.cpp
LOCAL_SRC_FILES :=            \
    drmdevice.cpp             \
    resourcemanager.cpp       \
    autolock.cpp              \
    drmconnector.cpp          \
    drmcrtc.cpp               \
    drmdisplaycomposition.cpp \
    drmdisplaycompositor.cpp  \
    drmencoder.cpp            \
    drmeventlistener.cpp      \
    drmhwctwo.cpp             \
    drmmode.cpp               \
    drmplane.cpp              \
    drmproperty.cpp           \
    hwcutils.cpp              \
    platform.cpp              \
    vsyncworker.cpp           \
    worker.cpp                \
    platformdrmgeneric.cpp    \
    platformminigbm.cpp


LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
#LOCAL_MODULE_CLASS := LOCAL_SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := $(TARGET_SHLIB_SUFFIX)
include $(BUILD_SHARED_LIBRARY)


endif
