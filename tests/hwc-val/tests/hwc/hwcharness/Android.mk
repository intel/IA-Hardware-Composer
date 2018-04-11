LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

## Manage software and hardware variants.
VAL_HWC_HWC_COMMON_INC_PATH:=$(LOCAL_PATH)/../framework/common
include $(VAL_HWC_HWC_COMMON_INC_PATH)/Hwcval.mk

ifndef VAL_HWC_TOP
$(error VAL_HWC_TOP not defined)
endif

VAL_HWC_DOXYGEN_SRC_DIR := ""

VAL_HWC_FEATURE := hwc

VAL_HWC_LIBPNG_INC_PATH=$(ANDROID_BUILD_TOP)/external/libpng
VAL_HWC_ZLIB_INC_PATH=$(ANDROID_BUILD_TOP)/external/zlib

LOCAL_SRC_FILES:=\
    HwchInterface.cpp \
    HwchGlInterface.cpp \
    HwcHarness.cpp \
    HwchChoice.cpp \
    HwchLayerChoice.cpp \
    HwchTest.cpp \
    HwchTests.cpp \
    HwchRandomTest.cpp \
    HwchApiTest.cpp \
    HwchRandomModesTest.cpp \
    HwchDirectPlanesTest.cpp \
    HwchFlickerTests.cpp \
    HwchStressTests.cpp \
    HwchModeTests.cpp \
    HwchReplayParser.cpp \
    HwchReplayRunner.cpp \
    HwchReplayHWCLRunner.cpp \
    HwchReplayDSRunner.cpp \
    HwchReplayDSLayers.cpp \
    HwchLayer.cpp \
    HwchLayers.cpp \
    HwchFrame.cpp \
    HwchDisplay.cpp \
    HwchSystem.cpp \
    HwchPattern.cpp \
    HwchGlPattern.cpp \
    HwchPatternMgr.cpp \
    HwchBufferSet.cpp \
    HwchBufferDestroyer.cpp \
    HwchVSync.cpp \
    HwchBufferFormatConfig.cpp \
    HwchDisplaySpoof.cpp \
    HwchPngImage.cpp \
    HwchWatchdogThread.cpp \
    HwchAsyncEventGenerator.cpp \
    HwchInputGenerator.cpp \
    HwchRange.cpp

LOCAL_SRC_FILES += ../../../../../common/utils/hwcutils.cpp \
                   ../../../../../common/utils/hwcthread.cpp \
                   ../../../../../common/utils/fdhandler.cpp \
                   ../../../../../common/utils/hwcevent.cpp

ifdef HWCVAL_BUILD_INTERNAL_TESTS
LOCAL_SRC_FILES += HwchInternalTests.cpp
endif

LOCAL_SRC_FILES += HwchGlTests.cpp
   
LOCAL_C_INCLUDES += $(VAL_HWC_TOP)/intel
LOCAL_C_INCLUDES += ../../../../../public
LOCAL_C_INCLUDES += system/libhidl/transport/token/1.0/utils/include
LOCAL_C_INCLUDES += system/libhidl/base/include

LOCAL_SHARED_LIBRARIES += \
    libbinder \
    libdrm \
    libdrm_intel \
    libdl \
    libcutils \
    libutils \
    libhardware \
    libhwcservice \
    libui \
    libvalhwccommon \
    libsync \
    libEGL \
    libGLESv2 \
    libpng \
    liblog

# Obtain Android Version
ANDROID_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))

ifeq ($(shell test $(ANDROID_VERSION) -ge 8; echo $$?), 0)
LOCAL_SHARED_LIBRARIES += \
    android.hardware.graphics.bufferqueue@1.0
endif

ifneq ($(HWCVAL_RESOURCE_LEAK_CHECKING),)
LOCAL_SHARED_LIBRARIES += \
    libc_malloc_debug_leak
endif

LOCAL_MODULE_TAGS := optional eng
LOCAL_MODULE:= valhwcharness
LOCAL_MODULE_PATH=$(VAL_HWC_TARGET_TEST_PATH)/bin

include $(BUILD_EXECUTABLE)
