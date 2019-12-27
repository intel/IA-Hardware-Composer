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

#include "virtualpanoramadisplay.h"

#include <drm_fourcc.h>

#include <hwclayer.h>
#include <nativebufferhandler.h>

#include <sstream>
#include <vector>

#include "hwctrace.h"
#include "overlaylayer.h"

#include "gpudevice.h"
#include "hwcutils.h"

namespace hwcomposer {

VirtualPanoramaDisplay::VirtualPanoramaDisplay(
    uint32_t gpu_fd, NativeBufferHandler *buffer_handler, uint32_t pipe_id,
    uint32_t /*crtc_id*/)
    : output_handle_(0),
      acquire_fence_(-1),
      width_(0),
      height_(0),
      display_index_(pipe_id) {
  resource_manager_.reset(new ResourceManager(buffer_handler));
  if (!resource_manager_) {
    ETRACE("Failed to construct hwc layer buffer manager");
  }
  compositor_.Init(resource_manager_.get(), gpu_fd);
}

void VirtualPanoramaDisplay::InitHyperDmaBuf() {
  if (hyper_dmabuf_initialized)
    return;
#ifdef HYPER_DMABUF_SHARING
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
  if (mHyperDmaBuf_Fd > 0) {
    hyper_dmabuf_initialized = true;
  }
#endif
}

VirtualPanoramaDisplay::~VirtualPanoramaDisplay() {
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
  HyperDmaUnExport();
#endif
}

#ifdef HYPER_DMABUF_SHARING
void VirtualPanoramaDisplay::HyperDmaUnExport() {
  HyperDmaExport(true);
  if (mHyperDmaBuf_Fd > 0) {
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

    mHyperDmaExportedBuffers.clear();

    close(mHyperDmaBuf_Fd);
    mHyperDmaBuf_Fd = -1;
  }
  hyper_dmabuf_initialized = false;
}
#endif

void VirtualPanoramaDisplay::InitVirtualDisplay(uint32_t width,
                                                uint32_t height) {
  width_ = width;
  height_ = height;
  CreateOutBuffer();
}

bool VirtualPanoramaDisplay::GetActiveConfig(uint32_t *config) {
  if (!config)
    return false;

  config[0] = 1;
  return true;
}

bool VirtualPanoramaDisplay::SetActiveConfig(uint32_t /*config*/) {
  return true;
}

void VirtualPanoramaDisplay::HyperDmaExport(bool notify_stopping) {
#ifdef HYPER_DMABUF_SHARING
  if (mHyperDmaBuf_Fd <= 0) {
    ETRACE("Hyper DmaBuf: Device is not ready\n");
    return;
  }

  int ret = 0;
  uint32_t surf_index = display_index_;
  size_t info_size = sizeof(vm_buffer_info);
  size_t header_size = sizeof(vm_header);
  vm_header header;
  vm_buffer_info info;
  memset(&header, 0, header_size);
  memset(&info, 0, info_size);
  struct ioctl_hyper_dmabuf_export_remote msg;
  char meta_data[header_size + info_size];

  header.n_buffers = 1;
  header.version = 3;
  header.output = display_index_;
  header.counter = frame_count_++;
  header.disp_w = width_;
  header.disp_h = height_;

  std::shared_ptr<OverlayBuffer> buffer(NULL);
  uint32_t gpu_fd = resource_manager_->GetNativeBufferHandler()->GetFd();
  uint32_t id = GetNativeBuffer(gpu_fd, output_handle_);
  buffer = resource_manager_->FindCachedBuffer(id);
  const uint32_t *pitches;
  const uint32_t *offsets;
  int32_t fd = 0;

  if (buffer == NULL) {
    buffer = OverlayBuffer::CreateOverlayBuffer();
    buffer->InitializeFromNativeHandle(output_handle_, resource_manager_.get());
    resource_manager_->RegisterBuffer(id, buffer);
    fd = buffer->GetPrimeFD();
    if (fd > 0) {
      mHyperDmaExportedBuffers[fd].hyper_dmabuf_id =
          (hyper_dmabuf_id_t){-1, {-1, -1, -1}};
      mHyperDmaExportedBuffers[fd].width = buffer->GetWidth();
      mHyperDmaExportedBuffers[fd].height = buffer->GetHeight();
      mHyperDmaExportedBuffers[fd].format = buffer->GetFormat();
      pitches = buffer->GetPitches();
      offsets = buffer->GetOffsets();
      mHyperDmaExportedBuffers[fd].pitch[0] = pitches[0];
      mHyperDmaExportedBuffers[fd].pitch[1] = pitches[1];
      mHyperDmaExportedBuffers[fd].pitch[2] = pitches[2];
      mHyperDmaExportedBuffers[fd].offset[0] = offsets[0];
      mHyperDmaExportedBuffers[fd].offset[1] = offsets[1];
      mHyperDmaExportedBuffers[fd].offset[2] = offsets[2];
      mHyperDmaExportedBuffers[fd].tile_format = buffer->GetTilingMode();
      mHyperDmaExportedBuffers[fd].rotation = 0;
      mHyperDmaExportedBuffers[fd].status = 0;
      mHyperDmaExportedBuffers[fd].counter = 0;
      if (notify_stopping) {
        // Send an invalid surface_id to let SOS daemon knowns guest is stopping
        // sharing.
        mHyperDmaExportedBuffers[fd].surface_id = 0xff;
      } else {
        mHyperDmaExportedBuffers[fd].surface_id = display_index_;
      }
      mHyperDmaExportedBuffers[fd].bbox[0] = 0;
      mHyperDmaExportedBuffers[fd].bbox[1] = 0;
      mHyperDmaExportedBuffers[fd].bbox[2] = buffer->GetWidth();
      mHyperDmaExportedBuffers[fd].bbox[3] = buffer->GetHeight();
    }
  } else {
    if (!notify_stopping) {
      fd = buffer->GetPrimeFD();
      mHyperDmaExportedBuffers[fd].surface_id = display_index_;
    }
  }

  msg.sz_priv = header_size + info_size;
  msg.priv = meta_data;

  /* TODO: add more flexibility here, instead of hardcoded domain 0*/
  msg.remote_domain = 0;
  msg.dmabuf_fd = buffer->GetPrimeFD();

  char index[15];
  mHyperDmaExportedBuffers[msg.dmabuf_fd].surf_index = surf_index;
  memset(index, 0, sizeof(index));
  snprintf(index, sizeof(index), "Cluster_%d", surf_index);
  strncpy(mHyperDmaExportedBuffers[msg.dmabuf_fd].surface_name, index,
          SURFACE_NAME_LENGTH);
  memcpy(meta_data, &header, header_size);
  memcpy(meta_data + header_size, &mHyperDmaExportedBuffers[msg.dmabuf_fd],
         info_size);

  ret = ioctl(mHyperDmaBuf_Fd, IOCTL_HYPER_DMABUF_EXPORT_REMOTE, &msg);
  if (ret) {
    ETRACE("Hyper DmaBuf: Exporting hyper_dmabuf failed with error %d\n", ret);
    return;
  }

  // unexporting previous hyper_dmabuf_id for the gem object
  if (mHyperDmaExportedBuffers[msg.dmabuf_fd].hyper_dmabuf_id.id != -1 &&
      memcmp(&mHyperDmaExportedBuffers[msg.dmabuf_fd].hyper_dmabuf_id, &msg.hid,
             sizeof(hyper_dmabuf_id_t))) {
    struct ioctl_hyper_dmabuf_unexport unexport_msg;
    int ret;
    unexport_msg.hid = mHyperDmaExportedBuffers[msg.dmabuf_fd].hyper_dmabuf_id;
    unexport_msg.delay_ms = 100; /* 100ms would be enough */

    ret = ioctl(mHyperDmaBuf_Fd, IOCTL_HYPER_DMABUF_UNEXPORT, &unexport_msg);
    if (ret) {
      ETRACE(
          "Hyper DmaBuf:"
          "IOCTL_HYPER_DMABUF_UNEXPORT ioctl failed %d [0x%x]\n",
          ret, unexport_msg.hid.id);
    } else {
      ITRACE(
          "Hyper DmaBuf:"
          "IOCTL_HYPER_DMABUF_UNEXPORT ioctl Done [0x%x]!\n",
          unexport_msg.hid.id);
    }
  }

  mHyperDmaExportedBuffers[msg.dmabuf_fd].hyper_dmabuf_id = msg.hid;

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
              "Hyper DmaBuf: IOCTL_HYPER_DMABUF_UNEXPORT ioctl failed %d "
              "[0x%x]\n",
              ret, search->second.hyper_dmabuf_id.id);
        } else {
          ITRACE(
              "Hyper DmaBuf: IOCTL_HYPER_DMABUF_UNEXPORT ioctl Done [0x%x]!\n",
              search->second.hyper_dmabuf_id.id);
        }
        mHyperDmaExportedBuffers.erase(search);
      }

      FrameBufferManager *fb_manager =
          GpuDevice::getInstance().GetFrameBufferManager();

      if (fb_manager) {
        fb_manager->RemoveFB(handle.handle_->meta_data_.num_planes_,
                             handle.handle_->meta_data_.gem_handles_);
      }

      handler->ReleaseBuffer(handle.handle_);
      handler->DestroyHandle(handle.handle_);
    }
  }
#endif
}

bool VirtualPanoramaDisplay::Present(std::vector<HwcLayer *> &source_layers,
                                     int32_t *retire_fence,
                                     PixelUploaderCallback * /*call_back*/,
                                     bool handle_constraints) {
  CTRACE();

  if (!hyper_dmabuf_mode_) {
    return true;
  }

  if (!hyper_dmabuf_initialized) {
    InitHyperDmaBuf();
  }

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

    handle_constraints = true;

    overlay_layer.InitializeFromHwcLayer(
        layer, resource_manager_.get(), previous_layer, z_order, layer_index,
        height_, width_, kIdentity, handle_constraints);
    index.emplace_back(z_order);
    layers_rects.emplace_back(overlay_layer.GetDisplayFrame());

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
            layers, layers_rects, index, resource_manager_.get(), width_,
            height_, output_handle_, acquire_fence_, retire_fence)) {
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

#ifdef HYPER_DMABUF_SHARING
  HyperDmaExport(false);
#endif

  return true;
}

void VirtualPanoramaDisplay::CreateOutBuffer() {
  const NativeBufferHandler *handler =
      resource_manager_->GetNativeBufferHandler();
  HWCNativeHandle native_handle = 0;
  bool modifier_used = false;
  uint32_t usage = hwcomposer::kLayerNormal;

  handler->CreateBuffer(width_, height_, DRM_FORMAT_BGRA8888, &native_handle,
                        usage, &modifier_used);

  DTRACE("Create Buffer handler :%p", native_handle);
  SetOutputBuffer(native_handle, -1);
}

void VirtualPanoramaDisplay::SetOutputBuffer(HWCNativeHandle buffer,
                                             int32_t acquire_fence) {
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
  } else {
    delete buffer;
  }

  if (acquire_fence_ > 0) {
    close(acquire_fence_);
    acquire_fence_ = -1;
  }

  if (acquire_fence > 0) {
    acquire_fence_ = dup(acquire_fence);
  }
}

bool VirtualPanoramaDisplay::Initialize(
    NativeBufferHandler * /*buffer_manager*/) {
  return true;
}

bool VirtualPanoramaDisplay::GetDisplayAttribute(uint32_t /*config*/,
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

bool VirtualPanoramaDisplay::GetDisplayConfigs(uint32_t *num_configs,
                                               uint32_t *configs) {
  if (!num_configs)
    return false;
  *num_configs = 1;
  if (configs)
    configs[0] = 0;

  return true;
}

bool VirtualPanoramaDisplay::GetDisplayName(uint32_t *size, char *name) {
  std::ostringstream stream;
  stream << "Virtual Panorama:" << display_index_;
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

int VirtualPanoramaDisplay::GetDisplayPipe() {
  return -1;
}

#ifdef HYPER_DMABUF_SHARING
bool VirtualPanoramaDisplay::SetHyperDmaBufMode(uint32_t mode) {
  if (hyper_dmabuf_mode_ != mode) {
    hyper_dmabuf_mode_ = mode;
    if (hyper_dmabuf_mode_) {
      // Trigger hyperdmabuf sharing
      // Disable hyper dmabuf sharing to make sure the Present method has the
      // chance to re-establish the new hyper dmabuf channel.
      // The purpose is to workaround SOS vmdisplay-wayland's refresh issue
      // after resumsing from latest hyper dmabuf sharing stopping.
      HyperDmaUnExport();
      resource_manager_->PurgeBuffer();
    }
  }

  return true;
}
#endif

bool VirtualPanoramaDisplay::SetPowerMode(uint32_t /*power_mode*/) {
  return true;
}

int VirtualPanoramaDisplay::RegisterVsyncCallback(
    std::shared_ptr<VsyncCallback> /*callback*/, uint32_t /*display_id*/) {
  // return 0;
  return 1;
}

void VirtualPanoramaDisplay::VSyncControl(bool /*enabled*/) {
}

bool VirtualPanoramaDisplay::CheckPlaneFormat(uint32_t /*format*/) {
  // assuming that virtual display supports the format
  return true;
}
}  // namespace hwcomposer
