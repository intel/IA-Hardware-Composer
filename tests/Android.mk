ifeq ($(strip $(BOARD_USES_IA_HWCOMPOSER)),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_REQUIRED_MODULES := libjson-c \
                          libhwcomposer.$(TARGET_BOARD_PLATFORM)
#LOCAL_LDLIBS   += -L$(PRODUCT_OUT)/vendor/lib/hw/ -l:gralloc.android_ia.so -l:hwcomposer.android_ia.so


#Enabling EXPLICIT_SYNC causes segmentation faults.
#ifeq ($(strip $(BOARD_ENABLE_EXPLICIT_SYNC)),false)
LOCAL_CPPFLAGS += -DDISABLE_EXPLICIT_SYNC
LOCAL_CPPFLAGS += -DDISABLE_OVERLAY_USAGE
#endif


LOCAL_CPPFLAGS += \
	-DHWC2_USE_CPP11 \
	-DUSE_MINIGBM \
	-DHWC2_INCLUDE_STRINGIFICATION \
	-DUSE_ANDROID_SHIM \
	-fPIC -O2 \
	-D_FORTIFY_SOURCE=2 \
	-DDISABLE_TTY \
	-fstack-protector-strong \
	-fPIE -Wformat -Wformat-security

LOCAL_WHOLE_STATIC_LIBRARIES := \
  libhwcomposer.$(TARGET_BOARD_PLATFORM)

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libdrm \
	libEGL \
	libGLESv2 \
	libhardware \
	liblog \
	libsync \
	libui \
	libutils \
	libjson-c

LOCAL_SHARED_LIBRARIES += \
  libva \
  libva-android

LOCAL_C_INCLUDES := \
	system/core/include/utils \
	system/core/libsync \
	system/core/libsync/include \
	$(LOCAL_PATH)/third_party/json-c \
	$(LOCAL_PATH)/common \
	$(LOCAL_PATH)/../os/android \
	$(LOCAL_PATH)/../os    \
	$(LOCAL_PATH)/../wsi   \
	$(LOCAL_PATH)/../public \
	$(LOCAL_PATH)/../common/core \
	$(LOCAL_PATH)/../common/compositor \
	$(LOCAL_PATH)/../common/compositor/gl \
	$(LOCAL_PATH)/../common/display \
	$(LOCAL_PATH)/../common/utils \
	$(LOCAL_PATH)/../common/watchers \
	$(INTEL_MINIGBM)/cros_gralloc/

LOCAL_SRC_FILES := \
    common/layerrenderer.cpp \
    common/gllayerrenderer.cpp \
    common/glcubelayerrenderer.cpp \
    common/videolayerrenderer.cpp \
    common/imagelayerrenderer.cpp \
    common/cclayerrenderer.cpp \
    common/esTransform.cpp \
    common/jsonhandlers.cpp \
    apps/jsonlayerstest.cpp

LOCAL_MODULE_TAGS := optional eng

LOCAL_MODULE := testlayers
LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_EXECUTABLE)


# To copy json files on the target
# $1 is the *.sh file to copy
define script_to_copy
include $(CLEAR_VARS)
LOCAL_MODULE := $(1)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := intel
LOCAL_SRC_FILES := $(1)
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_PROPRIETARY_MODULE := true
#LOCAL_MODULE_RELATIVE_PATH := jsonconfigs
include $(BUILD_PREBUILT)

endef

json_list := jsonconfigs/colorcorrection.json jsonconfigs/kmscube1layer.json \
	     jsonconfigs/multiplelayersnovideo.json jsonconfigs/powermode.json \
	     jsonconfigs/video1layer_nv12.json jsonconfigs/example.json \
	     jsonconfigs/multiplelayers.json jsonconfigs/multiplelayersnovideo_powermode.json \
	     jsonconfigs/video1layer_bgra.json



$(foreach script,$(json_list),$(eval $(call script_to_copy,$(script))))

include $(LOCAL_PATH)/../Android.static.mk
endif
