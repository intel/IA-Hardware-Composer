#ifndef LAYER_FROM_JSON_H
#define LAYER_FROM_JSON_H

#include <vector>
#include <string>
#include <stdint.h>

typedef enum {

  LAYER_FORMAT_C8 = 0,  //('C', '8', ' ', ' ') /* [7:0] C */

  /* 8 bpp Red */
  LAYER_FORMAT_R8 = 1,  //('R', '8', ' ', ' ') /* [7:0] R */

  /* 16 bpp RG */
  LAYER_FORMAT_GR88 =
      2,  //('G', 'R', '8', '8') /* [15:0] G:R 8:8 little endian */

  /* 8 bpp RGB */
  LAYER_FORMAT_RGB332 = 3,  //('R', 'G', 'B', '8') /* [7:0] R:G:B 3:3:2 */
  LAYER_FORMAT_BGR233 = 4,  //('B', 'G', 'R', '8') /* [7:0] B:G:R 2:3:3 */

  /* 16 bpp RGB */
  LAYER_FORMAT_XRGB4444 =
      5,  //('X', 'R', '1', '2') /* [15:0] x:R:G:B 4:4:4:4 little endian */
  LAYER_FORMAT_XBGR4444 =
      6,  //('X', 'B', '1', '2') /* [15:0] x:B:G:R 4:4:4:4 little endian */
  LAYER_FORMAT_RGBX4444 =
      7,  //('R', 'X', '1', '2') /* [15:0] R:G:B:x 4:4:4:4 little endian */
  LAYER_FORMAT_BGRX4444 =
      8,  //('B', 'X', '1', '2') /* [15:0] B:G:R:x 4:4:4:4 little endian */

  LAYER_FORMAT_ARGB4444 =
      9,  //('A', 'R', '1', '2') /* [15:0] A:R:G:B 4:4:4:4 little endian */
  LAYER_FORMAT_ABGR4444 =
      10,  //('A', 'B', '1', '2') /* [15:0] A:B:G:R 4:4:4:4 little endian */
  LAYER_FORMAT_RGBA4444 =
      11,  //('R', 'A', '1', '2') /* [15:0] R:G:B:A 4:4:4:4 little endian */
  LAYER_FORMAT_BGRA4444 =
      12,  //('B', 'A', '1', '2') /* [15:0] B:G:R:A 4:4:4:4 little endian */

  LAYER_FORMAT_XRGB1555 =
      13,  //('X', 'R', '1', '5') /* [15:0] x:R:G:B 1:5:5:5 little endian */
  LAYER_FORMAT_XBGR1555 =
      14,  //('X', 'B', '1', '5') /* [15:0] x:B:G:R 1:5:5:5 little endian */
  LAYER_FORMAT_RGBX5551 =
      15,  //('R', 'X', '1', '5') /* [15:0] R:G:B:x 5:5:5:1 little endian */
  LAYER_FORMAT_BGRX5551 =
      16,  //('B', 'X', '1', '5') /* [15:0] B:G:R:x 5:5:5:1 little endian */

  LAYER_FORMAT_ARGB1555 =
      17,  //('A', 'R', '1', '5') /* [15:0] A:R:G:B 1:5:5:5 little endian */
  LAYER_FORMAT_ABGR1555 =
      18,  //('A', 'B', '1', '5') /* [15:0] A:B:G:R 1:5:5:5 little endian */
  LAYER_FORMAT_RGBA5551 =
      19,  //('R', 'A', '1', '5') /* [15:0] R:G:B:A 5:5:5:1 little endian */
  LAYER_FORMAT_BGRA5551 =
      20,  //('B', 'A', '1', '5') /* [15:0] B:G:R:A 5:5:5:1 little endian */

  LAYER_FORMAT_RGB565 =
      21,  //('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
  LAYER_FORMAT_BGR565 =
      22,  //('B', 'G', '1', '6') /* [15:0] B:G:R 5:6:5 little endian */

  LAYER_FORMAT_RGB888 =
      23,  //('R', 'G', '2', '4') /* [23:0] R:G:B little endian */
  LAYER_FORMAT_BGR888 =
      24,  //('B', 'G', '2', '4') /* [23:0] B:G:R little endian */

  /* 32 bpp RGB */
  LAYER_FORMAT_XRGB8888 =
      25,  //('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
  LAYER_FORMAT_XBGR8888 =
      26,  //('X', 'B', '2', '4') /* [31:0] x:B:G:R 8:8:8:8 little endian */
  LAYER_FORMAT_RGBX8888 =
      27,  //('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
  LAYER_FORMAT_BGRX8888 =
      28,  //('B', 'X', '2', '4') /* [31:0] B:G:R:x 8:8:8:8 little endian */

  LAYER_FORMAT_ARGB8888 =
      29,  //('A', 'R', '2', '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */
  LAYER_FORMAT_ABGR8888 =
      30,  //('A', 'B', '2', '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
  LAYER_FORMAT_RGBA8888 =
      31,  //('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
  LAYER_FORMAT_BGRA8888 =
      32,  //('B', 'A', '2', '4') /* [31:0] B:G:R:A 8:8:8:8 little endian */

  LAYER_FORMAT_XRGB2101010 =
      33,  //('X', 'R', '3', '0') /* [31:0] x:R:G:B 2:10:10:10 little endian */
  LAYER_FORMAT_XBGR2101010 =
      34,  //('X', 'B', '3', '0') /* [31:0] x:B:G:R 2:10:10:10 little endian */
  LAYER_FORMAT_RGBX1010102 =
      35,  //('R', 'X', '3', '0') /* [31:0] R:G:B:x 10:10:10:2 little endian */
  LAYER_FORMAT_BGRX1010102 =
      36,  //('B', 'X', '3', '0') /* [31:0] B:G:R:x 10:10:10:2 little endian */

  LAYER_FORMAT_ARGB2101010 =
      37,  //('A', 'R', '3', '0') /* [31:0] A:R:G:B 2:10:10:10 little endian */
  LAYER_FORMAT_ABGR2101010 =
      38,  //('A', 'B', '3', '0') /* [31:0] A:B:G:R 2:10:10:10 little endian */
  LAYER_FORMAT_RGBA1010102 =
      39,  //('R', 'A', '3', '0') /* [31:0] R:G:B:A 10:10:10:2 little endian */
  LAYER_FORMAT_BGRA1010102 =
      40,  //('B', 'A', '3', '0') /* [31:0] B:G:R:A 10:10:10:2 little endian */

  /* packed YCbCr */
  LAYER_FORMAT_YUYV = 41,  //('Y', 'U', 'Y', 'V') /* [31:0] Cr0:Y1:Cb0:Y0
  // 8:8:8:8 little endian */
  LAYER_FORMAT_YVYU = 42,  //('Y', 'V', 'Y', 'U') /* [31:0] Cb0:Y1:Cr0:Y0
  // 8:8:8:8 little endian */
  LAYER_FORMAT_UYVY = 43,  //('U', 'Y', 'V', 'Y') /* [31:0] Y1:Cr0:Y0:Cb0
  // 8:8:8:8 little endian */
  LAYER_FORMAT_VYUY = 44,  //('V', 'Y', 'U', 'Y') /* [31:0] Y1:Cb0:Y0:Cr0
  // 8:8:8:8 little endian */

  LAYER_FORMAT_AYUV =
      45,  //('A', 'Y', 'U', 'V') /* [31:0] A:Y:Cb:Cr 8:8:8:8 little endian */

  /*
   * 2 plane YCbCr
   * index 0 = Y plane, [7:0] Y
   * index 1 = Cr:Cb plane, [15:0] Cr:Cb little endian
   * or
   * index 1 = Cb:Cr plane, [15:0] Cb:Cr little endian
   */
  LAYER_FORMAT_NV12 =
      47,  //('N', 'V', '1', '2') /* 2x2 subsampled Cr:Cb plane */
  LAYER_FORMAT_NV21 =
      48,  //('N', 'V', '2', '1') /* 2x2 subsampled Cb:Cr plane */
  LAYER_FORMAT_NV16 =
      49,  //('N', 'V', '1', '6') /* 2x1 subsampled Cr:Cb plane */
  LAYER_FORMAT_NV61 =
      50,  //('N', 'V', '6', '1') /* 2x1 subsampled Cb:Cr plane */

  /*
   * 3 plane YCbCr
   * index 0: Y plane, [7:0] Y
   * index 1: Cb plane, [7:0] Cb
   * index 2: Cr plane, [7:0] Cr
   * or
   * index 1: Cr plane, [7:0] Cr
   * index 2: Cb plane, [7:0] Cb
   */
  LAYER_FORMAT_YUV410 =
      51,  //('Y', 'U', 'V', '9') /* 4x4 subsampled Cb (1) and Cr (2) planes */
  LAYER_FORMAT_YVU410 =
      52,  //('Y', 'V', 'U', '9') /* 4x4 subsampled Cr (1) and Cb (2) planes */
  LAYER_FORMAT_YUV411 =
      53,  //('Y', 'U', '1', '1') /* 4x1 subsampled Cb (1) and Cr (2) planes */
  LAYER_FORMAT_YVU411 =
      54,  //('Y', 'V', '1', '1') /* 4x1 subsampled Cr (1) and Cb (2) planes */
  LAYER_FORMAT_YUV420 =
      55,  //('Y', 'U', '1', '2') /* 2x2 subsampled Cb (1) and Cr (2) planes */
  LAYER_FORMAT_YVU420 =
      56,  //('Y', 'V', '1', '2') /* 2x2 subsampled Cr (1) and Cb (2) planes */
  LAYER_FORMAT_YUV422 =
      57,  //('Y', 'U', '1', '6') /* 2x1 subsampled Cb (1) and Cr (2) planes */
  LAYER_FORMAT_YVU422 =
      58,  //('Y', 'V', '1', '6') /* 2x1 subsampled Cr (1) and Cb (2) planes */
  LAYER_FORMAT_YUV444 =
      59,  //('Y', 'U', '2', '4') /* non-subsampled Cb (1) and Cr (2) planes */
  LAYER_FORMAT_YVU444 =
      60,  // ('Y', 'V', '2', '4') /* non-subsampled Cr (1) and Cb (2) planes */

  LAYER_FORMAT_UNDEFINED

} LAYER_FORMAT;

typedef enum {
  LAYER_TYPE_GL = 0,
  LAYER_TYPE_VIDEO = 1,
  LAYER_TYPE_IMAGE = 2,
  LAYER_TYPE_UNDEFINED
} LAYER_TYPE;

typedef enum {
  LAYER_REFLECT_X = 0,
  LAYER_REFLECT_Y = 1,
  LAYER_ROTATE_90 = 2,
  LAYER_ROTATE_180 = 3,
  LAYER_ROTATE_270 = 4,
  LAYER_TRANSFORM_UNDEFINED
} LAYER_TRANSFORM;

typedef struct {
  LAYER_TYPE type;
  LAYER_FORMAT format;
  LAYER_TRANSFORM transform;
  std::string resource_path;
  uint32_t source_width;
  uint32_t source_height;
  uint32_t source_crop_x;
  uint32_t source_crop_y;
  uint32_t source_crop_width;
  uint32_t source_crop_height;
  uint32_t frame_x;
  uint32_t frame_y;
  uint32_t frame_width;
  uint32_t frame_height;
} LAYER_PARAMETER;

typedef std::vector<LAYER_PARAMETER> LAYER_PARAMETERS;

bool parseLayersFromJson(const char* json_path, LAYER_PARAMETERS& parameters);

#endif
