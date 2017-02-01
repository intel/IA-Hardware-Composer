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
#include <gbm.h>
#include <drm_fourcc.h>

#include "esUtil.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>

#include <libsync.h>

#include <gpudevice.h>
#include <hwclayer.h>
#include <nativedisplay.h>
#include <platformdefines.h>
#include <nativefence.h>
#include <spinlock.h>

#include "glcubelayerrenderer.h"
#include "videolayerrenderer.h"
#include "imagelayerrenderer.h"
#include "layerfromjson.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Exit after rendering the given number of frames. If 0, then continue
 * rendering forever.
 */
static uint64_t arg_frames = 0;

glContext gl;

struct frame {
  std::vector<std::unique_ptr<hwcomposer::HwcLayer> > layers;
  std::vector<std::unique_ptr<LayerRenderer> > layer_renderers;
  // NativeFence release_fence;
};

static void setHWCLayer(LAYER_PARAMETER *pParameter, LayerRenderer *pRenderer) {
}

bool init_gl() {
  EGLint major, minor, n;
  static const EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3,
                                           EGL_NONE};

  static const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_DONT_CARE,
                                          EGL_NONE};

  gl.display = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA,
                                     EGL_DEFAULT_DISPLAY, NULL);

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

class HotPlugEventCallback : public hwcomposer::DisplayHotPlugEventCallback {
 public:
  HotPlugEventCallback(hwcomposer::GpuDevice *device) : device_(device) {
  }

  void Callback(std::vector<hwcomposer::NativeDisplay *> connected_displays) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    connected_displays_.swap(connected_displays);
  }

  const std::vector<hwcomposer::NativeDisplay *> &GetConnectedDisplays() {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    if (connected_displays_.empty())
      connected_displays_ = device_->GetConnectedPhysicalDisplays();

    return connected_displays_;
  }

  void PresentLayers(std::vector<hwcomposer::HwcLayer *> &layers) {
    hwcomposer::ScopedSpinLock lock(spin_lock_);
    if (connected_displays_.empty())
      connected_displays_ = device_->GetConnectedPhysicalDisplays();

    if (connected_displays_.empty())
      return;

    for (auto &display : connected_displays_)
      display->Present(layers);
  }

 private:
  std::vector<hwcomposer::NativeDisplay *> connected_displays_;
  hwcomposer::GpuDevice *device_;
  hwcomposer::SpinLock spin_lock_;
};

static struct { struct gbm_device *dev; } gbm;

struct drm_fb {
  struct gbm_bo *bo;
};

static int init_gbm(int fd) {
  gbm.dev = gbm_create_device(fd);
  if (!gbm.dev) {
    printf("failed to create gbm device\n");
    return -1;
  }

  return 0;
}

char json_path[1024];

static uint32_t layerformat2gbmformat(LAYER_FORMAT format) {
  uint32_t iformat = (uint32_t)format;
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
  std::vector<LAYER_PARAMETER> layer_parameters;
  parseLayersFromJson(json_path, layer_parameters);

  for (int i = 0; i < ARRAY_SIZE(frames); ++i) {
    struct frame *frame = &frames[i];

    for (int j = 0; j < layer_parameters.size(); ++j) {
      LAYER_PARAMETER layer_parameter = layer_parameters[j];
      LayerRenderer *renderer = NULL;
      hwcomposer::HwcLayer *hwc_layer = NULL;
      uint32_t gbm_format = layerformat2gbmformat(layer_parameter.format);

      switch (layer_parameter.type) {
        // todo: more GL layer categories intead of only one CubeLayer
        case LAYER_TYPE_GL:
          renderer = new GLCubeLayerRenderer(gbm.dev);
          break;
        case LAYER_TYPE_VIDEO:
          renderer = new VideoLayerRenderer(gbm.dev);
          break;
        case LAYER_TYPE_IMAGE:
          renderer = new ImageLayerRenderer(gbm.dev);
          break;
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
      "<jsonfile>]\n");
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

  while ((opt = getopt_long(argc, argv, "+:hfj:", longopts,
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
  struct drm_fb *fb;
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
  primary_width = displays.at(0)->Width();
  primary_height = displays.at(0)->Height();

  ret = init_gbm(fd);
  if (ret) {
    printf("failed to initialize GBM\n");
    close(fd);
    return ret;
  }

  if (!init_gl())
    exit(-1);
  init_frames(primary_width, primary_height);

  /* clear the color buffer */
  int64_t gpu_fence_fd = -1; /* out-fence from gpu, in-fence to kms */
  std::vector<hwcomposer::HwcLayer *> layers;

  for (uint64_t i = 1; arg_frames == 0 || i < arg_frames; ++i) {
    struct frame *frame = &frames[i % ARRAY_SIZE(frames)];

    for (uint32_t j = 0; j < frame->layers.size(); j++) {
      if (frame->layers[j]->release_fence.get() != -1) {
        ret = sync_wait(frame->layers[j]->release_fence.get(), 1000);
        frame->layers[j]->release_fence.Reset(-1);
        if (ret) {
          printf("failed waiting on sync fence: %s\n", strerror(errno));
          return -1;
        }
      }
    }

    std::vector<hwcomposer::HwcLayer *>().swap(layers);
    for (uint32_t j = 0; j < frame->layers.size(); j++) {
      frame->layer_renderers[j]->Draw(&gpu_fence_fd);
      frame->layers[j]->acquire_fence = gpu_fence_fd;
      layers.emplace_back(frame->layers[j].get());
    }

    callback->PresentLayers(layers);
  }

  close(fd);
  return ret;
}
