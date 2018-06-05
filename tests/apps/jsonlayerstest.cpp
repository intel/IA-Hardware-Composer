/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <assert.h>
#include <sys/sysmacros.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/major.h>
#include <signal.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include "esUtil.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>

#include <libsync.h>

#include <gpudevice.h>
#include <hwclayer.h>
#include <hwcdefs.h>
#include <nativedisplay.h>
#include <platformdefines.h>
#include <spinlock.h>

#include "glcubelayerrenderer.h"
#include "videolayerrenderer.h"
#include "imagelayerrenderer.h"
#include "cclayerrenderer.h"
#include "jsonhandlers.h"

#include <nativebufferhandler.h>
#include "platformcommondefines.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int tty;

static void
reset_vt()
{
  struct vt_mode mode = { 0 };

  if (ioctl(tty, KDSETMODE, KD_TEXT))
    fprintf(stderr, "failed to set KD_TEXT mode on tty: %m\n");

  mode.mode = VT_AUTO;
  if (ioctl(tty, VT_SETMODE, &mode) < 0)
    fprintf(stderr, "could not reset vt handling\n");

  exit(0);
}


static void
handle_signal(int sig)
{
  reset_vt();
}

static int setup_tty() {
  struct vt_mode mode = {0};
  struct stat buf;
  int ret, kd_mode;

  tty = dup(STDIN_FILENO);

  if (fstat(tty, &buf) == -1 || major(buf.st_rdev) != TTY_MAJOR) {
    fprintf(stderr, "Please run the program in a vt \n");
    goto err_close;
  }

  ret = ioctl(tty, KDGETMODE, &kd_mode);
  if (ret) {
    fprintf(stderr, "failed to get VT mode: %m\n");
    return -1;
  }

  if (kd_mode != KD_TEXT) {
    fprintf(stderr,
            "Already in graphics mode, "
            "is a display server running?\n");
    goto err_close;
  }

  ioctl(tty, VT_ACTIVATE, minor(buf.st_rdev));
  ioctl(tty, VT_WAITACTIVE, minor(buf.st_rdev));

  ret = ioctl(tty, KDSETMODE, KD_GRAPHICS);
  if (ret) {
    fprintf(stderr, "failed to set KD_GRAPHICS mode on tty: %m\n");
    goto err_close;
  }

  mode.mode = VT_PROCESS;
  mode.relsig = 0;
  mode.acqsig = 0;
  if (ioctl(tty, VT_SETMODE, &mode) < 0) {
    fprintf(stderr, "failed to take control of vt handling\n");
    goto err_close;
  }

  struct sigaction act;
  act.sa_handler = handle_signal;
  act.sa_flags = SA_RESETHAND;
  sigaction(SIGINT, &act, NULL);
  sigaction(SIGSEGV, &act, NULL);
  sigaction(SIGABRT, &act, NULL);

  return 0;

err_close:
  close(tty);
  exit(0);
}

/* Exit after rendering the given number of frames. If 0, then continue
 * rendering forever.
 */
static uint64_t arg_frames = 0;

/*flag set to test displaymode*/
static int display_mode;
int force_mode = 0, config_index = 0, print_display_config = 0;

glContext gl;

struct frame {
  std::vector<std::unique_ptr<hwcomposer::HwcLayer>> layers;
  std::vector<std::unique_ptr<LayerRenderer>> layer_renderers;
  std::vector<std::vector<uint32_t>> layers_fences;
  std::vector<int32_t> fences;
};

bool init_gl() {
  EGLint major, minor, n;
  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                           EGL_NONE};

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                          EGL_NONE};

  gl.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (!eglInitialize(gl.display, &major, &minor)) {
    printf("failed to initialize EGL\n");
    return false;
  }

#define get_proc(name, proc)                  \
  do {                                        \
    gl.name = (proc)eglGetProcAddress(#name); \
    assert(gl.name);                          \
  } while (0)
  get_proc(glEGLImageTargetRenderbufferStorageOES,
           PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC);
  get_proc(eglCreateImageKHR, PFNEGLCREATEIMAGEKHRPROC);
  get_proc(eglCreateSyncKHR, PFNEGLCREATESYNCKHRPROC);
  get_proc(eglDestroySyncKHR, PFNEGLDESTROYSYNCKHRPROC);
  get_proc(eglWaitSyncKHR, PFNEGLWAITSYNCKHRPROC);
  get_proc(eglClientWaitSyncKHR, PFNEGLCLIENTWAITSYNCKHRPROC);
  get_proc(eglDupNativeFenceFDANDROID, PFNEGLDUPNATIVEFENCEFDANDROIDPROC);
  get_proc(glEGLImageTargetTexture2DOES, PFNGLEGLIMAGETARGETTEXTURE2DOESPROC);
  get_proc(eglDestroyImageKHR, PFNEGLDESTROYIMAGEKHRPROC);

  printf("Using display %p with EGL version %d.%d\n", gl.display, major, minor);

  printf("EGL Version \"%s\"\n", eglQueryString(gl.display, EGL_VERSION));
  printf("EGL Vendor \"%s\"\n", eglQueryString(gl.display, EGL_VENDOR));
  printf("EGL Extensions \"%s\"\n", eglQueryString(gl.display, EGL_EXTENSIONS));

  if (!eglBindAPI(EGL_OPENGL_ES_API)) {
    printf("failed to bind api EGL_OPENGL_ES_API\n");
    return false;
  }
  if (!eglChooseConfig(gl.display, config_attribs, &gl.config, 1, &n) ||
      n != 1) {
    printf("failed to choose config: %d\n", n);
    return false;
  }
  gl.context =
      eglCreateContext(gl.display, gl.config, EGL_NO_CONTEXT, context_attribs);
  if (gl.context == NULL) {
    printf("failed to create context\n");
    return false;
  }
  return true;
}

static struct frame frames[2];

class DisplayVSyncCallback : public hwcomposer::VsyncCallback {
 public:
  DisplayVSyncCallback() {
  }

  void Callback(uint32_t /*display*/, int64_t /*timestamp*/) {
  }
};

class HotPlugEventCallback : public hwcomposer::DisplayHotPlugEventCallback {
 public:
  HotPlugEventCallback(hwcomposer::GpuDevice *device) : device_(device) {
  }

  void Callback(std::vector<hwcomposer::NativeDisplay *> connected_displays) {
    spin_lock_.lock();
    connected_displays_.swap(connected_displays);
    if (connected_displays_.empty()) {
      spin_lock_.unlock();
      return;
    }

    hwcomposer::NativeDisplay *primary = connected_displays_.at(0);
    for (size_t i = 1; i < connected_displays_.size(); i++) {
      hwcomposer::NativeDisplay *cloned = connected_displays_.at(i);
      cloned->CloneDisplay(primary);
    }

    spin_lock_.unlock();
  }

  const std::vector<hwcomposer::NativeDisplay *> &GetConnectedDisplays() {
    spin_lock_.lock();
    PopulateConnectedDisplays();
    spin_lock_.unlock();
    return connected_displays_;
  }

  void PresentLayers(std::vector<hwcomposer::HwcLayer *> &layers,
                     std::vector<std::vector<uint32_t>> &layers_fences,
                     std::vector<int32_t> &fences) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    // We only support cloned mode for now.
    hwcomposer::NativeDisplay *primary = connected_displays_.at(0);
    int32_t retire_fence = -1;
    primary->Present(layers, &retire_fence);
    fences.emplace_back(retire_fence);
    // store fences for each display for each layer
    unsigned int fence_index = 0;
    for (auto layer : layers) {
      layers_fences[fence_index].emplace_back(layer->GetReleaseFence());
    }
  }

  void SetGamma(float red, float green, float blue) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      display->SetGamma(red, green, blue);
    }
  }

  void SetBrightness(char red, char green, char blue) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      display->SetBrightness(red, green, blue);
    }
  }

  void SetContrast(char red, char green, char blue) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      display->SetContrast(red, green, blue);
    }
  }

  void SetBroadcastRGB(const char *range_property) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->SetBroadcastRGB(range_property);
  }

  void SetPowerMode(uint32_t power_mode) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->SetPowerMode(power_mode);
  }

  uint16_t GetRGBABits(uint64_t color, uint16_t bpc, RGBA comp) {
    uint64_t comp_color;
    uint16_t nbits = (1 << bpc) - 1;

    comp_color = color & (uint64_t)(nbits << (bpc * comp));
    if (bpc <= 10)
      comp_color &= 0xffffffff;

    uint16_t comp_bits = (uint16_t)(comp_color >> (bpc * comp));
    return comp_bits;
  }

  void SetCanvasColor(uint64_t color, uint16_t bpc) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    /**
     * We are assuming that the color provided the user is in hex and in
     * ABGR format with R in LSB. For example, 0x000000ff would be Red.
     */
    for (auto &display : connected_displays_)
      display->SetCanvasColor(bpc, GetRGBABits(color, bpc, RED),
                              GetRGBABits(color, bpc, GREEN),
                              GetRGBABits(color, bpc, BLUE),
                              GetRGBABits(color, bpc, ALPHA));
  }

  void SetActiveConfig(uint32_t config) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->SetActiveConfig(config);
  }

  void GetDisplayAttribute(uint32_t config,
                           hwcomposer::HWCDisplayAttribute attribute,
                           int32_t *value) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;
    int32_t tempValue;
    for (auto &display : connected_displays_)
      display->GetDisplayAttribute(config, attribute, &tempValue);
    *value = tempValue;
  }

  void GetDisplayConfigs(uint32_t *num_configs, uint32_t *configs) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    uint32_t numConfigs, configIndex;

    if (connected_displays_.empty())
      return;
    for (auto &display : connected_displays_)
      display->GetDisplayConfigs(&numConfigs, NULL);
    *num_configs = numConfigs;
  }

  void PopulateConnectedDisplays() {
    if (connected_displays_.empty()) {
      device_->GetConnectedPhysicalDisplays(connected_displays_);

      for (auto &display : connected_displays_) {
        auto callback = std::make_shared<DisplayVSyncCallback>();
        display->RegisterVsyncCallback(callback, 0);
        display->VSyncControl(true);
      }
    }
  }

 private:
  std::vector<hwcomposer::NativeDisplay *> connected_displays_;
  hwcomposer::GpuDevice *device_;
  hwcomposer::SpinLock spin_lock_;
  uint16_t GetRGBABits(uint64_t color, uint16_t bpc, RGBA comp) const;
};

char json_path[1024];
TEST_PARAMETERS test_parameters;
hwcomposer::NativeBufferHandler *buffer_handler;

static uint32_t layerformat2gbmformat(LAYER_FORMAT format,
                                      uint32_t *usage_format, uint32_t *usage) {
  *usage = 0;

  switch (format) {
    case LAYER_FORMAT_C8:
      return DRM_FORMAT_C8;
    case LAYER_FORMAT_R8:
      return DRM_FORMAT_R8;
    case LAYER_FORMAT_GR88:
      return DRM_FORMAT_GR88;
    case LAYER_FORMAT_RGB332:
      return DRM_FORMAT_RGB332;
    case LAYER_FORMAT_BGR233:
      return DRM_FORMAT_BGR233;
    case LAYER_FORMAT_XRGB4444:
      return DRM_FORMAT_XRGB4444;
    case LAYER_FORMAT_XBGR4444:
      return DRM_FORMAT_XBGR4444;
    case LAYER_FORMAT_RGBX4444:
      return DRM_FORMAT_RGBX4444;
    case LAYER_FORMAT_BGRX4444:
      return DRM_FORMAT_BGRX4444;
    case LAYER_FORMAT_ARGB4444:
      return DRM_FORMAT_ARGB4444;
    case LAYER_FORMAT_ABGR4444:
      return DRM_FORMAT_ABGR4444;
    case LAYER_FORMAT_RGBA4444:
      return DRM_FORMAT_RGBA4444;
    case LAYER_FORMAT_BGRA4444:
      return DRM_FORMAT_BGRA4444;
    case LAYER_FORMAT_XRGB1555:
      return DRM_FORMAT_XRGB1555;
    case LAYER_FORMAT_XBGR1555:
      return DRM_FORMAT_XBGR1555;
    case LAYER_FORMAT_RGBX5551:
      return DRM_FORMAT_RGBX5551;
    case LAYER_FORMAT_BGRX5551:
      return DRM_FORMAT_BGRX5551;
    case LAYER_FORMAT_ARGB1555:
      return DRM_FORMAT_ARGB1555;
    case LAYER_FORMAT_ABGR1555:
      return DRM_FORMAT_ABGR1555;
    case LAYER_FORMAT_RGBA5551:
      return DRM_FORMAT_RGBA5551;
    case LAYER_FORMAT_BGRA5551:
      return DRM_FORMAT_BGRA5551;
    case LAYER_FORMAT_RGB565:
      return DRM_FORMAT_RGB565;
    case LAYER_FORMAT_BGR565:
      return DRM_FORMAT_BGR565;
    case LAYER_FORMAT_RGB888:
      return DRM_FORMAT_RGB888;
    case LAYER_FORMAT_BGR888:
      return DRM_FORMAT_BGR888;
    case LAYER_FORMAT_XRGB8888:
      return DRM_FORMAT_XRGB8888;
    case LAYER_FORMAT_XBGR8888:
      return DRM_FORMAT_XBGR8888;
    case LAYER_FORMAT_RGBX8888:
      return DRM_FORMAT_RGBX8888;
    case LAYER_FORMAT_BGRX8888:
      return DRM_FORMAT_BGRX8888;
    case LAYER_FORMAT_ARGB8888:
      return DRM_FORMAT_ARGB8888;
    case LAYER_FORMAT_ABGR8888:
      return DRM_FORMAT_ABGR8888;
    case LAYER_FORMAT_RGBA8888:
      return DRM_FORMAT_RGBA8888;
    case LAYER_FORMAT_BGRA8888:
      return DRM_FORMAT_BGRA8888;
    case LAYER_FORMAT_XRGB2101010:
      return DRM_FORMAT_XRGB2101010;
    case LAYER_FORMAT_XBGR2101010:
      return DRM_FORMAT_XBGR2101010;
    case LAYER_FORMAT_RGBX1010102:
      return DRM_FORMAT_RGBX1010102;
    case LAYER_FORMAT_BGRX1010102:
      return DRM_FORMAT_BGRX1010102;
    case LAYER_FORMAT_ARGB2101010:
      return DRM_FORMAT_ARGB2101010;
    case LAYER_FORMAT_ABGR2101010:
      return DRM_FORMAT_ABGR2101010;
    case LAYER_FORMAT_RGBA1010102:
      return DRM_FORMAT_RGBA1010102;
    case LAYER_FORMAT_BGRA1010102:
      return DRM_FORMAT_BGRA1010102;
    case LAYER_FORMAT_YUYV:
      return DRM_FORMAT_YUYV;
    case LAYER_FORMAT_YVYU:
      return DRM_FORMAT_YVYU;
    case LAYER_FORMAT_UYVY:
      return DRM_FORMAT_UYVY;
    case LAYER_FORMAT_VYUY:
      return DRM_FORMAT_VYUY;
    case LAYER_FORMAT_AYUV:
      return DRM_FORMAT_AYUV;
    case LAYER_FORMAT_NV12:
      return DRM_FORMAT_NV12;
    case LAYER_FORMAT_NV21:
      return DRM_FORMAT_NV21;
    case LAYER_FORMAT_NV16:
      return DRM_FORMAT_NV16;
    case LAYER_FORMAT_NV61:
      return DRM_FORMAT_NV61;
    case LAYER_FORMAT_YUV410:
      return DRM_FORMAT_YUV410;
    case LAYER_FORMAT_YVU410:
      return DRM_FORMAT_YVU410;
    case LAYER_FORMAT_YUV411:
      return DRM_FORMAT_YUV411;
    case LAYER_FORMAT_YVU411:
      return DRM_FORMAT_YVU411;
    case LAYER_FORMAT_YUV420:
      return DRM_FORMAT_YUV420;
    case LAYER_FORMAT_YVU420:
      return DRM_FORMAT_YVU420;
    case LAYER_FORMAT_YUV422:
      return DRM_FORMAT_YUV422;
    case LAYER_FORMAT_YVU422:
      return DRM_FORMAT_YVU422;
    case LAYER_FORMAT_YUV444:
      return DRM_FORMAT_YUV444;
    case LAYER_FORMAT_YVU444:
      return DRM_FORMAT_YVU444;
    case LAYER_HAL_PIXEL_FORMAT_YV12:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YV12;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_YVU420_ANDROID;
    case LAYER_HAL_PIXEL_FORMAT_Y8:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_Y8;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_R8;
    case LAYER_HAL_PIXEL_FORMAT_Y16:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_Y16;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_R16;
    case LAYER_HAL_PIXEL_FORMAT_YCbCr_444_888:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YCbCr_444_888;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_YUV444;
    case LAYER_HAL_PIXEL_FORMAT_YCbCr_422_I:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YCbCr_422_I;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_YUYV;
    case LAYER_HAL_PIXEL_FORMAT_YCbCr_422_SP:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YCbCr_422_SP;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_NV16;
    case LAYER_HAL_PIXEL_FORMAT_YCbCr_422_888:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YCbCr_422_888;
      *usage |= hwcomposer::kLayerVideo;
      return DRM_FORMAT_YUV422;
    case LAYER_HAL_PIXEL_FORMAT_YCbCr_420_888:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YCbCr_420_888;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_NV12;
    case LAYER_HAL_PIXEL_FORMAT_YCrCb_420_SP:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_YCrCb_420_SP;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_NV21;
    case LAYER_HAL_PIXEL_FORMAT_RAW16:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_RAW16;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_R16;
    case LAYER_HAL_PIXEL_FORMAT_RAW_OPAQUE:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_RAW_OPAQUE;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_R16;
    case LAYER_HAL_PIXEL_FORMAT_BLOB:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_BLOB;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_R8;
    case LAYER_ANDROID_SCALER_AVAILABLE_FORMATS_RAW16:
      *usage_format = LAYER_ANDROID_SCALER_AVAILABLE_FORMATS_RAW16;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_R16;
    case LAYER_HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL:
      *usage_format = LAYER_HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL;
      *usage = hwcomposer::kLayerVideo;
      return DRM_FORMAT_NV12_Y_TILED_INTEL;
    case LAYER_FORMAT_UNDEFINED:
      return (uint32_t)-1;
  }

  return (uint32_t)-1;
}

static void fill_hwclayer(hwcomposer::HwcLayer *pHwcLayer,
                          LAYER_PARAMETER *pParameter,
                          LayerRenderer *pRenderer) {
  pHwcLayer->SetTransform(pParameter->transform);
  pHwcLayer->SetSourceCrop(hwcomposer::HwcRect<float>(
      pParameter->source_crop_x, pParameter->source_crop_y,
      pParameter->source_crop_width, pParameter->source_crop_height));
  pHwcLayer->SetDisplayFrame(hwcomposer::HwcRect<int>(
      pParameter->frame_x, pParameter->frame_y, pParameter->frame_width,
      pParameter->frame_height), 0, 0);
  pHwcLayer->SetNativeHandle(pRenderer->GetNativeBoHandle());
}

static void init_frames(int32_t width, int32_t height) {
  LAYER_PARAMETER layer_parameter;
  size_t LAYER_PARAM_SIZE;
  if (display_mode) {
    layer_parameter.type = static_cast<LAYER_TYPE>(0);
    layer_parameter.format = static_cast<LAYER_FORMAT>(25);
    layer_parameter.transform = static_cast<LAYER_TRANSFORM>(0);
    layer_parameter.resource_path = "";
    layer_parameter.source_width = width;
    layer_parameter.source_height = height;
    layer_parameter.source_crop_x = 0;
    layer_parameter.source_crop_y = 0;
    layer_parameter.source_crop_width = width;
    layer_parameter.source_crop_height = height;
    layer_parameter.frame_x = 0;
    layer_parameter.frame_y = 0;
    layer_parameter.frame_width = width;
    layer_parameter.frame_height = height;
    LAYER_PARAM_SIZE = 1;
  } else {
    parseParametersJson(json_path, &test_parameters);
    LAYER_PARAM_SIZE = test_parameters.layers_parameters.size();
  }
  for (size_t i = 0; i < ARRAY_SIZE(frames); ++i) {
    struct frame *frame = &frames[i];
    frame->layers_fences.resize(LAYER_PARAM_SIZE);

    for (size_t j = 0; j < LAYER_PARAM_SIZE; ++j) {
      if (!display_mode) {
        layer_parameter = test_parameters.layers_parameters[j];
        if (layer_parameter.source_width > width)
          layer_parameter.source_width = width;

        if (layer_parameter.source_height > height)
          layer_parameter.source_height = height;

        if (layer_parameter.source_crop_width > width)
          layer_parameter.source_crop_width = width;

        if (layer_parameter.source_crop_height > height)
          layer_parameter.source_crop_height = height;

        if (layer_parameter.frame_width > width)
          layer_parameter.frame_width = width;

        if (layer_parameter.frame_height > height)
          layer_parameter.frame_height = height;
      }

      LayerRenderer *renderer = NULL;
      hwcomposer::HwcLayer *hwc_layer = NULL;
      uint32_t usage_format, usage;
      uint64_t modificators[4];
      uint32_t gbm_format =
          layerformat2gbmformat(layer_parameter.format, &usage_format, &usage);

      switch (layer_parameter.type) {
        // todo: more GL layer categories intead of only one CubeLayer
        case LAYER_TYPE_GL:
          renderer = new GLCubeLayerRenderer(buffer_handler, false);
          break;
#ifdef USE_MINIGBM
        case LAYER_TYPE_VIDEO:
          renderer = new VideoLayerRenderer(buffer_handler);
          break;
        case LAYER_TYPE_IMAGE:
          renderer = new ImageLayerRenderer(buffer_handler);
          break;
        case LAYER_TYPE_GL_TEXTURE:
          renderer = new GLCubeLayerRenderer(buffer_handler, true);
          break;
        case LAYER_TYPE_CC:
          renderer = new CCLayerRenderer(buffer_handler);
          break;
#endif
        default:
          printf("un-recognized layer type!\n");
          exit(-1);
      }

      if (!renderer->Init(layer_parameter.source_width,
                          layer_parameter.source_height, gbm_format,
                          usage_format, usage, &gl,
                          layer_parameter.resource_path.c_str())) {
        printf("\nrender init not successful\n");
        exit(-1);
      }

      hwc_layer = new hwcomposer::HwcLayer();
      fill_hwclayer(hwc_layer, &layer_parameter, renderer);
      frame->layers.push_back(std::unique_ptr<hwcomposer::HwcLayer>(hwc_layer));
      frame->layer_renderers.push_back(
          std::unique_ptr<LayerRenderer>(renderer));
    }
  }
}

static void print_help(void) {
  printf(
      "usage: testjsonlayers [-h|--help] [-f|--frames <frames>] [-j|--json "
      "<jsonfile>] [-p|--powermode <on/off/doze/dozesuspend>][--displaymode "
      "<print/forcemode displayconfigindex]\n");
}

static void parse_args(int argc, char *argv[]) {
  static const struct option longopts[] = {
      {"help", no_argument, NULL, 'h'},
      {"frames", required_argument, NULL, 'f'},
      {"json", required_argument, NULL, 'j'},
      {"displaymode", required_argument, &display_mode, 1},
      {0},
  };

  char *endptr;
  int opt;
  int longindex = 0;

  /* Suppress getopt's poor error messages */
  opterr = 0;

  while ((opt = getopt_long(argc, argv, "+:hf:j:", longopts,
                            /*longindex*/ &longindex)) != -1) {
    switch (opt) {
      case 0:
        if (!strcmp(optarg, "forcemode")) {
          force_mode = 1;
          config_index = atoi(argv[optind++]);
        }
        if (!strcmp(optarg, "print")) {
          print_display_config = 1;
        }
        break;
      case 'h':
        print_help();
        exit(0);
        break;
      case 'j':
        if (strlen(optarg) >= 1024) {
          printf("too long json file path, litmited less than 1024!\n");
          exit(0);
        }
        printf("optarg:%s\n", optarg);
        strcpy(json_path, optarg);
        break;
      case 'f':
        errno = 0;
        arg_frames = strtoul(optarg, &endptr, 0);
        if (errno || *endptr != '\0') {
          fprintf(stderr, "usage error: invalid value for <frames>\n");
          exit(EXIT_FAILURE);
        }
        break;
      case ':':
        fprintf(stderr, "usage error: %s requires an argument\n",
                argv[optind - 1]);
        exit(EXIT_FAILURE);
        break;
      case '?':
      default:
        assert(opt == '?');
        fprintf(stderr, "usage error: unknown option '%s'\n", argv[optind - 1]);
        exit(EXIT_FAILURE);
        break;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "usage error: trailing args\n");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char *argv[]) {
  int ret, fd, primary_width, primary_height;
#ifndef DISABLE_TTY
  setup_tty();
#endif
  hwcomposer::GpuDevice device;
  device.Initialize();
  auto callback = std::make_shared<HotPlugEventCallback>(&device);
  device.RegisterHotPlugEventCallback(callback);
  const std::vector<hwcomposer::NativeDisplay *> &displays =
      device.GetAllDisplays();
  if (displays.empty())
    return 0;

  hwcomposer::NativeDisplay *primary = displays.at(0);
  primary->SetActiveConfig(0);
  primary->SetPowerMode(hwcomposer::kOn);
  for (size_t i = 1; i < displays.size(); i++) {
    hwcomposer::NativeDisplay *cloned = displays.at(i);
    cloned->CloneDisplay(primary);
  }

  parse_args(argc, argv);

  fd = open("/dev/dri/renderD128", O_RDWR);
  if (fd == -1) {
    ETRACE("Can't open GPU file");
    exit(-1);
  }

  primary_width = primary->Width();
  primary_height = primary->Height();

  buffer_handler = hwcomposer::NativeBufferHandler::CreateInstance(fd);

  if (!buffer_handler)
    exit(-1);

  if (!init_gl()) {
    delete buffer_handler;
    exit(-1);
  }

  init_frames(primary_width, primary_height);

  if (display_mode) {
    printf("\nSUPPORTED DISPLAY MODE\n");
    uint32_t numConfigs, configIndex;
    callback->GetDisplayConfigs(&numConfigs, NULL);
    int32_t tempValue;
    printf("\nMode WidthxHeight\tRefreshRate\tXDpi\tYDpi\n");
    for (uint32_t i = 0; i < numConfigs; i++) {
      printf("%-6d", i);
      callback->GetDisplayAttribute(i, hwcomposer::HWCDisplayAttribute::kWidth,
                                    &tempValue);
      printf("%-4dx", tempValue);
      callback->GetDisplayAttribute(i, hwcomposer::HWCDisplayAttribute::kHeight,
                                    &tempValue);
      printf("%-6d\t", tempValue);
      callback->GetDisplayAttribute(
          i, hwcomposer::HWCDisplayAttribute::kRefreshRate, &tempValue);
      printf("%d\t", tempValue);
      callback->GetDisplayAttribute(i, hwcomposer::HWCDisplayAttribute::kDpiX,
                                    &tempValue);
      printf("%d\t", tempValue);
      callback->GetDisplayAttribute(i, hwcomposer::HWCDisplayAttribute::kDpiY,
                                    &tempValue);
      printf("%d\t\n", tempValue);
    }
    if (print_display_config)
      exit(0);
    if (force_mode) {
      callback->SetActiveConfig(config_index);
      primary_width = primary->Width();
      primary_height = primary->Height();
    }
  } else {
    callback->SetBroadcastRGB(test_parameters.broadcast_rgb.c_str());
    callback->SetGamma(test_parameters.gamma_r, test_parameters.gamma_g,
                       test_parameters.gamma_b);
    callback->SetBrightness(test_parameters.brightness_r,
                            test_parameters.brightness_g,
                            test_parameters.brightness_b);
    callback->SetContrast(test_parameters.contrast_r,
                          test_parameters.contrast_g,
                          test_parameters.contrast_b);
  }

  callback->SetCanvasColor(test_parameters.canvas_color, test_parameters.bpc);

  /* clear the color buffer */
  int64_t gpu_fence_fd = -1; /* out-fence from gpu, in-fence to kms */
  std::vector<hwcomposer::HwcLayer *> layers;
  uint32_t frame_total = 0;

  for (uint64_t i = 0; arg_frames == 0 || i < arg_frames; ++i) {
    struct frame *frame = &frames[i % ARRAY_SIZE(frames)];
    std::vector<hwcomposer::HwcLayer *>().swap(layers);
    for (int32_t &fence : frame->fences) {
      if (fence == -1)
        continue;

      sync_wait(fence, -1);
      close(fence);
      fence = -1;
    }

    for (uint32_t j = 0; j < frame->layers.size(); j++) {
      for (auto &fence : frame->layers_fences[j]) {
        if (fence != -1) {
          ret = sync_wait(fence, -1);
          close(fence);
          fence = -1;
        }
      }

      frame->layers_fences[j].clear();
      frame->layer_renderers[j]->Draw(&gpu_fence_fd);
      frame->layers[j]->SetAcquireFence(gpu_fence_fd);
      std::vector<hwcomposer::HwcRect<int>> damage_region;
      damage_region.emplace_back(frame->layers[j]->GetDisplayFrame());
      frame->layers[j]->SetSurfaceDamage(damage_region);
      layers.emplace_back(frame->layers[j].get());
    }

    callback->PresentLayers(layers, frame->layers_fences, frame->fences);
    frame_total++;

    if (!strcmp(test_parameters.power_mode.c_str(), "on")) {
      if (frame_total == 500) {
        usleep(10000);
        callback->SetPowerMode(hwcomposer::kOff);
        sleep(1);
        callback->SetPowerMode(hwcomposer::kOn);
        frame_total = 0;
      }
    } else if (!strcmp(test_parameters.power_mode.c_str(), "off")) {
      if (frame_total == 500) {
        usleep(30000);
        callback->SetPowerMode(hwcomposer::kOff);
        sleep(1);
        callback->SetPowerMode(hwcomposer::kOn);
        frame_total = 0;
      }
    } else if (!strcmp(test_parameters.power_mode.c_str(), "doze")) {
      if (frame_total == 500) {
        usleep(10000);
        callback->SetPowerMode(hwcomposer::kDoze);
        sleep(1);
        callback->SetPowerMode(hwcomposer::kOn);
        frame_total = 0;
      }
    } else if (!strcmp(test_parameters.power_mode.c_str(), "dozesuspend")) {
      if (frame_total == 500) {
        usleep(10000);
        callback->SetPowerMode(hwcomposer::kDozeSuspend);
        sleep(1);
        callback->SetPowerMode(hwcomposer::kOn);
        frame_total = 0;
      }
    }
  }

  callback->SetBroadcastRGB("Automatic");
  callback->SetGamma(1, 1, 1);
  callback->SetBrightness(0x80, 0x80, 0x80);
  callback->SetContrast(0x80, 0x80, 0x80);
  callback->SetCanvasColor(0x0, 8);

  for (size_t i = 0; i < ARRAY_SIZE(frames); ++i) {
    struct frame *frame = &frames[i];
    for (int32_t &fence : frame->fences) {
      if (fence == -1)
        continue;

      close(fence);
      fence = -1;
    }

    for (uint32_t j = 0; j < frame->layers.size(); j++) {
      for (auto &fence : frame->layers_fences[j]) {
        if (fence != -1) {
          close(fence);
          fence = -1;
        }
      }
    }
  }

  close(fd);
  delete buffer_handler;
  exit(ret);
}
