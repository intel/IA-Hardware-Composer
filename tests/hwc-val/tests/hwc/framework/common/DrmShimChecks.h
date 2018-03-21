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

#ifndef __DrmShimChecks_h__
#define __DrmShimChecks_h__

// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.
#include "HwcTestDefs.h"
#include "HwcTestKernel.h"
#include "DrmShimWork.h"
#include "BufferObject.h"
#include "DrmShimCrtc.h"
#include "HwcvalStall.h"
#include "HwcvalDrmParser.h"
#include <drm_mode.h>

#include <xf86drmMode.h>  //< For structs and types.
#include <i915_drm.h>

class DrmShimPlane;

namespace Hwcval {
class PropertyManager;
}

#define EXPORT_API __attribute__((visibility("default")))
class EXPORT_API DrmShimChecks : public HwcTestKernel {
 protected:
  int mShimDrmFd;

  // All CRTCs, by CRTC id
  std::map<uint32_t, DrmShimCrtc*> mCrtcs;

  // Crtc for each connector
  class Connector {
   public:
    DrmShimCrtc* mCrtc;
    HwcTestCrtc::ModeVec mModes;
    uint32_t mDisplayIx;
    HwcTestState::DisplayType mRealDisplayType;

    uint32_t mAttributes;
    uint32_t mRealRefresh;
  };

  std::map<uint32_t, Connector> mConnectors;

  // gralloc buffer object tracking by fb ID
  std::map<uint32_t, std::shared_ptr<DrmShimBuffer> >
      mBuffersByFbId;

  // List of all hot-pluggable connectors
  std::set<uint32_t> mHotPluggableConnectors;

  // Per-encoder data
  std::map<uint32_t, uint32_t> mConnectorForEncoder;
  std::map<uint32_t, uint32_t> mPossibleCrtcsForEncoder;

  // CRTCs indexed by pipe index - NOT the same as display index
  DrmShimCrtc* mCrtcByPipe[HWCVAL_MAX_PIPES];

  // Frame number currently processing in HWC's DRM thread (according to log
  // entries)
  int mCurrentFrame[HWCVAL_MAX_CRTCS];
  bool mLastFrameWasDropped[HWCVAL_MAX_CRTCS];

  // DRM property manager, coping with spoofed properties
  Hwcval::PropertyManager* mPropMgr;

  // Are universal planes enabled?
  bool mUniversalPlanes;

  // Frame number of frames being sent to DRM
  uint32_t mDrmFrameNo;

  // Parsing
  Hwcval::DrmParser mDrmParser;

 public:
  //-----------------------------------------------------------------------------
  // Constructor & Destructor
  DrmShimChecks();
  virtual ~DrmShimChecks();

  // Connector attributes (bit masks)
  enum {
    eDDRFreq = 1,  // DDR frequency can be set
    eDRRS = 2      // DRRS Enabled
  };

  // Access functions

  EXPORT_API void SetFd(int fd);
  EXPORT_API int GetFd();

  /// Enable/disable universal plane support
  void SetUniversalPlanes(bool enable = true);

  // Shim check functions
  // Both AddFB and AddFB2
  EXPORT_API void CheckAddFB(int fd, uint32_t width, uint32_t height,
                             uint32_t pixel_format, uint32_t depth,
                             uint32_t bpp, uint32_t bo_handles[4],
                             uint32_t pitches[4], uint32_t offsets[4],
                             uint32_t buf_id, uint32_t flags, __u64 modifier[4],
                             int ret);

  EXPORT_API void CheckRmFB(int fd, uint32_t bufferId);

  // Processing of work from work queue
  // This will take place when it's safe to take a lock.
  void DoWork(const Hwcval::Work::AddFbItem& item);
  void DoWork(const Hwcval::Work::RmFbItem& item);

  EXPORT_API void checkPageFlipEnter(int fd, uint32_t crtc_id, uint32_t fb_id,
                                     uint32_t flags, void*& user_data);

  EXPORT_API void checkPageFlipExit(int fd, uint32_t crtc_id, uint32_t fb_id,
                                    uint32_t flags, void* user_data, int ret);

  EXPORT_API void checkSetPlaneEnter(int fd, uint32_t plane_id,
                                     uint32_t crtc_id, uint32_t fb_id,
                                     uint32_t flags, uint32_t crtc_x,
                                     uint32_t crtc_y, uint32_t crtc_w,
                                     uint32_t crtc_h, uint32_t src_x,
                                     uint32_t src_y, uint32_t src_w,
                                     uint32_t src_h, void*& user_data);

  EXPORT_API void checkSetPlaneExit(int fd, uint32_t plane_id, uint32_t crtc_id,
                                    uint32_t fb_id, uint32_t flags,
                                    uint32_t crtc_x, uint32_t crtc_y,
                                    uint32_t crtc_w, uint32_t crtc_h,
                                    uint32_t src_x, uint32_t src_y,
                                    uint32_t src_w, uint32_t src_h, int ret);


  EXPORT_API void AtomicShimUserData(struct drm_mode_atomic* drmAtomic);
  EXPORT_API void AtomicUnshimUserData(struct drm_mode_atomic* drmAtomic);

  EXPORT_API void CheckGetResourcesExit(int fd, drmModeResPtr res);

  EXPORT_API void CheckGetConnectorExit(int fd, uint32_t connId,
                                        drmModeConnectorPtr& pConn);

  EXPORT_API void CheckGetEncoder(uint32_t encoder_id,
                                  drmModeEncoderPtr pEncoder);

  EXPORT_API void CheckSetCrtcEnter(int fd, uint32_t crtcId, uint32_t bufferId,
                                    uint32_t x, uint32_t y,
                                    uint32_t* connectors, int count,
                                    drmModeModeInfoPtr mode);

  EXPORT_API void CheckSetCrtcExit(int fd, uint32_t crtcId, uint32_t ret);

  EXPORT_API void CheckGetCrtcExit(uint32_t crtcId, drmModeCrtcPtr pCrtc);

  EXPORT_API void CheckGetPlaneResourcesExit(drmModePlaneResPtr pRes);

  EXPORT_API void CheckGetPlaneExit(uint32_t plane_id, drmModePlanePtr pPlane);

  EXPORT_API void CheckIoctlI915SetPlane180Rotation(
      struct drm_i915_plane_180_rotation* rot);

  EXPORT_API void CheckIoctlI915SetDecrypt(
      struct drm_i915_reserved_reg_bit_2* decrypt);

  EXPORT_API void CheckSetDPMS(uint32_t conn_id, uint64_t value,
                               HwcTestEventHandler* eventHandler,
                               HwcTestCrtc*& crtc, bool& reenable);
  EXPORT_API void CheckSetDPMSExit(uint32_t fd, HwcTestCrtc* crtc,
                                   bool reenable,
                                   HwcTestEventHandler* eventHandler,
                                   uint32_t status);
  EXPORT_API void CheckSetPanelFitter(uint32_t conn_id, uint64_t value);
  EXPORT_API void CheckSetPanelFitterSourceSize(uint32_t conn_id, uint32_t sw,
                                                uint32_t sh);
  EXPORT_API void CheckSetDDRFreq(uint64_t value);

  /// Get Crtc
  DrmShimCrtc* GetCrtc(uint32_t crtcId);
  DrmShimCrtc* GetCrtcByDisplayIx(uint32_t displayIx);
  DrmShimCrtc* GetCrtcByPipe(uint32_t pipeIx);
  uint32_t GetCrtcIdForConnector(uint32_t conn_id);

  /// Display property query
  virtual uint32_t GetDisplayProperty(uint32_t displayIx,
                                      HwcTestState::DisplayPropertyType prop);

  /// ESD Recovery
  virtual void MarkEsdRecoveryStart(uint32_t connectorId);

  /// Set reference to the DRM property manager
  void SetPropertyManager(Hwcval::PropertyManager& propMgr);

  /// Is this object a plane, CRTC or other?
  ObjectClass GetObjectClass(uint32_t objId);

  // Convert DRM (nuclear on Broxton) transform values to HAL transform values.
  uint32_t DrmTransformToHalTransform(HwcTestState::DeviceType deviceType,
                                      uint32_t drmTransform);

  /// Broxton plane scaler validation
  static uint32_t BroxtonPlaneValidation(HwcTestCrtc* crtc,
                                         std::shared_ptr<DrmShimBuffer> buf,
                                         const char* str, uint32_t id,
                                         double srcW, double srcH,
                                         uint32_t dstW, uint32_t dstH,
                                         uint32_t transform);

  // Simulate hotplug on any suitable display
  virtual EXPORT_API bool SimulateHotPlug(uint32_t displayTypes,
                                          bool connected);

  // Do we have any hotpluggable display connected?
  virtual EXPORT_API bool IsHotPluggableDisplayAvailable();

  // Set DRRS Enable/disable
  bool IsDRRSEnabled(uint32_t connId);

  // Convert an aspect ratio DRM value to a string
  static const char* AspectStr(uint32_t aspect);

  // Set current DRM frame number
  void SetDrmFrameNo(uint32_t drmFrame);

  // Validate stated display/frame against current state
  void ValidateFrame(uint32_t crtcId, uint32_t nextFrame);
  void ValidateDrmReleaseTo(uint32_t connectorId);
  void ValidateFrame(DrmShimCrtc* crtc, uint32_t nextFrame, bool drop);

  // Validation of additional parsed events
  void ValidateEsdRecovery(uint32_t d);
  void ValidateDisplayMapping(uint32_t connId, uint32_t crtcId);
  void ValidateDisplayUnmapping(uint32_t crtcId);

 protected:
  /// Override the preferred mode suggested by DRM
  void OverrideDefaultMode(drmModeConnectorPtr pConn);

  /// Note that a buffer has been drawn on a particular plane
  std::shared_ptr<DrmShimBuffer> UpdateBufferPlane(uint32_t fbId, DrmShimCrtc* crtc,
                                               DrmShimPlane* plane);

  /// Create a tracking object for a buffer object
  virtual HwcTestBufferObject* CreateBufferObject(int fd, uint32_t boHandle);

  /// Find or create and index a tracking object for a bo
  virtual std::shared_ptr<HwcTestBufferObject> GetBufferObject(uint32_t boHandle);

  /// Move device-specific ids from old to new buffer
  virtual void MoveDsIds(std::shared_ptr<DrmShimBuffer> existingBuf,
                         std::shared_ptr<DrmShimBuffer> buf);

  // Change the number and order of modes in a mode list (from
  // drmModeGetConnector)
  void RandomizeModes(int& count, drmModeModeInfoPtr& modes);

  // Log the modes
  void LogModes(uint32_t connId, const char* str, drmModeConnectorPtr pConn);

  // Can HWC set DDR freq?
  virtual bool IsDDRFreqSupported();

 private:
  // Create the CRTC object for the stated pipe index, unless it already exists
  // This will be a very minimal object, more details will be filled in later.
  DrmShimCrtc* CreatePipe(uint32_t pipe, uint32_t crtcId = 0);

  // Return the plane with the requested plane Id
  // (for nuclear spoofing when we don't have universal plane support, this is
  // more complex
  // than it appears)
  DrmShimPlane* GetDrmPlane(uint32_t drmPlaneId);

  // Associate a display index with a DRM connector and hence a physical display
  void MapDisplay(int32_t displayIx, uint32_t connId, uint32_t crtcId);

  // Returns the parser object
  virtual Hwcval::LogChecker* GetParser();
};

inline DrmShimCrtc* DrmShimChecks::GetCrtc(uint32_t crtcId) {
  return mCrtcs[crtcId];
}

inline void DrmShimChecks::SetFd(int fd) {
  mShimDrmFd = fd;
}

inline int DrmShimChecks::GetFd() {
  return mShimDrmFd;
}

inline void DrmShimChecks::SetUniversalPlanes(bool enable) {
  HWCLOGD("DrmShimChecks@%p::SetUniversalPlanes(%d)", this, enable);
  mUniversalPlanes = enable;
}

inline void DrmShimChecks::SetDrmFrameNo(uint32_t frameNo) {
  mDrmFrameNo = frameNo;
}

#endif  // __DrmShimChecks_h__
