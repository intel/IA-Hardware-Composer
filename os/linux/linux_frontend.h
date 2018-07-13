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

#ifndef LINUX_FRONTEND_H_
#define LINUX_FRONTEND_H_

#include <gpudevice.h>
#include <hwcdefs.h>
#include <hwclayer.h>
#include <map>
#include <type_traits>
#include <vector>
#include "iahwc.h"
#include "pixeluploader.h"
#include "spinlock.h"

namespace hwcomposer {

class NativeBufferHandler;
class PixelUploader;

class IAHWC : public iahwc_device {
 public:
  IAHWC();
  int32_t Init();

  class IAHWCLayer : public PixelUploaderLayerCallback {
   public:
    IAHWCLayer(PixelUploader* uploader);
    ~IAHWCLayer() override;
    int SetBo(gbm_bo* bo);
    int SetRawPixelData(iahwc_raw_pixel_data bo);
    int SetAcquireFence(int32_t acquire_fence);
    int SetLayerUsage(int32_t layer_usage);
    int32_t GetLayerUsage() {
      return layer_usage_;
    }
    int SetLayerTransform(int32_t layer_transform);
    int SetLayerSourceCrop(iahwc_rect_t rect);
    int SetLayerDisplayFrame(iahwc_rect_t rect);
    int SetLayerSurfaceDamage(iahwc_region_t region);
    int SetLayerPlaneAlpha(float alpha);
    int SetLayerIndex(uint32_t layer_index);
    uint32_t GetLayerIndex() {
      return layer_index_;
    }
    hwcomposer::HwcLayer* GetLayer();

    void UploadDone() override;

   private:
    void ClosePrimeHandles();
    hwcomposer::HwcLayer iahwc_layer_;
    struct gbm_handle hwc_handle_;
    HWCNativeHandle pixel_buffer_ = NULL;
    uint32_t orig_width_ = 0;
    uint32_t orig_height_ = 0;
    uint32_t orig_stride_ = 0;
    PixelUploader* raw_data_uploader_ = NULL;
    int32_t layer_usage_;
    uint32_t layer_index_;
    bool upload_in_progress_ = false;
  };

  class IAHWCDisplay : public PixelUploaderCallback {
   public:
    IAHWCDisplay();
    ~IAHWCDisplay();
    int Init(hwcomposer::NativeDisplay* display, uint32_t gpu_fd);
    int GetConnectionStatus(int32_t* status);
    int GetDisplayInfo(uint32_t config, int attribute, int32_t* value);
    int GetDisplayName(uint32_t* size, char* name);
    int GetDisplayConfigs(uint32_t* num_configs, uint32_t* configs);
    int SetDisplayGamma(float r, float b, float g);
    int SetDisplayConfig(uint32_t config);
    int GetDisplayConfig(uint32_t* config);
    int SetPowerMode(uint32_t power_mode);
    int ClearAllLayers();
    int PresentDisplay(int32_t* release_fd);
    int RegisterVsyncCallback(iahwc_callback_data_t data,
                              iahwc_function_ptr_t hook);
    void RegisterPixelUploaderCallback(iahwc_callback_data_t data,
                                       iahwc_function_ptr_t hook);
    int CreateLayer(uint32_t* layer_handle);
    int DestroyLayer(uint32_t layer_handle);
    bool IsConnected();
    IAHWCLayer& get_layer(iahwc_layer_t layer) {
      return layers_.at(layer);
    }

    int DisableOverlayUsage();

    int EnableOverlayUsage();

    void Synchronize() override;

    int RegisterHotPlugCallback(iahwc_callback_data_t data,
                                iahwc_function_ptr_t func);
    int RunPixelUploader(bool enable);

   private:
    PixelUploader* raw_data_uploader_ = NULL;
    hwcomposer::NativeDisplay* native_display_;
    std::map<iahwc_layer_t, IAHWCLayer> layers_;
  };

  static IAHWC* toIAHWC(iahwc_device_t* dev) {
    return static_cast<IAHWC*>(dev);
  }

  static int HookOpen(const iahwc_module_t*, iahwc_device_t**);
  static int HookClose(iahwc_device_t*);
  static iahwc_function_ptr_t HookGetFunctionPtr(iahwc_device_t*, int);

  template <typename PFN, typename T>
  static iahwc_function_ptr_t ToHook(T function) {
    static_assert(std::is_same<PFN, T>::value, "Incompatible fn pointer");
    return reinterpret_cast<iahwc_function_ptr_t>(function);
  }

  template <typename T, typename HookType, HookType func, typename... Args>
  static T DeviceHook(iahwc_device_t* dev, Args... args) {
    IAHWC* hwc = toIAHWC(dev);
    return static_cast<T>(((*hwc).*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t DisplayHook(iahwc_device_t* dev,
                             iahwc_display_t display_handle, Args... args) {
    IAHWC* hwc = toIAHWC(dev);
    IAHWCDisplay* display = hwc->displays_.at(display_handle);
    return static_cast<int32_t>((display->*func)(std::forward<Args>(args)...));
  }

  template <typename HookType, HookType func, typename... Args>
  static int32_t LayerHook(iahwc_device_t* dev, iahwc_display_t display_handle,
                           iahwc_layer_t layer_handle, Args... args) {
    IAHWC* hwc = toIAHWC(dev);
    IAHWCDisplay* display = hwc->displays_.at(display_handle);
    IAHWCLayer& layer = display->get_layer(layer_handle);

    return static_cast<int32_t>((layer.*func)(std::forward<Args>(args)...));
  }

 private:
  int GetNumDisplays(int* num_displays);
  int RegisterCallback(int32_t description, uint32_t display_handle,
                       iahwc_callback_data_t data, iahwc_function_ptr_t hook);
  hwcomposer::GpuDevice device_;
  std::vector<IAHWCDisplay*> displays_;
};

}  // namespace hwcomposer
#endif
