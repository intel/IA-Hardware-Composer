#
#
#

ifeq ($(strip $(BOARD_USES_IA_HWCOMPOSER)), true)

HWC_PATH := $(call my-dir)

HWC_VERSION_GIT_BRANCH := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse --abbrev-ref HEAD; popd > /dev/null)
HWC_VERSION_GIT_SHA := $(shell pushd $(HWC_PATH) > /dev/null; git rev-parse HEAD; popd > /dev/null)


ANDROID_M := 6
ANDROID_N := 7
ANDROID_O := 8
ANDROID_P := 9
ANDROID_VERSION_6 := $(ANDROID_M)
ANDROID_VERSION_7 := $(ANDROID_N)
ANDROID_VERSION_8 := $(ANDROID_O)
ANDROID_VERSION_9 := $(ANDROID_P)
ANDROID_VERSION_O := $(ANDROID_O)
ANDROID_VERSION_OMR1 := $(ANDROID_O)
ANDROID_SUPPORTED_VERSIONS := 6 7 O 8 OMR1 9

$(info "PLATFORM_VERSION $(PLATFORM_VERSION)")
ANDROID_MAJOR_VERSION := $(word 1, $(subst ., , $(PLATFORM_VERSION)))
$(foreach item, $(ANDROID_SUPPORTED_VERSIONS),\
$(if $(call streq,$(ANDROID_MAJOR_VERSION),$(item)),\
$(eval ANDROID_VERSION := $(ANDROID_VERSION_$(item))),))
$(info "ANDROID_VERSION $(ANDROID_VERSION)")

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SHARED_LIBRARIES := \
	libcutils             \
	libdrm_pri            \
	libhardware           \
	liblog                \
    libsync               \
	libui                 \
	libutils

LOCAL_C_INCLUDES :=                             \
    system/core/include/utils                   \
    frameworks/native/libs/nativewindow/include \
    frameworks/native/libs/nativewindow

LOCAL_C_INCLUDES += $(INTEL_MINIGBM)/cros_gralloc/

LOCAL_CPPFLAGS +=                  \
    -DHWC2_USE_CPP11               \
    -DHWC2_INCLUDE_STRINGIFICATION

LOCAL_STATIC_LIBRARIES := libdrmhwc_utils

endif
