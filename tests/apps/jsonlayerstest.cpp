/*
 * Copyright (c) 2012 Arvin Schnell <arvin.schnell@gmail.com>
 * Copyright (c) 2012 Rob Clark <rob@ti.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* Based on a egl cube test app originally written by Arvin Schnell */

#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

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
#include <nativefence.h>
#include <spinlock.h>

#include "glcubelayerrenderer.h"
#include "videolayerrenderer.h"
#include "imagelayerrenderer.h"
#include "jsonhandlers.h"

#include <nativebufferhandler.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Exit after rendering the given number of frames. If 0, then continue
 * rendering forever.
 */
static uint64_t arg_frames = 0;

glContext gl;

struct frame {
  std::vector<std::unique_ptr<hwcomposer::HwcLayer>> layers;
  std::vector<std::unique_ptr<LayerRenderer>> layer_renderers;
  std::vector<std::vector<std::unique_ptr<hwcomposer::NativeFence>>>
      layers_fences;
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
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    connected_displays_.swap(connected_displays);

    for (auto &display : connected_displays_) {
      auto callback = std::make_shared<DisplayVSyncCallback>();
      display->RegisterVsyncCallback(callback, 0);
      display->VSyncControl(true);
    }
  }

  const std::vector<hwcomposer::NativeDisplay *> &GetConnectedDisplays() {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    return connected_displays_;
  }

  void PresentLayers(
      std::vector<hwcomposer::HwcLayer *> &layers,
      std::vector<std::vector<std::unique_ptr<hwcomposer::NativeFence>>> &
          layers_fences,
      std::vector<int32_t> &fences) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    PopulateConnectedDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_) {
      int32_t retire_fence = -1;
      display->Present(layers, &retire_fence);
      fences.emplace_back(retire_fence);
      // store fences for each display for each layer
      unsigned int fence_index = 0;
      for (auto layer : layers) {
        hwcomposer::NativeFence *fence = new hwcomposer::NativeFence();
        fence->Reset(layer->release_fence.Release());
        layers_fences[fence_index].emplace_back(fence);
      }
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

  void PopulateConnectedDisplays() {
    if (connected_displays_.empty()) {
      connected_displays_ = device_->GetConnectedPhysicalDisplays();

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
};

char json_path[1024];
TEST_PARAMETERS test_parameters;
hwcomposer::NativeBufferHandler *buffer_handler;

static uint32_t layerformat2gbmformat(LAYER_FORMAT format) {
  switch (format) {
    case LAYER_FORMAT_C8:
      return GBM_FORMAT_C8;
    case LAYER_FORMAT_R8:
      return GBM_FORMAT_R8;
    case LAYER_FORMAT_GR88:
      return GBM_FORMAT_GR88;
    case LAYER_FORMAT_RGB332:
      return GBM_FORMAT_RGB332;
    case LAYER_FORMAT_BGR233:
      return GBM_FORMAT_BGR233;
    case LAYER_FORMAT_XRGB4444:
      return GBM_FORMAT_XRGB4444;
    case LAYER_FORMAT_XBGR4444:
      return GBM_FORMAT_XBGR4444;
    case LAYER_FORMAT_RGBX4444:
      return GBM_FORMAT_RGBX4444;
    case LAYER_FORMAT_BGRX4444:
      return GBM_FORMAT_BGRX4444;
    case LAYER_FORMAT_ARGB4444:
      return GBM_FORMAT_ARGB4444;
    case LAYER_FORMAT_ABGR4444:
      return GBM_FORMAT_ABGR4444;
    case LAYER_FORMAT_RGBA4444:
      return GBM_FORMAT_RGBA4444;
    case LAYER_FORMAT_BGRA4444:
      return GBM_FORMAT_BGRA4444;
    case LAYER_FORMAT_XRGB1555:
      return GBM_FORMAT_XRGB1555;
    case LAYER_FORMAT_XBGR1555:
      return GBM_FORMAT_XBGR1555;
    case LAYER_FORMAT_RGBX5551:
      return GBM_FORMAT_RGBX5551;
    case LAYER_FORMAT_BGRX5551:
      return GBM_FORMAT_BGRX5551;
    case LAYER_FORMAT_ARGB1555:
      return GBM_FORMAT_ARGB1555;
    case LAYER_FORMAT_ABGR1555:
      return GBM_FORMAT_ABGR1555;
    case LAYER_FORMAT_RGBA5551:
      return GBM_FORMAT_RGBA5551;
    case LAYER_FORMAT_BGRA5551:
      return GBM_FORMAT_BGRA5551;
    case LAYER_FORMAT_RGB565:
      return GBM_FORMAT_RGB565;
    case LAYER_FORMAT_BGR565:
      return GBM_FORMAT_BGR565;
    case LAYER_FORMAT_RGB888:
      return GBM_FORMAT_RGB888;
    case LAYER_FORMAT_BGR888:
      return GBM_FORMAT_BGR888;
    case LAYER_FORMAT_XRGB8888:
      return GBM_FORMAT_XRGB8888;
    case LAYER_FORMAT_XBGR8888:
      return GBM_FORMAT_XBGR8888;
    case LAYER_FORMAT_RGBX8888:
      return GBM_FORMAT_RGBX8888;
    case LAYER_FORMAT_BGRX8888:
      return GBM_FORMAT_BGRX8888;
    case LAYER_FORMAT_ARGB8888:
      return GBM_FORMAT_ARGB8888;
    case LAYER_FORMAT_ABGR8888:
      return GBM_FORMAT_ABGR8888;
    case LAYER_FORMAT_RGBA8888:
      return GBM_FORMAT_RGBA8888;
    case LAYER_FORMAT_BGRA8888:
      return GBM_FORMAT_BGRA8888;
    case LAYER_FORMAT_XRGB2101010:
      return GBM_FORMAT_XRGB2101010;
    case LAYER_FORMAT_XBGR2101010:
      return GBM_FORMAT_XBGR2101010;
    case LAYER_FORMAT_RGBX1010102:
      return GBM_FORMAT_RGBX1010102;
    case LAYER_FORMAT_BGRX1010102:
      return GBM_FORMAT_BGRX1010102;
    case LAYER_FORMAT_ARGB2101010:
      return GBM_FORMAT_ARGB2101010;
    case LAYER_FORMAT_ABGR2101010:
      return GBM_FORMAT_ABGR2101010;
    case LAYER_FORMAT_RGBA1010102:
      return GBM_FORMAT_RGBA1010102;
    case LAYER_FORMAT_BGRA1010102:
      return GBM_FORMAT_BGRA1010102;
    case LAYER_FORMAT_YUYV:
      return GBM_FORMAT_YUYV;
    case LAYER_FORMAT_YVYU:
      return GBM_FORMAT_YVYU;
    case LAYER_FORMAT_UYVY:
      return GBM_FORMAT_UYVY;
    case LAYER_FORMAT_VYUY:
      return GBM_FORMAT_VYUY;
    case LAYER_FORMAT_AYUV:
      return GBM_FORMAT_AYUV;
    case LAYER_FORMAT_NV12:
      return GBM_FORMAT_NV12;
    case LAYER_FORMAT_NV21:
      return GBM_FORMAT_NV21;
    case LAYER_FORMAT_NV16:
      return GBM_FORMAT_NV16;
    case LAYER_FORMAT_NV61:
      return GBM_FORMAT_NV61;
    case LAYER_FORMAT_YUV410:
      return GBM_FORMAT_YUV410;
    case LAYER_FORMAT_YVU410:
      return GBM_FORMAT_YVU410;
    case LAYER_FORMAT_YUV411:
      return GBM_FORMAT_YUV411;
    case LAYER_FORMAT_YVU411:
      return GBM_FORMAT_YVU411;
    case LAYER_FORMAT_YUV420:
      return GBM_FORMAT_YUV420;
    case LAYER_FORMAT_YVU420:
      return GBM_FORMAT_YVU420;
    case LAYER_FORMAT_YUV422:
      return GBM_FORMAT_YUV422;
    case LAYER_FORMAT_YVU422:
      return GBM_FORMAT_YVU422;
    case LAYER_FORMAT_YUV444:
      return GBM_FORMAT_YUV444;
    case LAYER_FORMAT_YVU444:
      return GBM_FORMAT_YVU444;
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
      pParameter->frame_height));
  pHwcLayer->SetNativeHandle(pRenderer->GetNativeBoHandle());
}

static void init_frames(int32_t width, int32_t height) {
  parseParametersJson(json_path, &test_parameters);

  for (size_t i = 0; i < ARRAY_SIZE(frames); ++i) {
    struct frame *frame = &frames[i];
    frame->layers_fences.resize(test_parameters.layers_parameters.size());

    for (size_t j = 0; j < test_parameters.layers_parameters.size(); ++j) {
      LAYER_PARAMETER layer_parameter = test_parameters.layers_parameters[j];
      LayerRenderer *renderer = NULL;
      hwcomposer::HwcLayer *hwc_layer = NULL;
      uint32_t gbm_format = layerformat2gbmformat(layer_parameter.format);

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
#endif
        default:
          printf("un-recognized layer type!\n");
          exit(-1);
      }

      if (!renderer->Init(layer_parameter.source_width,
                          layer_parameter.source_height, gbm_format, &gl,
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
      "<jsonfile>] [-p|--powermode <on/off/doze/dozesuspend>]\n");
}

static void parse_args(int argc, char *argv[]) {
  static const struct option longopts[] = {
      {"help", no_argument, NULL, 'h'},
      {"frames", required_argument, NULL, 'f'},
      {"json", required_argument, NULL, 'j'},
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
  hwcomposer::GpuDevice device;
  device.Initialize();
  auto callback = std::make_shared<HotPlugEventCallback>(&device);
  device.RegisterHotPlugEventCallback(callback);
  const std::vector<hwcomposer::NativeDisplay *> &displays =
      callback->GetConnectedDisplays();
  if (displays.empty())
    return 0;

  parse_args(argc, argv);

  fd = open("/dev/dri/renderD128", O_RDWR);
  if (fd == -1) {
    ETRACE("Can't open GPU file");
    exit(-1);
  }

  primary_width = displays.at(0)->getWidth();
  primary_height = displays.at(0)->getHeight();

  buffer_handler = hwcomposer::NativeBufferHandler::CreateInstance(fd);

  if (!buffer_handler)
    exit(-1);

  if (!init_gl()) {
    delete buffer_handler;
    exit(-1);
  }

  init_frames(primary_width, primary_height);

  callback->SetBroadcastRGB(test_parameters.broadcast_rgb.c_str());
  callback->SetGamma(test_parameters.gamma_r, test_parameters.gamma_g,
                     test_parameters.gamma_b);
  callback->SetBrightness(test_parameters.brightness_r,
                          test_parameters.brightness_g,
                          test_parameters.brightness_b);
  callback->SetContrast(test_parameters.contrast_r, test_parameters.contrast_g,
                        test_parameters.contrast_b);

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
        if (fence->get() != -1) {
          ret = sync_wait(fence->get(), -1);
        }
      }

      frame->layers_fences[j].clear();
      frame->layer_renderers[j]->Draw(&gpu_fence_fd);
      frame->layers[j]->acquire_fence.Reset(gpu_fence_fd);
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

  close(fd);
  delete buffer_handler;
  exit(ret);
}
