/*
 * Copyright (c) 2017 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OS_LINUX_IAHWC_H_
#define OS_LINUX_IAHWC_H_

#include <gbm.h>
#include <stdint.h>
#include <hwcdefs.h>

#define IAHWC_MODULE IAHWC_MODULE_INFO
#define IAHWC_MODULE_STR "IAHWC_MODULE_INFO"

typedef void (*iahwc_function_ptr_t)();
typedef uint32_t iahwc_display_t;
typedef uint32_t iahwc_layer_t;
typedef void* iahwc_callback_data_t;

struct iahwc_device;

typedef struct iahwc_module {
  const char* name;
  int (*open)(const struct iahwc_module* module, struct iahwc_device** device);
} iahwc_module_t;

typedef struct iahwc_device {
  struct iahwc_module module;
  int (*close)(struct iahwc_device* device);
  iahwc_function_ptr_t (*getFunctionPtr)(struct iahwc_device* device,
                                         int descriptor);
} iahwc_device_t;

typedef struct iahwc_raw_pixel_data {
  void* buffer;
  void* callback_data;
  uint64_t width;
  uint64_t height;
  uint64_t stride;
  uint32_t format;
} iahwc_raw_pixel_data;

typedef enum {
  IAHWC_DISPLAY_STATUS_CONNECTED,
  IAHWC_DISPLAY_STATUS_DISCONNECTED
} iahwc_hotplug_status;

typedef enum {
  IAHWC_ERROR_NONE = 0,
  IAHWC_ERROR_BAD_CONFIG,
  IAHWC_ERROR_BAD_DISPLAY,
  IAHWC_ERROR_BAD_LAYER,
  IAHWC_ERROR_BAD_PARAMETER,
  IAHWC_ERROR_HAS_CHANGES,
  IAHWC_ERROR_NO_RESOURCES,
  IAHWC_ERROR_NOT_VALIDATED,
  IAHWC_ERROR_UNSUPPORTED,
} iahwc_error_t;

enum iahwc_display_configs {
  IAHWC_CONFIG_WIDTH = 1,
  IAHWC_CONFIG_HEIGHT = 2,
  IAHWC_CONFIG_REFRESHRATE = 3,
  IAHWC_CONFIG_DPIX = 4,
  IAHWC_CONFIG_DPIY = 5
};

enum iahwc_function_descriptors {
  IAHWC_FUNC_INVALID = 0,
  IAHWC_FUNC_GET_NUM_DISPLAYS,
  IAHWC_FUNC_REGISTER_CALLBACK,
  IAHWC_FUNC_DISPLAY_GET_CONNECTION_STATUS,
  IAHWC_FUNC_DISPLAY_GET_INFO,
  IAHWC_FUNC_DISPLAY_GET_NAME,
  IAHWC_FUNC_DISPLAY_GET_CONFIGS,
  IAHWC_FUNC_DISPLAY_SET_GAMMA,
  IAHWC_FUNC_DISPLAY_SET_CONFIG,
  IAHWC_FUNC_DISPLAY_GET_CONFIG,
  IAHWC_FUNC_DISPLAY_SET_POWER_MODE,
  IAHWC_FUNC_DISPLAY_CLEAR_ALL_LAYERS,
  IAHWC_FUNC_PRESENT_DISPLAY,
  IAHWC_FUNC_DISABLE_OVERLAY_USAGE,
  IAHWC_FUNC_ENABLE_OVERLAY_USAGE,
  IAHWC_FUNC_CREATE_LAYER,
  IAHWC_FUNC_DESTROY_LAYER,
  IAHWC_FUNC_LAYER_SET_BO,
  IAHWC_FUNC_LAYER_SET_RAW_PIXEL_DATA,
  IAHWC_FUNC_LAYER_SET_ACQUIRE_FENCE,
  IAHWC_FUNC_LAYER_SET_USAGE,
  IAHWC_FUNC_LAYER_SET_TRANSFORM,
  IAHWC_FUNC_LAYER_SET_SOURCE_CROP,
  IAHWC_FUNC_LAYER_SET_DISPLAY_FRAME,
  IAHWC_FUNC_LAYER_SET_SURFACE_DAMAGE,
  IAHWC_FUNC_LAYER_SET_PLANE_ALPHA,
  IAHWC_FUNC_LAYER_SET_INDEX,
};

enum iahwc_callback_descriptor {
  IAHWC_CALLBACK_VSYNC,
  IAHWC_CALLBACK_PIXEL_UPLOADER,
  IAHWC_CALLBACK_HOTPLUG
};

enum iahwc_layer_usage {
  IAHWC_LAYER_USAGE_CURSOR,
  IAHWC_LAYER_USAGE_OVERLAY,
  IAHWC_LAYER_USAGE_NORMAL,
};

enum iahwc_layer_transform {
  IAHWC_TRANSFORM_FLIP_H,
  IAHWC_TRANSFORM_FLIP_V,
  IAHWC_TRANSFORM_ROT_90,
  IAHWC_TRANSFORM_ROT_180,
  IAHWC_TRANSFORM_ROT_270,
  IAHWC_TRANSFORM_FLIP_H_ROT_90,
  IAHWC_TRANSFORM_FLIP_V_ROT_90
};

typedef struct iahwc_rect {
  uint32_t left;
  uint32_t top;
  uint32_t right;
  uint32_t bottom;
} iahwc_rect_t;

typedef struct iahwc_region {
  size_t numRects;
  iahwc_rect_t const* rects;
} iahwc_region_t;

typedef int (*IAHWC_PFN_GET_NUM_DISPLAYS)(iahwc_device_t*, int* num_displays);
typedef int (*IAHWC_PFN_REGISTER_CALLBACK)(iahwc_device_t*, int descriptor,
                                           iahwc_display_t display_handle,
                                           iahwc_callback_data_t data,
                                           iahwc_function_ptr_t hook);
typedef int (*IAHWC_PFN_DISPLAY_GET_CONNECTION_STATUS)(
    iahwc_device_t*, iahwc_display_t display_handle, int32_t* status);
typedef int (*IAHWC_PFN_DISPLAY_GET_INFO)(iahwc_device_t*,
                                          iahwc_display_t display_handle,
                                          uint32_t config, int attribute,
                                          int32_t* value);
typedef int (*IAHWC_PFN_DISPLAY_GET_NAME)(iahwc_device_t*,
                                          iahwc_display_t display_handle,
                                          uint32_t* size, char* name);
typedef int (*IAHWC_PFN_DISPLAY_GET_CONFIGS)(iahwc_device_t*,
                                             iahwc_display_t display_handle,
                                             uint32_t* num_configs,
                                             uint32_t* configs);
typedef int (*IAHWC_PFN_DISPLAY_SET_GAMMA)(iahwc_device_t*,
                                           iahwc_display_t display_handle,
                                           float r, float g, float b);
typedef int (*IAHWC_PFN_DISPLAY_SET_CONFIG)(iahwc_device_t*,
                                            iahwc_display_t display_handle,
                                            uint32_t config);
typedef int (*IAHWC_PFN_DISPLAY_GET_CONFIG)(iahwc_device_t*,
                                            iahwc_display_t display_handle,
                                            uint32_t* config);
typedef int (*IAHWC_PFN_DISPLAY_SET_POWER_MODE)(iahwc_device_t*,
                                                iahwc_display_t display_handle,
                                                uint32_t power_mode);
typedef int (*IAHWC_PFN_DISPLAY_CLEAR_ALL_LAYERS)(
    iahwc_device_t*, iahwc_display_t display_handle);
typedef int (*IAHWC_PFN_PRESENT_DISPLAY)(iahwc_device_t*,
                                         iahwc_display_t display_handle,
                                         int32_t* release_fd);
typedef int (*IAHWC_PFN_DISABLE_OVERLAY_USAGE)(iahwc_device_t*,
                                               iahwc_display_t display_handle);
typedef int (*IAHWC_PFN_ENABLE_OVERLAY_USAGE)(iahwc_device_t*,
                                              iahwc_display_t display_handle);
typedef int (*IAHWC_PFN_CREATE_LAYER)(iahwc_device_t*,
                                      iahwc_display_t display_handle,
                                      iahwc_layer_t* layer_handle);
typedef int (*IAHWC_PFN_DESTROY_LAYER)(iahwc_device_t*,
                                       iahwc_display_t display_handle,
                                       iahwc_layer_t layer_handle);
typedef int (*IAHWC_PFN_LAYER_SET_BO)(iahwc_device_t*,
                                      iahwc_display_t display_handle,
                                      iahwc_layer_t layer_handle,
                                      struct gbm_bo*);
typedef int (*IAHWC_PFN_LAYER_SET_RAW_PIXEL_DATA)(
    iahwc_device_t*, iahwc_display_t display_handle, iahwc_layer_t layer_handle,
    struct iahwc_raw_pixel_data);
typedef int (*IAHWC_PFN_LAYER_SET_ACQUIRE_FENCE)(iahwc_device_t*,
                                                 iahwc_display_t display_handle,
                                                 iahwc_layer_t layer_handle,
                                                 int32_t acquire_fence);
typedef int (*IAHWC_PFN_LAYER_SET_USAGE)(iahwc_device_t*,
                                         iahwc_display_t display_handle,
                                         iahwc_layer_t layer_handle,
                                         int32_t layer_usage);
typedef int (*IAHWC_PFN_LAYER_SET_TRANSFORM)(iahwc_device_t*,
                                             iahwc_display_t display_handle,
                                             iahwc_layer_t layer_handle,
                                             int32_t layer_transform);
typedef int (*IAHWC_PFN_LAYER_SET_SOURCE_CROP)(iahwc_device_t*,
                                               iahwc_display_t display_handle,
                                               iahwc_layer_t layer_handle,
                                               iahwc_rect_t rect);
typedef int (*IAHWC_PFN_LAYER_SET_DISPLAY_FRAME)(iahwc_device_t*,
                                                 iahwc_display_t display_handle,
                                                 iahwc_layer_t layer_handle,
                                                 iahwc_rect_t rect);
typedef int (*IAHWC_PFN_LAYER_SET_SURFACE_DAMAGE)(
    iahwc_device_t*, iahwc_display_t display_handle, iahwc_layer_t layer_handle,
    iahwc_region_t region);
typedef int (*IAHWC_PFN_LAYER_SET_PLANE_ALPHA)(iahwc_device_t*,
                                               iahwc_display_t display_handle,
                                               iahwc_layer_t layer_handle,
                                               float alpha);
typedef int (*IAHWC_PFN_LAYER_SET_INDEX)(iahwc_device_t*,
                                         iahwc_display_t display_handle,
                                         iahwc_layer_t layer_handle,
                                         uint32_t layer_index);
typedef int (*IAHWC_PFN_VSYNC)(iahwc_callback_data_t data,
                               iahwc_display_t display, int64_t timestamp);
typedef int (*IAHWC_PFN_PIXEL_UPLOADER)(iahwc_callback_data_t data,
                                        iahwc_display_t display,
                                        uint32_t start_access,
                                        void* call_back_data);
typedef int (*IAHWC_PFN_HOTPLUG)(iahwc_callback_data_t data,
                                 iahwc_display_t display, uint32_t status);
#endif  // OS_LINUX_IAHWC_H_
