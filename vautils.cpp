/*
// Copyright (c) 2017 Intel Corporation
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

#include "vautils.h"

#include <va/va.h>

#include <drm_fourcc.h>
#include <log/log.h>
//#include <hwctrace.h>
#include <system/graphics.h>


//#include "platformdefines.h"

namespace android {

enum { HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL = 0x100,
       HAL_PIXEL_FORMAT_NV12_LINEAR_INTEL = 0x101,
       HAL_PIXEL_FORMAT_YCrCb_422_H_INTEL = 0x102,
       HAL_PIXEL_FORMAT_NV12_LINEAR_PACKED_INTEL = 0x103,
       HAL_PIXEL_FORMAT_YCbCr_422_H_INTEL = 0x104,
       HAL_PIXEL_FORMAT_NV12_X_TILED_INTEL = 0x105,
       HAL_PIXEL_FORMAT_RGBA_5551_INTEL = 0x106,
       HAL_PIXEL_FORMAT_RGBA_4444_INTEL = 0x107,
       HAL_PIXEL_FORMAT_GENERIC_8BIT_INTEL = 0x108,
       HAL_PIXEL_FORMAT_YCbCr_411_INTEL = 0x109,
       HAL_PIXEL_FORMAT_YCbCr_420_H_INTEL = 0x10A,
       HAL_PIXEL_FORMAT_YCbCr_422_V_INTEL = 0x10B,
       HAL_PIXEL_FORMAT_YCbCr_444_INTEL = 0x10C,
       HAL_PIXEL_FORMAT_RGBP_INTEL = 0x10D,
       HAL_PIXEL_FORMAT_BGRP_INTEL = 0x10E,
       HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL = 0x10F,
       HAL_PIXEL_FORMAT_P010_INTEL = 0x110,
       HAL_PIXEL_FORMAT_Z16_INTEL = 0x111,
       HAL_PIXEL_FORMAT_UVMAP64_INTEL = 0x112,
       HAL_PIXEL_FORMAT_A2R10G10B10_INTEL = 0x113,
       HAL_PIXEL_FORMAT_A2B10G10R10_INTEL = 0x114,
       HAL_PIXEL_FORMAT_YCrCb_NORMAL_INTEL = 0x115,
       HAL_PIXEL_FORMAT_YCrCb_SWAPUVY_INTEL = 0x116,
       HAL_PIXEL_FORMAT_YCrCb_SWAPUV_INTEL = 0x117,
       HAL_PIXEL_FORMAT_YCrCb_SWAPY_INTEL = 0x118,
       HAL_PIXEL_FORMAT_X2R10G10B10_INTEL = 0x119,
       HAL_PIXEL_FORMAT_X2B10G10R10_INTEL = 0x11A,
       HAL_PIXEL_FORMAT_P016_INTEL = 0x11C,
       HAL_PIXEL_FORMAT_Y210_INTEL = 0x11D,
       HAL_PIXEL_FORMAT_Y216_INTEL = 0x11E,
       HAL_PIXEL_FORMAT_Y410_INTEL = 0x11F,
       HAL_PIXEL_FORMAT_Y416_INTEL = 0x120,
       HAL_PIXEL_FORMAT_Y8I_INTEL = 0x121,
       HAL_PIXEL_FORMAT_Y12I_INTEL = 0x122,
       HAL_PIXEL_FORMAT_YUYV_INTEL = HAL_PIXEL_FORMAT_YCrCb_NORMAL_INTEL,
       HAL_PIXEL_FORMAT_YUY2_INTEL = HAL_PIXEL_FORMAT_YCrCb_NORMAL_INTEL,
       HAL_PIXEL_FORMAT_VYUY_INTEL = HAL_PIXEL_FORMAT_YCrCb_SWAPUVY_INTEL,
       HAL_PIXEL_FORMAT_YVYU_INTEL = HAL_PIXEL_FORMAT_YCrCb_SWAPUV_INTEL,
       HAL_PIXEL_FORMAT_UYVY_INTEL = HAL_PIXEL_FORMAT_YCrCb_SWAPY_INTEL,
       HAL_PIXEL_FORMAT_NV12_TILED_INTEL = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
       HAL_PIXEL_FORMAT_NV12_INTEL = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
       HAL_PIXEL_FORMAT_INTEL_NV12 = HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
       HAL_PIXEL_FORMAT_NV12 = 0x10F,
       HAL_PIXEL_FORMAT_YUV420PackedSemiPlanar_INTEL = 0x7FA00E00,
       HAL_PIXEL_FORMAT_YUV420PackedSemiPlanar_Tiled_INTEL = 0x7FA00F00,
};
 

int DrmFormatToVAFormat(int format) {
    ALOGE("############## in vautils.cpp %s,line %d format=0x%x\n",__FUNCTION__,__LINE__,format);
  switch (format) {
    case DRM_FORMAT_NV12:
      return VA_FOURCC_NV12;
    case DRM_FORMAT_YVU420:
      return VA_FOURCC_YV12;
    case DRM_FORMAT_YUV420:
      return VA_FOURCC('I', '4', '2', '0');
    case DRM_FORMAT_YUV422:
      return VA_FOURCC_YUY2;
    case DRM_FORMAT_UYVY:
      return VA_FOURCC_UYVY;
    case DRM_FORMAT_YUYV:
      return VA_FOURCC_YUY2;
    case DRM_FORMAT_P010:
      return VA_FOURCC_P010;
    case DRM_FORMAT_ABGR8888:
      return VA_FOURCC_RGBA;
    case DRM_FORMAT_XBGR8888:
      return VA_FOURCC_RGBX;
    case DRM_FORMAT_RGBA8888:
        return VA_FOURCC_BGRA;
    case DRM_FORMAT_ARGB8888:
      return VA_FOURCC_ABGR;
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_YUV444:
    case DRM_FORMAT_AYUV:
    default:
      ALOGE("Unable to convert to VAFormat from format %x", format);
      break;
  }
  return 0;
}

 bool IsSupportedMediaFormat(uint32_t format) {
     ALOGE("#################### in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
  switch (format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_P010:
	case DRM_FORMAT_YVU420:
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_AYUV:
	case DRM_FORMAT_NV12_Y_TILED_INTEL:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YVU420_ANDROID:
        ALOGE("#################### in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
	  return true;
	default:
	  break;
  }
  ALOGE("#################### in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
  return false;
}


int DrmFormatToRTFormat(int format) {
 /*   ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_NV12);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_YVU420);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_YUV420);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_UYVY);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_YUYV);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_YVYU);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_VYUY);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_YUV422);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_YUV444);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_P010);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_ABGR8888);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_XBGR8888);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_ARGB8888);
    ALOGE(" in vautils.cpp %s,line %d,DRM_FORMAT_NV12=%d\n",__FUNCTION__,__LINE__,DRM_FORMAT_RGBA8888);*/
  switch (format) {
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_YUV420:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_YUYV:
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_VYUY:
    ALOGE(" in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
        return VA_RT_FORMAT_YUV420;
    case DRM_FORMAT_YUV422:
      return VA_RT_FORMAT_YUV422;
    case DRM_FORMAT_YUV444:
      return VA_RT_FORMAT_YUV444;
    case DRM_FORMAT_P010:
      return VA_RT_FORMAT_YUV420_10BPP;
    case DRM_FORMAT_ABGR8888:
        ALOGE(" in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
      return VA_RT_FORMAT_RGB32;
    case DRM_FORMAT_XBGR8888:
        ALOGE(" in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
      return VA_RT_FORMAT_RGB32;
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_RGBA8888:
        ALOGE(" in vautils.cpp %s,line %d\n",__FUNCTION__,__LINE__);
      return VA_RT_FORMAT_RGB32;
    default:
      ALOGE("Unable to convert to RTFormat from format %x", format);
      break;
  }
  return 0;
}
 int DrmFormatToHALFormat(int format) {
  switch (format) {
    case DRM_FORMAT_BGRA8888:
      return HAL_PIXEL_FORMAT_RGBA_8888;
    case DRM_FORMAT_BGRX8888:
      return HAL_PIXEL_FORMAT_RGBX_8888;
    case DRM_FORMAT_BGR888:
      return HAL_PIXEL_FORMAT_RGB_888;
    case DRM_FORMAT_BGR565:
      return HAL_PIXEL_FORMAT_RGB_565;
    case DRM_FORMAT_ARGB8888:
      return HAL_PIXEL_FORMAT_BGRA_8888;
    case DRM_FORMAT_YVU420:
      return HAL_PIXEL_FORMAT_YV12;
    case DRM_FORMAT_R8:
      return HAL_PIXEL_FORMAT_BLOB;
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_R16:
      return HAL_PIXEL_FORMAT_Y16;
    case DRM_FORMAT_ABGR8888:
      return HAL_PIXEL_FORMAT_RGBA_8888;
    case DRM_FORMAT_RGB332:  //('R', 'G', 'B', '8') /* [7:0] R:G:B 3:3:2 */
      return 0;
    case DRM_FORMAT_BGR233:  //('B', 'G', 'R', '8') /* [7:0] B:G:R 2:3:3 */
      return 0;

    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:
      return 0;
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:
      return 0;
    case DRM_FORMAT_RGB565:
      return HAL_PIXEL_FORMAT_RGB_565;
    case DRM_FORMAT_RGB888:
      return HAL_PIXEL_FORMAT_RGB_888;
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_RGBX8888:
    case DRM_FORMAT_RGBA8888:
        ALOGE("in vatuils.cpp %s,line %d \n",__FUNCTION__,__LINE__);
      return 0;
    case DRM_FORMAT_ABGR2101010:
      return HAL_PIXEL_FORMAT_RGBA_1010102;
    case DRM_FORMAT_XRGB2101010:
    case DRM_FORMAT_XBGR2101010:
    case DRM_FORMAT_RGBX1010102:
    case DRM_FORMAT_BGRX1010102:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_RGBA1010102:
    case DRM_FORMAT_BGRA1010102:
      return 0;
    case DRM_FORMAT_YUYV:
      return HAL_PIXEL_FORMAT_YCbCr_422_I;
    case DRM_FORMAT_YVYU:
    case DRM_FORMAT_UYVY:
    case DRM_FORMAT_VYUY:
    case DRM_FORMAT_AYUV:
      ALOGE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_NV12:
      return HAL_PIXEL_FORMAT_NV12;
    case DRM_FORMAT_NV21:
      return HAL_PIXEL_FORMAT_YCrCb_420_SP;
    case DRM_FORMAT_NV16:
      return HAL_PIXEL_FORMAT_YCbCr_422_SP;
    case DRM_FORMAT_NV61:
    case DRM_FORMAT_YUV410:
    case DRM_FORMAT_YVU410:
    case DRM_FORMAT_YUV411:
    case DRM_FORMAT_YVU411:
      ALOGE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_YUV420:
      return HAL_PIXEL_FORMAT_YCbCr_420_888;
    case DRM_FORMAT_YVU420_ANDROID:
      return HAL_PIXEL_FORMAT_YV12;
    case DRM_FORMAT_YUV422:
      return HAL_PIXEL_FORMAT_YCbCr_422_888;
    case DRM_FORMAT_YVU422:
      ALOGE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_YUV444:
      return HAL_PIXEL_FORMAT_YCbCr_444_888;
    case DRM_FORMAT_YVU444:
      ALOGE("YUV format using RGB buffer \n");
      return 0;
    case DRM_FORMAT_NV12_Y_TILED_INTEL:
      return HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL;
    case DRM_FORMAT_P010:
      return HAL_PIXEL_FORMAT_P010_INTEL;
   // case DRM_FORMAT_XBGR161616:
   //   return HAL_PIXEL_FORMAT_RGBA_FP16;
    default:
      return 0;
      break;
  }

  return DRM_FORMAT_NONE;
}

}  // namespace hwcomposer
