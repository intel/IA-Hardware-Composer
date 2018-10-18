
VAL_HWC_SRC_PATH:= $(call my-dir)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

## Manage software and hardware variants.
include $(LOCAL_PATH)/Hwcval.mk

VAL_HWC_HWC_COMMON_INC_PATH=$(LOCAL_PATH)

ifndef VAL_HWC_TOP
$(error VAL_HWC_TOP not defined)
endif

VAL_HWC_DOXYGEN_SRC_DIR := ""

# Build the static library
LOCAL_SRC_FILES:=\
    HwcTestConfig.cpp \

LOCAL_CFLAGS += -fvisibility=default -DHWCVAL_LOG_$(HWCVAL_LOG_VERBOSITY) \
    -DHWCVAL_LOG_$(HWCVAL_LOG_DESTINATION)

LOCAL_C_INCLUDES += \
    $(VAL_HWC_HWC_COMMON_INC_PATH) \
    $(VAL_HWC_SRC_PATH) \
    $(LOCAL_PATH)

LOCAL_SHARED_LIBRARIES += libbinder
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= libvalhwcstatic
include $(BUILD_STATIC_LIBRARY)

# Now build the shared library
include $(CLEAR_VARS)

## Manage software and hardware variants.
include $(LOCAL_PATH)/Hwcval.mk

LOCAL_SRC_FILES:=\
    HwcTestConfig.cpp \
    HwcCrcReader.cpp \
    HwcTestState.cpp \
    HwcTestCrtc.cpp \
    HwcTestLog.cpp \
    HwcvalContent.cpp \
    HwcvalHwc2Content.cpp \
    HwcvalHwc2.cpp \
    HwcTestReferenceComposer.cpp \
    HwcTestCompValThread.cpp \
    HwcTestKernel.cpp \
    HwcTestUtil.cpp \
    HwcTestDebug.cpp \
    HwcTestDisplaySpoof.cpp \
    HwcvalWork.cpp \
    HwcvalWatchdog.cpp \
    HwcvalStall.cpp \
    HwcvalStatistics.cpp \
    HwcvalThreadTable.cpp \
    HwcvalLogParser.cpp \
    HwcvalDrmParser.cpp \
    HwcvalLogDisplay.cpp \
    HwcvalLayerListQueue.cpp \
    DrmShimCrtc.cpp \
    DrmShimPlane.cpp \
    DrmShimBuffer.cpp \
    DrmShimTransform.cpp \
    DrmShimChecks.cpp \
    BufferObject.cpp \
    DrmShimCallbackBase.cpp \
    DrmShimWork.cpp \
    SSIMUtils.cpp \
    CrcDebugfs.cpp \
    HwcvalDebug.cpp

LOCAL_SRC_FILES += ../../../../../../common/utils/hwcutils.cpp \
                   ../../../../../../common/utils/hwcthread.cpp \
                   ../../../../../../common/utils/fdhandler.cpp \
                   ../../../../../../common/utils/hwcevent.cpp
LOCAL_SRC_FILES += ../../../../../../os/android/gralloc1bufferhandler.cpp

LOCAL_CPPFLAGS += -DUSE_GRALLOC1

LOCAL_SHARED_LIBRARIES += \
    libdl \
    libhardware \
    libhwcservice \
    libbinder \
    libutils \
    libcutils \
    libsync \
    libui \
    libGLESv1_CM \
    libGLESv2 \
    libEGL \
    libdrm \
    liblog

LOCAL_C_INCLUDES += \
    $(VAL_HWC_HWC_COMMON_INC_PATH) \
    $(VAL_HWC_SRC_PATH) \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/../drm_shim/ \
    $(VAL_HWC_LIB_DRM_PATHS) \
    $(VAL_HWC_HARDWARE_COMPOSER_PATH)/os/android/libhwcservice \
    $(VAL_HWC_TOP)/intel \
    $(VAL_HWC_TOP)/hwcharness \
    $(VAL_HWC_HARDWARE_COMPOSER_PATH)/common/utils/val \
    $(VAL_HWC_HARDWARE_COMPOSER_PATH)/os/android/libhwcservice 

LOCAL_EXPORT_C_INCLUDE_DIRS += $(LOCAL_PATH)
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= libvalhwccommon

include $(BUILD_SHARED_LIBRARY)

