#ifndef OS_ALIOS_PLATFORMDEFINES_H_
#define OS_ALIOS_PLATFORMDEFINES_H_

#include <hwcmeta.h>
#include <log/Log.h>
#include <string.h>
#include <cstdint>

#include <cutils/native_target.h>
#include "platformcommondefines.h"

#define LOG_TAG "IAHWF"

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

// _cplusplus
#ifdef _cplusplus
extern "C" {
#endif

/************ FROM MESA 17+ *************/
#define fourcc_code(a, b, c, d) \
  ((__u32)(a) | ((__u32)(b) << 8) | ((__u32)(c) << 16) | ((__u32)(d) << 24))

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [10:6] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [10:6:10:6] little endian
 */
/* 2x2 subsampled Cr:Cb plane 10 bits per channel */
#define DRM_FORMAT_P010 fourcc_code('P', '0', '1', '0')

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y:x [12:4] little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:x:Cb:x [12:4:12:4] little endian
 */
/* 2x2 subsampled Cr:Cb plane 12 bits per channel */
#define DRM_FORMAT_P012 fourcc_code('P', '0', '1', '2')

/*
 * 2 plane YCbCr MSB aligned
 * index 0 = Y plane, [15:0] Y little endian
 * index 1 = Cr:Cb plane, [31:0] Cr:Cb [16:16] little endian
 */
/* 2x2 subsampled Cr:Cb plane 16 bits per channel */
#define DRM_FORMAT_P016 fourcc_code('P', '0', '1', '6')

/* 16 bpp Red */
/* [15:0] R little endian */
#define DRM_FORMAT_R16 fourcc_code('R', '1', '6', ' ')

#define DRM_MODE_ROTATE_0 (1 << 0)
#define DRM_MODE_ROTATE_90 (1 << 1)
#define DRM_MODE_ROTATE_180 (1 << 2)
#define DRM_MODE_ROTATE_270 (1 << 3)

#define DRM_MODE_REFLECT_X (1 << 4)
#define DRM_MODE_REFLECT_Y (1 << 5)

/*************************/

struct yalloc_handle {
  gb_target_t target_;
  native_target_t* imported_target_;
  bool hwc_buffer_ = false;
  HwcMeta meta_data_;
  void* pixel_memory_ = NULL;
};

typedef struct yalloc_handle* HWCNativeHandle;
typedef struct native_target HWCNativeBuffer;

#define GETNATIVEBUFFER(handle) (*(native_target_t*)(handle->target_))

// define for rework.
#define VTRACE(fmt, ...) \
  LOG_V("%s:%d: %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define DTRACE(fmt, ...) \
  LOG_D("%s:%d: %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define ETRACE(fmt, ...) \
  LOG_E("%s:%d: %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define WTRACE(fmt, ...) \
  LOG_W("%s:%d: %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#define ITRACE(fmt, ...) \
  LOG_I("%s:%d: %s " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define STRACE()

inline uint32_t GetNativeBuffer(uint32_t gpu_fd, HWCNativeHandle handle) {
  uint32_t id = 0;
  uint32_t prime_fd = handle->target_->fds.data[0];
  if (drmPrimeFDToHandle(gpu_fd, prime_fd, &id)) {
    ETRACE("Error generate handle from prime fd %d", prime_fd);
  }
  return id;
}

// _cplusplus
#ifdef _cplusplus
}
#endif

#endif
