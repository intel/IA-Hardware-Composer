# Obtain root HWF source path
LOCAL_PATH := $(call my-dir)

##################### libyunhal_hwf ###########################

include $(CLEAR_VARS)
LOCAL_MODULE := libyunhal_Hwf
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := usr/lib/yunhal/
LOCAL_RPATH := /usr/lib/yunhal/
#LOCAL_MODULE_RELATIVE_PATH := hw


LOCAL_CFLAGS := -std=c++11 -DYUN_HAL
LOCAL_LDFLAGS := -lm -lpthread

LOCAL_CPPFLAGS += \
	-DUSE_GL \
	-DYUN_HAL \
	-DLOCK_DIR_PREFIX='"/vendor/etc"' \
	-DHWC_DISPLAY_INI_PATH='"/vendor/etc/hwc_display.ini"'

LOCAL_C_INCLUDES := \
	system/core/include/utils \
	$(LOCAL_PATH)/public \
	$(LOCAL_PATH)/common/core \
	$(LOCAL_PATH)/common/compositor \
	$(LOCAL_PATH)/common/compositor/gl \
	$(LOCAL_PATH)/common/compositor/va \
	$(LOCAL_PATH)/common/display \
	$(LOCAL_PATH)/common/utils \
	$(LOCAL_PATH)/os \
	$(LOCAL_PATH)/os/alios \
	$(LOCAL_PATH)/wsi \
	$(LOCAL_PATH)/wsi/drm

LOCAL_SRC_FILES := os/alios/hwf_alioshal.cpp \
    common/core/gpudevice.cpp \
    common/core/logicaldisplaymanager.cpp \
    common/core/logicaldisplay.cpp \
    common/core/mosaicdisplay.cpp \
    common/core/hwclayer.cpp \
    common/core/overlaylayer.cpp \
    common/core/resourcemanager.cpp \
    common/core/framebuffermanager.cpp \
    common/utils/hwcutils.cpp \
    common/utils/hwcthread.cpp \
    common/utils/hwcevent.cpp \
    common/utils/fdhandler.cpp \
    common/utils/disjoint_layers.cpp \
    common/display/virtualdisplay.cpp \
    common/display/displayqueue.cpp \
    common/display/displayplanestate.cpp \
    common/display/displayplanemanager.cpp \
    common/display/vblankeventhandler.cpp \
    common/compositor/compositor.cpp \
    common/compositor/compositorthread.cpp \
    common/compositor/nativesurface.cpp \
    common/compositor/factory.cpp \
    common/compositor/renderstate.cpp \
    common/compositor/gl/glsurface.cpp \
    common/compositor/gl/glrenderer.cpp \
    common/compositor/gl/shim.cpp \
    common/compositor/gl/egloffscreencontext.cpp \
    common/compositor/gl/nativeglresource.cpp \
    common/compositor/gl/glprogram.cpp \
    common/compositor/va/varenderer.cpp \
    common/compositor/va/vautils.cpp \
    wsi/drm/drmdisplaymanager.cpp \
    wsi/drm/drmscopedtypes.cpp \
    wsi/drm/drmdisplay.cpp \
    wsi/drm/drmplane.cpp \
    wsi/drm/drmbuffer.cpp \
    wsi/physicaldisplay.cpp \
    os/platformcommondrmdefines.cpp \
    os/alios/platformdefines.cpp \
    os/alios/yallocbufferhandler.cpp \
    os/alios/yallocbufferhandler.h \
    os/alios/utils_alios.h

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(base-includes)  \
    $(graphics-includes) \
    $(systemd-includes) \
    $(properties-includes) \
    $(YUNOS_ROOT)/third_party/libdrm \
    $(YUNOS_ROOT)/third_party/libdrm/include/drm \
    $(YUNOS_ROOT)/third_party/libdrm/intel  \
    $(YUNOS_ROOT)/third_party/mesa/include  \
    $(YUNOS_ROOT)/vendor/intel/libva \
    $(YUNOS_ROOT)/vendor/intel/graphics/yalloc/include \
    $(YUNOS_ROOT)/yunhal/modules/include/yunhal \
    $(YUNOS_ROOT)/framework/libs/gui/surface/yunhal

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/include

#LOCAL_SHARED_LIBRARIES := \
    libdrm \
    libproperties \
    liblog \
    libgfx-cutils \
    libEGL \
    libGLESv1 \
    libGLESv2 \
    libva \
    libyunhal_Yalloc \
    libhal
LOCAL_SHARED_LIBRARIES := \
    libdrm \
    libproperties \
    liblog \
    libgfx-cutils \
    libva \
    libyunhal_Yalloc \
    libhal

# for glFlush/glFinish
ifeq ($(XMAKE_ENABLE_MESA_3),true)
        LOCAL_SHARED_LIBRARIES += \
	libGLESv1_CM \
	libGLESv2 \
	libEGL
else
        LOCAL_REQUIRED_MODULES += mesa
        LOCAL_LDFLAGS += \
	-lGLESv1_CM \
	-lGLESv2 \
	-lEGL
endif

include $(BUILD_SHARED_LIBRARY)

# config file
include $(CLEAR_VARS)

LOCAL_MODULE := hwf_cfg_prebuilt
LOCAL_SRC_FILES:= hwc_display.ini \
	hwc_display_virt.ini

#need check patch
LOCAL_MODULE_PATH := vendor/etc/

include $(BUILD_PREBUILT)

##################### hwf_test ###########################

#include $(CLEAR_VARS)

#LOCAL_CFLAGS := -std=c++11

#LOCAL_SRC_FILES := \
	tests/hwf/hwf_test.cpp

#LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/include \
    $(base-includes)  \
    $(graphics-includes) \
    $(YUNOS_ROOT)/vendor/intel/graphics/yalloc/include

#LOCAL_SHARED_LIBRARIES := \
    liblog \
    libbase \
    libgfx-cutils \
    libhal

#LOCAL_MODULE := hwf_test
#LOCAL_MODULE_TAGS := optional
#include $(BUILD_EXECUTABLE)

#Test case:
#include $(CLEAR_VARS)

#LOCAL_MODULE := hwf_test_prebuilt
#LOCAL_SRC_FILES:= tests/hwf/akiyo_352x288.i420 \
		tests/hwf/akiyo_352x288.nv12 \
		tests/hwf/akiyo_352x288.nv21 \
		tests/hwf/akiyo_352x288.uyvy \
		tests/hwf/akiyo_352x288.vyuy \
		tests/hwf/akiyo_352x288.yuyv \
		tests/hwf/akiyo_352x288.yv12 \
		tests/hwf/akiyo_352x288.yvyu \
		tests/hwf/scene_1280x720.nv12 \
		tests/hwf/scene_1920x1080.nv12
#LOCAL_MODULE_PATH := usr/bin

#include $(BUILD_PREBUILT)
