/*
// Copyright (c) 2018 Intel Corporation
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

#ifndef WESTON_COMPOSITOR_IAHWC_H
#define WESTON_COMPOSITOR_IAHWC_H

#include "compositor.h"
#include "plugin-registry.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WESTON_IAHWC_BACKEND_CONFIG_VERSION 1

struct libinput_device;

enum weston_iahwc_backend_output_mode {
  /** The output is disabled */
  WESTON_IAHWC_BACKEND_OUTPUT_OFF,
  /** The output will use the current active mode */
  WESTON_IAHWC_BACKEND_OUTPUT_CURRENT,
  /** The output will use the preferred mode. A modeline can be provided
   * by setting weston_backend_output_config::modeline in the form of
   * "WIDTHxHEIGHT" or in the form of an explicit modeline calculated
   * using e.g. the cvt tool. If a valid modeline is supplied it will be
   * used, if invalid or NULL the preferred available mode will be used. */
  WESTON_IAHWC_BACKEND_OUTPUT_PREFERRED,
};

#define WESTON_IAHWC_OUTPUT_API_NAME "weston_iahwc_output_api_v1"

struct weston_iahwc_output_api {
  /** The mode to be used by the output. Refer to the documentation
   *  of WESTON_IAHWC_BACKEND_OUTPUT_PREFERRED for details.
   *
   * Returns 0 on success, -1 on failure.
   */
  int (*set_mode)(struct weston_output *output,
                  enum weston_iahwc_backend_output_mode mode,
                  const char *modeline);

  /** The pixel format to be used by the output. Valid values are:
   * - NULL - The format set at backend creation time will be used;
   * - "xrgb8888";
   * - "rgb565"
   * - "xrgb2101010"
   */
  void (*set_gbm_format)(struct weston_output *output, const char *gbm_format);

  /** The seat to be used by the output. Set to NULL to use the
   *  default seat.
   */
  void (*set_seat)(struct weston_output *output, const char *seat);
};

static inline const struct weston_iahwc_output_api *weston_iahwc_output_get_api(
    struct weston_compositor *compositor) {
  const void *api;
  api = weston_plugin_api_get(compositor, WESTON_IAHWC_OUTPUT_API_NAME,
                              sizeof(struct weston_iahwc_output_api));

  return (const struct weston_iahwc_output_api *)api;
}

/** The backend configuration struct.
 *
 * weston_iahwc_backend_config contains the configuration used by a IAHWC
 * backend.
 */
struct weston_iahwc_backend_config {
  struct weston_backend_config base;

  /** The tty to be used. Set to 0 to use the current tty. */
  int tty;

  /** Whether to use the pixman renderer instead of the OpenGL ES renderer. */
  bool use_pixman;

  /** The seat to be used for input and output.
   *
   * If NULL the default "seat0" will be used.  The backend will
   * take ownership of the seat_id pointer and will free it on
   * backend destruction.
   */
  char *seat_id;

  /** The pixel format of the framebuffer to be used.
   *
   * Valid values are:
   * - NULL - The default format ("xrgb8888") will be used;
   * - "xrgb8888";
   * - "rgb565"
   * - "xrgb2101010"
   * The backend will take ownership of the format pointer and will free
   * it on backend destruction.
   */
  char *gbm_format;

  /** Callback used to configure input devices.
   *
   * This function will be called by the backend when a new input device
   * needs to be configured.
   * If NULL the device will use the default configuration.
   */
  void (*configure_device)(struct weston_compositor *compositor,
                           struct libinput_device *device);

  /** Maximum duration for a pageflip event to arrive, after which the
   * compositor will consider the IAHWC driver crashed and will try to exit
   * cleanly.
   *
   * It is exprimed in milliseconds, 0 means disabled. */
  uint32_t pageflip_timeout;
};

#ifdef __cplusplus
}
#endif

#endif /* WESTON_COMPOSITOR_IAHWC_H */
