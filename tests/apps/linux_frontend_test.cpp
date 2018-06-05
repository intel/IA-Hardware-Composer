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
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/kd.h>
#include <linux/major.h>
#include <linux/vt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// clang-format off
#include "esUtil.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
// clang-format on

#include <libsync.h>

#include <gpudevice.h>
#include <hwclayer.h>
#include <hwcdefs.h>
#include <nativedisplay.h>
#include <platformdefines.h>
#include <spinlock.h>

#include <iahwc.h>

#include "glcubelayerrenderer.h"
#include "videolayerrenderer.h"
#include "imagelayerrenderer.h"
#include "cclayerrenderer.h"
#include "jsonhandlers.h"

#include <nativebufferhandler.h>
#include "platformcommondefines.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

int tty;

static void reset_vt() {
  struct vt_mode mode = {0};

  if (ioctl(tty, KDSETMODE, KD_TEXT))
    fprintf(stderr, "failed to set KD_TEXT mode on tty: %m\n");

  mode.mode = VT_AUTO;
  if (ioctl(tty, VT_SETMODE, &mode) < 0)
    fprintf(stderr, "could not reset vt handling\n");

  exit(0);
}

static void handle_signal(int sig) {
  if (sig == 11)
    printf("received SIGSEGV\n");
  fflush(stdout);
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

  atexit(reset_vt);

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
LAYER_PARAMETER layer_parameter;

glContext gl;

struct iahwc_backend {
  iahwc_module_t *iahwc_module;
  iahwc_device_t *iahwc_device;
  IAHWC_PFN_GET_NUM_DISPLAYS iahwc_get_num_displays;
  IAHWC_PFN_REGISTER_CALLBACK iahwc_register_callback;
  IAHWC_PFN_DISPLAY_GET_INFO iahwc_get_display_info;
  IAHWC_PFN_DISPLAY_GET_NAME iahwc_get_display_name;
  IAHWC_PFN_DISPLAY_GET_CONFIGS iahwc_get_display_configs;
  IAHWC_PFN_DISPLAY_SET_GAMMA iahwc_set_display_gamma;
  IAHWC_PFN_DISPLAY_SET_CONFIG iahwc_set_display_config;
  IAHWC_PFN_DISPLAY_GET_CONFIG iahwc_get_display_config;
  IAHWC_PFN_PRESENT_DISPLAY iahwc_present_display;
  IAHWC_PFN_CREATE_LAYER iahwc_create_layer;
  IAHWC_PFN_LAYER_SET_BO iahwc_layer_set_bo;
  IAHWC_PFN_LAYER_SET_ACQUIRE_FENCE iahwc_layer_set_acquire_fence;
  IAHWC_PFN_LAYER_SET_USAGE iahwc_layer_set_usage;
  IAHWC_PFN_LAYER_SET_TRANSFORM iahwc_layer_set_transform;
  IAHWC_PFN_LAYER_SET_SOURCE_CROP iahwc_layer_set_source_crop;
  IAHWC_PFN_LAYER_SET_DISPLAY_FRAME iahwc_layer_set_display_frame;
  IAHWC_PFN_LAYER_SET_SURFACE_DAMAGE iahwc_layer_set_surface_damage;
  IAHWC_PFN_VSYNC iahwc_vsync;

} * backend;

struct frame {
  std::vector<iahwc_layer_t> layers;
  std::vector<gbm_bo *> layer_bos;
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

char json_path[1024];
char log_path[1024];
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

static void fill_hwclayer(iahwc_layer_t layer_handle_,
                          LAYER_PARAMETER *pParameter,
                          LayerRenderer *pRenderer) {
  backend->iahwc_layer_set_transform(backend->iahwc_device, 0, layer_handle_,
                                     pParameter->transform);
  backend->iahwc_layer_set_source_crop(
      backend->iahwc_device, 0, layer_handle_,
      {pParameter->source_crop_x, pParameter->source_crop_y,
       pParameter->source_crop_width, pParameter->source_crop_height});
  backend->iahwc_layer_set_display_frame(
      backend->iahwc_device, 0, layer_handle_,
      {pParameter->frame_x, pParameter->frame_y, pParameter->frame_width,
       pParameter->frame_height});
}

static void init_frames(int32_t width, int32_t height) {
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
    // hwcomposer::HwcLayer *hwc_layer = NULL;
    iahwc_layer_t layer_handle_;
    backend->iahwc_create_layer(backend->iahwc_device, 0, &layer_handle_);
    uint32_t usage_format, usage;
    uint64_t modificators[4];
    uint32_t gbm_format =
        layerformat2gbmformat(layer_parameter.format, &usage_format, &usage);

    switch (layer_parameter.type) {
      case LAYER_TYPE_GL:
        renderer = new GLCubeLayerRenderer(buffer_handler, true);
        break;
      default:
        printf("un-recognized layer type!\n");
        exit(-1);
    }

    if (!renderer->Init(layer_parameter.source_width,
                        layer_parameter.source_height, gbm_format, usage_format,
                        usage, &gl, layer_parameter.resource_path.c_str())) {
      printf("\nrender init not successful\n");
      exit(-1);
    }

    fill_hwclayer(layer_handle_, &layer_parameter, renderer);
    for (size_t i = 0; i < ARRAY_SIZE(frames); ++i) {
      struct frame *frame = &frames[i];
      frame->layers_fences.resize(LAYER_PARAM_SIZE);
      frame->layers.push_back(layer_handle_);
      frame->layer_renderers.push_back(
          std::unique_ptr<LayerRenderer>(renderer));
      gbm_handle *buffer_handle_ = renderer->GetNativeBoHandle();
      frame->layer_bos.push_back(buffer_handle_->bo);
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
      {"log", required_argument, NULL, 'l'},
      {"displaymode", required_argument, &display_mode, 1},
      {0},
  };

  char *endptr;
  int opt;
  int longindex = 0;
  FILE *fp;

  /* Suppress getopt's poor error messages */
  opterr = 0;

  while ((opt = getopt_long(argc, argv, "+:hf:j:l:", longopts,
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
      case 'l':
        if (strlen(optarg) >= 1024) {
          printf(
              "too long log file path, please provide less than 1024 "
              "characters!\n");
          exit(0);
        }
        printf("optarg:%s\n", optarg);
        strcpy(log_path, optarg);
        fp = freopen(log_path, "a", stderr);
        if (!fp) {
          printf("unable to open log file\n");
          fclose(fp);
          exit(EXIT_FAILURE);
        }
        fclose(fp);
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
  void *iahwc_dl_handle;
  iahwc_module_t *iahwc_module;
  iahwc_device_t *iahwc_device;
  int num_displays, i;
  uint num_configs;
  uint32_t *configs, preferred_config;
  int32_t kms_fence = -1;

  setup_tty();

  backend = new iahwc_backend;

  iahwc_dl_handle = dlopen("libhwcomposer.so", RTLD_NOW);
  if (!iahwc_dl_handle) {
    printf("Unable to open libhwcomposer.so: %s\n", dlerror());
    printf("aborting...\n");
    abort();
  }

  iahwc_module = (iahwc_module_t *)dlsym(iahwc_dl_handle, IAHWC_MODULE_STR);
  iahwc_module->open(iahwc_module, &iahwc_device);

  backend->iahwc_module = iahwc_module;
  backend->iahwc_device = iahwc_device;

  backend->iahwc_get_num_displays =
      (IAHWC_PFN_GET_NUM_DISPLAYS)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_GET_NUM_DISPLAYS);
  backend->iahwc_create_layer =
      (IAHWC_PFN_CREATE_LAYER)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_CREATE_LAYER);
  backend->iahwc_get_display_info =
      (IAHWC_PFN_DISPLAY_GET_INFO)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_INFO);
  backend->iahwc_get_display_configs =
      (IAHWC_PFN_DISPLAY_GET_CONFIGS)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_CONFIGS);
  backend->iahwc_get_display_name =
      (IAHWC_PFN_DISPLAY_GET_NAME)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_NAME);
  backend->iahwc_set_display_gamma =
      (IAHWC_PFN_DISPLAY_SET_GAMMA)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_SET_GAMMA);
  backend->iahwc_set_display_config =
      (IAHWC_PFN_DISPLAY_SET_CONFIG)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_SET_CONFIG);
  backend->iahwc_get_display_config =
      (IAHWC_PFN_DISPLAY_GET_CONFIG)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_DISPLAY_GET_CONFIG);
  backend->iahwc_present_display =
      (IAHWC_PFN_PRESENT_DISPLAY)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_PRESENT_DISPLAY);
  backend->iahwc_layer_set_bo =
      (IAHWC_PFN_LAYER_SET_BO)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_BO);
  backend->iahwc_layer_set_acquire_fence =
      (IAHWC_PFN_LAYER_SET_ACQUIRE_FENCE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_ACQUIRE_FENCE);
  backend->iahwc_layer_set_transform =
      (IAHWC_PFN_LAYER_SET_TRANSFORM)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_TRANSFORM);
  backend->iahwc_layer_set_source_crop =
      (IAHWC_PFN_LAYER_SET_SOURCE_CROP)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_SOURCE_CROP);
  backend->iahwc_layer_set_display_frame =
      (IAHWC_PFN_LAYER_SET_DISPLAY_FRAME)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_DISPLAY_FRAME);
  backend->iahwc_layer_set_surface_damage =
      (IAHWC_PFN_LAYER_SET_SURFACE_DAMAGE)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_LAYER_SET_SURFACE_DAMAGE);
  backend->iahwc_register_callback =
      (IAHWC_PFN_REGISTER_CALLBACK)iahwc_device->getFunctionPtr(
          iahwc_device, IAHWC_FUNC_REGISTER_CALLBACK);

  parse_args(argc, argv);

  fd = open("/dev/dri/renderD128", O_RDWR);
  if (fd == -1) {
    ETRACE("Can't open GPU file");
    exit(-1);
  }

  buffer_handler = hwcomposer::NativeBufferHandler::CreateInstance(fd);

  if (!buffer_handler)
    exit(-1);

  if (!init_gl()) {
    delete buffer_handler;
    exit(-1);
  }

  backend->iahwc_get_num_displays(iahwc_device, &num_displays);
  printf("Number of displays available is %d\n", num_displays);

  backend->iahwc_get_display_configs(iahwc_device, 0, &num_configs, NULL);
  printf("Number of configs %d\n", num_configs);
  configs = new uint32_t[num_configs];
  backend->iahwc_get_display_configs(iahwc_device, 0, &num_configs, configs);
  backend->iahwc_get_display_config(iahwc_device, 0, &preferred_config);

  printf("Preferred config is %d\n", preferred_config);

  for (i = 0; i < num_configs; i++) {
    int width, height, refresh_rate, dpix, dpiy;
    backend->iahwc_get_display_info(iahwc_device, 0, configs[i],
                                    IAHWC_CONFIG_WIDTH, &width);
    backend->iahwc_get_display_info(iahwc_device, 0, configs[i],
                                    IAHWC_CONFIG_HEIGHT, &height);
    backend->iahwc_get_display_info(iahwc_device, 0, configs[i],
                                    IAHWC_CONFIG_REFRESHRATE, &refresh_rate);
    backend->iahwc_get_display_info(iahwc_device, 0, configs[i],
                                    IAHWC_CONFIG_DPIX, &dpix);
    backend->iahwc_get_display_info(iahwc_device, 0, configs[i],
                                    IAHWC_CONFIG_DPIY, &dpiy);

    printf(
        "Config %d: width %d, height %d, refresh rate %d, dpix %d, dpiy %d\n",
        configs[i], width, height, refresh_rate, dpix, dpiy);

    if (configs[i] == preferred_config) {
      primary_width = width;
      primary_height = height;
    }
  }

  printf("Width of primary display is %d height of the primary display is %d\n",
         primary_width, primary_height);

  init_frames(primary_width, primary_height);

  int64_t gpu_fence_fd = -1; /* out-fence from gpu, in-fence to kms */
  uint32_t frame_total = 0;

  for (uint64_t i = 0; arg_frames == 0 || i < arg_frames; ++i) {
    struct frame *frame = &frames[i % ARRAY_SIZE(frames)];
    if (kms_fence != -1) {
      sync_wait(kms_fence, -1);
      close(kms_fence);
      kms_fence = -1;
    }

    for (uint32_t j = 0; j < frame->layers.size(); j++) {
      frame->layers_fences[j].clear();
      frame->layer_renderers[j]->Draw(&gpu_fence_fd);
      backend->iahwc_layer_set_acquire_fence(iahwc_device, 0, frame->layers[j],
                                             gpu_fence_fd);
      iahwc_region_t damage_region;
      damage_region.numRects = 1;
      iahwc_rect_t *rect = new iahwc_rect_t[1];
      rect[0] = {layer_parameter.frame_x, layer_parameter.frame_y,
                 layer_parameter.frame_width, layer_parameter.frame_height};
      damage_region.rects = rect;
      backend->iahwc_layer_set_surface_damage(iahwc_device, 0, frame->layers[j],
                                              damage_region);
      backend->iahwc_layer_set_bo(iahwc_device, 0, frame->layers[j],
                                  frame->layer_bos[j]);
    }

    backend->iahwc_present_display(iahwc_device, 0, &kms_fence);
    frame_total++;
  }

  reset_vt();
  return 0;
}
