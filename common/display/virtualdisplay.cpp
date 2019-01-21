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

#include "virtualdisplay.h"

#include <drm_fourcc.h>

#include <hwclayer.h>
#include <nativebufferhandler.h>

#include <sstream>
#include <vector>

#include "hwctrace.h"
#include "overlaylayer.h"

#include "hwcutils.h"

namespace hwcomposer {

VirtualDisplay::VirtualDisplay(uint32_t gpu_fd,
                               NativeBufferHandler *buffer_handler,
                               FrameBufferManager *frame_buffer_manager,
                               uint32_t pipe_id, uint32_t /*crtc_id*/)
    : output_handle_(0),
      acquire_fence_(-1),
      width_(0),
      height_(0),
      display_index_(pipe_id) {
  resource_manager_.reset(new ResourceManager(buffer_handler));
  if (!resource_manager_) {
    ETRACE("Failed to construct hwc layer buffer manager");
  }
  fb_manager_ = frame_buffer_manager;
  compositor_.Init(resource_manager_.get(), gpu_fd, fb_manager_);
#ifdef HYPER_DMABUF_SHARING
  if (display_index_ == 0) {
    int ret;
    struct ioctl_hyper_dmabuf_tx_ch_setup msg;
    memset(&msg, 0, sizeof(ioctl_hyper_dmabuf_tx_ch_setup));

    mHyperDmaBuf_Fd = open(HYPER_DMABUF_PATH, O_RDWR);
    if (mHyperDmaBuf_Fd < 0)
      ETRACE("Hyper DmaBuf: open hyper dmabuf device node %s failed because %s",
             HYPER_DMABUF_PATH, strerror(errno));
    else {
      ITRACE("Hyper DmaBuf: open hyper dmabuf device node %s successfully!",
             HYPER_DMABUF_PATH);
      /* TODO: add config option to specify which domains should be used, for
       * now we share always with dom0 */
      msg.remote_domain = 0;
      ret = ioctl(mHyperDmaBuf_Fd, IOCTL_HYPER_DMABUF_TX_CH_SETUP, &msg);
      if (ret) {
        ETRACE(
            "Hyper DmaBuf:"
            "IOCTL_HYPER_DMABUF_TX_CH_SETUP failed with error %d\n",
            ret);
        close(mHyperDmaBuf_Fd);
        mHyperDmaBuf_Fd = -1;
      } else
        ITRACE("Hyper DmaBuf: IOCTL_HYPER_DMABUF_TX_CH_SETUP Done!\n");
    }
  }
#endif
}

VirtualDisplay::~VirtualDisplay() {
  if (acquire_fence_ > 0) {
    close(acquire_fence_);
  }

  if (handle_) {
    ResourceHandle temp;
    temp.handle_ = handle_;
    resource_manager_->MarkResourceForDeletion(temp, false);
  }

  if (output_handle_) {
    delete output_handle_;
  }

  std::vector<OverlayLayer>().swap(in_flight_layers_);

  resource_manager_->PurgeBuffer();
  compositor_.Reset();
#ifdef HYPER_DMABUF_SHARING
  if (mHyperDmaBuf_Fd > 0 && display_index_ == 0) {
    auto it = mHyperDmaExportedBuffers.begin();
    for (; it != mHyperDmaExportedBuffers.end(); ++it) {
      struct ioctl_hyper_dmabuf_unexport msg;
      int ret;
      msg.hid = it->second.hyper_dmabuf_id;
      // Todo: find a reduced dmabuf free delay time
      msg.delay_ms = 1000;
      ret = ioctl(mHyperDmaBuf_Fd, IOCTL_HYPER_DMABUF_UNEXPORT, &msg);
      if (ret) {
        ETRACE(
            "Hyper DmaBuf:"
            "IOCTL_HYPER_DMABUF_UNEXPORT ioctl failed %d [0x%x]\n",
            ret, it->second.hyper_dmabuf_id.id);
      } else {
        ITRACE("Hyper DmaBuf: IOCTL_HYPER_DMABUF_UNEXPORT ioctl Done [0x%x]!\n",
               it->second.hyper_dmabuf_id.id);
      }
    }
    /* Clear up the map of exported buffers whatever if the ioctl of
     * IOCTL_HYPER_DMABUF_UNEXPORTED success or fail.
     */
    mHyperDmaExportedBuffers.clear();

    close(mHyperDmaBuf_Fd);
    mHyperDmaBuf_Fd = -1;
  }
#endif
}

void VirtualDisplay::InitVirtualDisplay(uint32_t width, uint32_t height) {
  width_ = width;
  height_ = height;
}

bool VirtualDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 1;
  return true;
}

bool VirtualDisplay::SetActiveConfig(uint32_t /*config*/) {
  return true;
}

bool VirtualDisplay::Present(std::vector<HwcLayer *> &source_layers,
                             int32_t *retire_fence,
                             PixelUploaderCallback * /*call_back*/,
                             bool handle_constraints) {
#ifdef HYPER_DMABUF_SHARING
  if (display_index_ == 0) {
    int ret = 0;
    size_t size = source_layers.size();
    const uint32_t *pitches;
    const uint32_t *offsets;
    HWCNativeHandle sf_handle;
    size_t buffer_number = 0;
    uint32_t surf_index = 0;
    size_t info_size = sizeof(vm_buffer_info);
    size_t header_size = sizeof(vm_header);
    vm_header header;
    vm_buffer_info info;
    memset(&header, 0, header_size);
    memset(&info, 0, info_size);
    struct ioctl_hyper_dmabuf_export_remote msg;
    char meta_data[header_size + info_size];
    uint32_t imported_fd = 0;

    resource_manager_->RefreshBufferCache();
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      HwcLayer *layer = source_layers.at(layer_index);
      if (!layer->IsVisible())
        continue;
      // Discard protected video for tear down
      if (discard_protected_video_) {
        if (layer->GetNativeHandle() != NULL &&
            (layer->GetNativeHandle()->meta_data_.usage_ &
             hwcomposer::kLayerProtected))
          continue;
      }

      buffer_number++;
    }

    header.n_buffers = buffer_number;
    header.version = 3;
    header.output = 0;
    header.counter = frame_count_++;
    header.disp_w = width_;
    header.disp_h = height_;

    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      HwcLayer *layer = source_layers.at(layer_index);
      if (!layer->IsVisible())
        continue;

      if (discard_protected_video_) {
        if (layer->GetNativeHandle() != NULL &&
            (layer->GetNativeHandle()->meta_data_.usage_ &
             hwcomposer::kLayerProtected))
          continue;
      }

      const HwcRect<int> &display_frame = layer->GetDisplayFrame();
      sf_handle = layer->GetNativeHandle();

      if (NULL == sf_handle) {
        ITRACE("Skip layer index: %u for Hyper DMA buffer sharing",
               layer_index);
        continue;
      }

      std::shared_ptr<OverlayBuffer> buffer(NULL);
      uint32_t gpu_fd = resource_manager_->GetNativeBufferHandler()->GetFd();
      uint32_t id = GetNativeBuffer(gpu_fd, sf_handle);
      buffer = resource_manager_->FindCachedBuffer(id);
      if (buffer == NULL) {
        buffer = OverlayBuffer::CreateOverlayBuffer();
        buffer->InitializeFromNativeHandle(sf_handle, resource_manager_.get(),
                                           fb_manager_);
        resource_manager_->RegisterBuffer(id, buffer);
        imported_fd = buffer->GetPrimeFD();

        if (mHyperDmaBuf_Fd > 0 && imported_fd > 0) {
          mHyperDmaExportedBuffers[imported_fd].width = buffer->GetWidth();
          mHyperDmaExportedBuffers[imported_fd].height = buffer->GetHeight();
          mHyperDmaExportedBuffers[imported_fd].format = buffer->GetFormat();
          pitches = buffer->GetPitches();
          offsets = buffer->GetOffsets();
          mHyperDmaExportedBuffers[imported_fd].pitch[0] = pitches[0];
          mHyperDmaExportedBuffers[imported_fd].pitch[1] = pitches[1];
          mHyperDmaExportedBuffers[imported_fd].pitch[2] = pitches[2];
          mHyperDmaExportedBuffers[imported_fd].offset[0] = offsets[0];
          mHyperDmaExportedBuffers[imported_fd].offset[1] = offsets[1];
          mHyperDmaExportedBuffers[imported_fd].offset[2] = offsets[2];
          mHyperDmaExportedBuffers[imported_fd].tile_format =
              buffer->GetTilingMode();
          mHyperDmaExportedBuffers[imported_fd].rotation = 0;
          mHyperDmaExportedBuffers[imported_fd].status = 0;
          mHyperDmaExportedBuffers[imported_fd].counter = 0;
          mHyperDmaExportedBuffers[imported_fd].surface_id =
              (uint64_t)sf_handle;
          mHyperDmaExportedBuffers[imported_fd].bbox[0] = display_frame.left;
          mHyperDmaExportedBuffers[imported_fd].bbox[1] = display_frame.top;
          mHyperDmaExportedBuffers[imported_fd].bbox[2] = buffer->GetWidth();
          mHyperDmaExportedBuffers[imported_fd].bbox[3] = buffer->GetHeight();
        }
      }

      msg.sz_priv = header_size + info_size;
      msg.priv = meta_data;

      /* TODO: add more flexibility here, instead of hardcoded domain 0*/
      msg.remote_domain = 0;
      msg.dmabuf_fd = buffer->GetPrimeFD();

      char index[15];
      mHyperDmaExportedBuffers[buffer->GetPrimeFD()].surf_index = surf_index;
      memset(index, 0, sizeof(index));
      snprintf(index, sizeof(index), "Cluster_%d", surf_index);
      strncpy(mHyperDmaExportedBuffers[buffer->GetPrimeFD()].surface_name,
              index, SURFACE_NAME_LENGTH);
      mHyperDmaExportedBuffers[buffer->GetPrimeFD()].hyper_dmabuf_id =
          (hyper_dmabuf_id_t){-1, {-1, -1, -1}};
      memcpy(meta_data, &header, header_size);
      memcpy(meta_data + header_size,
             &mHyperDmaExportedBuffers[buffer->GetPrimeFD()], info_size);

      ret = ioctl(mHyperDmaBuf_Fd, IOCTL_HYPER_DMABUF_EXPORT_REMOTE, &msg);
      if (ret) {
        ETRACE("Hyper DmaBuf: Exporting hyper_dmabuf failed with error %d\n",
               ret);
        return false;
      }
      mHyperDmaExportedBuffers[buffer->GetPrimeFD()].hyper_dmabuf_id = msg.hid;
      surf_index++;
    }

    resource_manager_->PreparePurgedResources();

    std::vector<ResourceHandle> purged_gl_resources;
    std::vector<MediaResourceHandle> purged_media_resources;
    bool has_gpu_resource = false;
    resource_manager_->GetPurgedResources(
        purged_gl_resources, purged_media_resources, &has_gpu_resource);
    size_t purged_size = purged_gl_resources.size();

    if (purged_size != 0) {
      const NativeBufferHandler *handler =
          resource_manager_->GetNativeBufferHandler();

      for (size_t i = 0; i < purged_size; i++) {
        const ResourceHandle &handle = purged_gl_resources.at(i);
        if (!handle.handle_) {
          continue;
        }

        auto search = mHyperDmaExportedBuffers.find(
            handle.handle_->imported_handle_->data[0]);
        if (search != mHyperDmaExportedBuffers.end()) {
          struct ioctl_hyper_dmabuf_unexport msg;
          int ret;
          msg.hid = search->second.hyper_dmabuf_id;
          msg.delay_ms = 1000;
          ret = ioctl(mHyperDmaBuf_Fd, IOCTL_HYPER_DMABUF_UNEXPORT, &msg);
          if (ret) {
            ETRACE(
                "Hyper DmaBuf:"
                "IOCTL_HYPER_DMABUF_UNEXPORT ioctl failed %d [0x%x]\n",
                ret, search->second.hyper_dmabuf_id.id);
          } else {
            ITRACE(
                "Hyper DmaBuf:"
                "IOCTL_HYPER_DMABUF_UNEXPORT ioctl Done [0x%x]!\n",
                search->second.hyper_dmabuf_id.id);
          }
          mHyperDmaExportedBuffers.erase(search);
        }
        handler->ReleaseBuffer(handle.handle_);
        handler->DestroyHandle(handle.handle_);
      }
    }
    return true;
  }
#endif
  CTRACE();
  std::vector<OverlayLayer> layers;
  std::vector<HwcRect<int>> layers_rects;
  std::vector<size_t> index;
  int ret = 0;
  size_t size = source_layers.size();
  size_t previous_size = in_flight_layers_.size();
  bool frame_changed = (size != previous_size);
  bool layers_changed = frame_changed;
  *retire_fence = -1;
  uint32_t z_order = 0;

  resource_manager_->RefreshBufferCache();
  for (size_t layer_index = 0; layer_index < size; layer_index++) {
    HwcLayer *layer = source_layers.at(layer_index);
    layer->SetReleaseFence(-1);
    if (!layer->IsVisible())
      continue;
    if (discard_protected_video_) {
      if (layer->GetNativeHandle() != NULL &&
          (layer->GetNativeHandle()->meta_data_.usage_ &
           hwcomposer::kLayerProtected))
        continue;
    }

    layers.emplace_back();
    OverlayLayer &overlay_layer = layers.back();
    OverlayLayer *previous_layer = NULL;
    if (previous_size > z_order) {
      previous_layer = &(in_flight_layers_.at(z_order));
    }

    overlay_layer.InitializeFromHwcLayer(
        layer, resource_manager_.get(), previous_layer, z_order, layer_index,
        height_, kIdentity, handle_constraints, fb_manager_);
    index.emplace_back(z_order);
    layers_rects.emplace_back(layer->GetDisplayFrame());
    z_order++;

    if (frame_changed) {
      layer->Validate();
      continue;
    }

    if (!previous_layer || overlay_layer.HasLayerContentChanged() ||
        overlay_layer.HasDimensionsChanged()) {
      layers_changed = true;
    }

    layer->Validate();
  }

  if (layers_changed) {
    compositor_.BeginFrame(false);

    // Prepare for final composition.
    if (!compositor_.DrawOffscreen(
            layers, layers_rects, index, resource_manager_.get(), fb_manager_,
            width_, height_, output_handle_, acquire_fence_, retire_fence)) {
      ETRACE("Failed to prepare for the frame composition ret=%d", ret);
      return false;
    }

    acquire_fence_ = 0;

    in_flight_layers_.swap(layers);
  }

  int32_t fence = *retire_fence;

  if (fence > 0) {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      HwcLayer *layer = source_layers.at(layer_index);
      layer->SetReleaseFence(dup(fence));
    }
  } else {
    for (size_t layer_index = 0; layer_index < size; layer_index++) {
      const OverlayLayer &overlay_layer =
          in_flight_layers_.at(index.at(layer_index));
      HwcLayer *layer = source_layers.at(overlay_layer.GetLayerIndex());
      layer->SetReleaseFence(overlay_layer.ReleaseAcquireFence());
    }
  }
  if (resource_manager_->PreparePurgedResources()) {
    compositor_.FreeResources();
  }

  return true;
}

void VirtualDisplay::SetOutputBuffer(HWCNativeHandle buffer,
                                     int32_t acquire_fence) {
#ifdef HYPER_DMABUF_SHARING
  if (display_index_ == 0) {
    delete buffer;
    return;
  }
#endif

  if (!output_handle_ || output_handle_ != buffer) {
    const NativeBufferHandler *handler =
        resource_manager_->GetNativeBufferHandler();

    if (handle_) {
      handler->ReleaseBuffer(handle_);
      handler->DestroyHandle(handle_);
    }

    delete output_handle_;
    output_handle_ = buffer;
    handle_ = 0;

    if (output_handle_) {
      handler->CopyHandle(output_handle_, &handle_);
    }
  }

  if (acquire_fence_ > 0) {
    close(acquire_fence_);
    acquire_fence_ = -1;
  }

  if (acquire_fence > 0) {
    acquire_fence_ = dup(acquire_fence);
  }
}

bool VirtualDisplay::Initialize(NativeBufferHandler * /*buffer_manager*/,
                                FrameBufferManager *frame_buffer_manager) {
  return true;
}

bool VirtualDisplay::GetDisplayAttribute(uint32_t /*config*/,
                                         HWCDisplayAttribute attribute,
                                         int32_t *value) {
  // We always get the values from preferred mode config.
  switch (attribute) {
    case HWCDisplayAttribute::kWidth:
      *value = width_;
      break;
    case HWCDisplayAttribute::kHeight:
      *value = height_;
      break;
    case HWCDisplayAttribute::kRefreshRate:
      // in nanoseconds
      *value = 16666666;
      break;
    case HWCDisplayAttribute::kDpiX:
      // Dots per 1000 inches
      *value = 1;
      break;
    case HWCDisplayAttribute::kDpiY:
      // Dots per 1000 inches
      *value = 1;
      break;
    default:
      *value = -1;
      return false;
  }

  return true;
}

bool VirtualDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                       uint32_t *configs) {
  if (!num_configs)
    return false;
  *num_configs = 1;
  if (configs)
    configs[0] = 0;

  return true;
}

bool VirtualDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Virtual:" << display_index_;
  std::string string = stream.str();
  size_t length = string.length();
  if (!name) {
    *size = length;
    return true;
  }

  *size = std::min<uint32_t>(static_cast<uint32_t>(length - 1), *size);
  strncpy(name, string.c_str(), *size);
  return true;
}

int VirtualDisplay::GetDisplayPipe() {
  return -1;
}

bool VirtualDisplay::SetPowerMode(uint32_t /*power_mode*/) {
  return true;
}

int VirtualDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> /*callback*/, uint32_t /*display_id*/) {
  return 0;
}

void VirtualDisplay::VSyncControl(bool /*enabled*/) {
}

bool VirtualDisplay::CheckPlaneFormat(uint32_t /*format*/) {
  // assuming that virtual display supports the format
  return true;
}
}  // namespace hwcomposer
