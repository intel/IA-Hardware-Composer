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

#ifndef __DrmShimBuffer_h__
#define __DrmShimBuffer_h__

#include <drm_fourcc.h>


#include <stdint.h>
#include <utils/Vector.h>
#include <set>
#include <map>
// NOTE: HwcTestDefs.h sets defines which are used in the HWC and DRM stack.
// -> to be included before any other HWC or DRM header file.

#include "hwcbuffer.h"
#include "HwcTestDefs.h"
#include "hardware/hwcomposer_defs.h"
#include "HwcTestUtil.h"
#include "DrmShimTransform.h"
#include "public/nativebufferhandler.h"
#include "os/android/platformdefines.h"

#define BUFIDSTR "prime"

class DrmShimPlane;
class DrmShimBuffer;
class HwcTestBufferObject;

typedef std::vector<DrmShimTransform> DrmShimTransformVector;
typedef std::set<DrmShimTransform> DrmShimSortedTransformVector;
typedef std::set<DrmShimTransform>::iterator DrmShimSortedTransformVectorItr;
typedef std::vector<std::shared_ptr<DrmShimBuffer> > DrmShimBufferVector;
typedef std::set<std::shared_ptr<HwcTestBufferObject> >
    HwcTestBufferObjectVector;
typedef std::set<std::shared_ptr<HwcTestBufferObject> >::iterator
    HwcTestBufferObjectVectorItr;

class DrmShimBuffer  {
 public:
  struct FbIdData {
    uint32_t pixelFormat;
    bool hasAuxBuffer;
    uint32_t auxPitch;
    uint32_t auxOffset;
    __u64 modifier;
  };

  typedef std::map<uint32_t, FbIdData> FbIdVector;
  void setBufferHandler(hwcomposer::NativeBufferHandler *bufferHandler){bufferHandler_ = bufferHandler;}

 protected:
  HWCNativeHandle mHandle;         // Buffer handle
  HwcTestBufferObjectVector mBos;  // All open buffer objects

  int64_t mDsId;  // ADF device-specific Id
  int mAcquireFenceFd;

  bool mNew;   // This is a new buffer we haven't seen before.
  bool mUsed;  // Used either as a composition input or on a screen
  Hwcval::BufferSourceType
      mBufferSource;    // Is buffer the result of a composition?
  bool mBlanking;       // It is just a blanking buffer
  bool mBlack;          // Content is (believed to be) all black
  int32_t mFbtDisplay;  // -1 if not a FRAMEBUFFERTARGET; display index if it is

  char mStrFormat[5];  // Buffer format as a string
  bool mTransparentFromHarness;

  FbIdVector mFbIds;

  DrmShimTransformVector mCombinedFrom;
  uint32_t mBufferIx;  // For iteration functions

  // Lifetime management
  Hwcval::FrameNums mLastHwcFrame;

  // Last time buffer appeared in onSet
  Hwcval::FrameNums mLastOnSetFrame;

  // Shadow buffer for reference composition
  HWCNativeHandle mRefBuf;

  // Local copy of graphic buffer (only when needed for comparison)
  HWCNativeHandle mBufCpy;

  // Flag to indicate comparison is needed
  int32_t mToBeCompared;

  // Total comparison mismatches so far
  static uint32_t mCompMismatchCount;

  // How many times has the buffer appeared sequentially in the layer list?
  uint32_t mAppearanceCount;

  // Is the buffer content all nulls?
  Hwcval::BufferContentType mBufferContent;

 public:
  // Total count of buffers in existence
  static uint32_t mCount;

 private:
  void ReportCompositionMismatch(uint32_t lineWidthBytes,
                                 uint32_t lineStrideCpy, uint32_t lineStrideRef,
                                 double SSIMIndex, unsigned char* cpyData,
                                 unsigned char* refData);

 public:
  DrmShimBuffer(HWCNativeHandle handle, Hwcval::BufferSourceType bufferSource =
                                            Hwcval::BufferSourceType::Input);
  ~DrmShimBuffer();

  void FreeBufCopies();

  HWCNativeHandle GetHandle() const;

  bool IsOpen();
  uint32_t GetOpenCount();

  DrmShimBuffer* AddBo(std::shared_ptr<HwcTestBufferObject> bo);
  DrmShimBuffer* RemoveBo(std::shared_ptr<HwcTestBufferObject> bo);
  DrmShimBuffer* RemoveBo(int fd, uint32_t boHandle);
  HwcTestBufferObjectVector& GetBos();

  DrmShimBuffer* SetNew(bool isNew);
  bool IsNew();

  DrmShimBuffer* SetUsed(bool used);
  bool IsUsed();

  DrmShimBuffer* SetCompositionTarget(Hwcval::BufferSourceType bufferSource);
  Hwcval::BufferSourceType GetSource();
  bool IsCompositionTarget();

  DrmShimBuffer* SetBlanking(bool blanking);
  bool IsBlanking();

  DrmShimBuffer* SetBlack(bool black);
  bool IsBlack();

  DrmShimBuffer* SetFbtDisplay(uint32_t displayIx);
  bool IsFbt();
  uint32_t GetFbtDisplay();
  bool IsFbtDisplay0();

  FbIdVector& GetFbIds();
  FbIdData* GetFbIdData(uint32_t fbId);
  uint32_t GetPixelFormat(uint32_t fbId);
  uint32_t NumFbIds() const;

  // ADF device-specific Id
  DrmShimBuffer* SetDsId(int64_t dsId);
  int64_t GetDsId();
  DrmShimBuffer* SetGlobalId(int id);
  int GetGlobalId() const;

  DrmShimBuffer* UpdateResolveDetails();
  uint32_t GetWidth();
  uint32_t GetHeight();
  uint32_t GetAllocWidth();
  uint32_t GetAllocHeight();
  uint32_t GetUsage();

  // Returns whether this buffer contains a video format
  bool IsVideoFormat();
  bool IsNV12Format();

  // Returns whether the buffer is render compressed
  bool IsRenderCompressed();

  // Returns whether the buffer is in a format suitable for render compression
  bool IsRenderCompressibleFormat();

  // Return the up-to-date global ID for this buffer handle
  // (may not be same as our cached copy).
  int GetCurrentGlobalId();

  uint32_t GetAuxOffset();
  uint32_t GetAuxPitch();

  static const char* ValidityStr(Hwcval::ValidityType valid);

  uint32_t GetFormat() const;
  uint32_t GetDrmFormat();

  // Add and remove child buffers
  void AddCombinedFrom(DrmShimTransform& from);
  void SetAllCombinedFrom(const DrmShimTransformVector& combinedFrom);
  const DrmShimTransformVector& GetAllCombinedFrom();
  uint32_t NumCombinedFrom() const;

  // Iterate child buffers
  DrmShimTransform* FirstCombinedFrom();
  DrmShimTransform* NextCombinedFrom();
  void RemoveCurrentCombinedFrom();

  // Is buf one of the buffers that this one was composed from?
  bool IsCombinedFrom(std::shared_ptr<DrmShimBuffer> buf);

  // Use recursion to add FB Ids of all ancestors in layer list
  void AddSourceFBsToList(DrmShimSortedTransformVector& list,
                          DrmShimTransform& thisTransform,
                          uint32_t sources = 0);

  /// Buffer about to be deleted so make sure no-one points to us
  void Unassociate();

  /// Was this buffer first seen in layer list last frame?
  DrmShimBuffer* SetLastHwcFrame(Hwcval::FrameNums hwcFrame,
                                 bool isOnSet = false);
  bool IsCurrent(Hwcval::FrameNums hwcFrame);
  const char* GetHwcFrameStr(char* str, uint32_t len = HWCVAL_DEFAULT_STRLEN);

  /// Classify pixel format by bpp
  uint32_t GetBpp();

  /// Composition reference buffer handling
  /// Set reference to the reference buffer
  void SetRef(HWCNativeHandle& refBuf);
  void SetToBeCompared(bool toBeCompared = true);
  bool IsToBeCompared();
  bool IsToBeComparedOnce();

  /// Set copy of buffer for comparison purposes
  void SetBufCopy(HWCNativeHandle& buf);
  HWCNativeHandle GetBufCopy();
  bool HasBufCopy();

  /// Appearance counting (number of times sequentially in the layer list)
  DrmShimBuffer* IncAppearanceCount();
  void ResetAppearanceCount();
  uint32_t GetAppearanceCount();

  /// Is content of the buffer all nulls?
  bool IsBufferTransparent(const hwcomposer::HwcRect<int>& rect);
  static bool IsBufferTransparent(HWCNativeHandle handle,
                                  const hwcomposer::HwcRect<int>& rect);

  /// Compare buffer copy with copy of buffer from reference composition
  bool CompareWithRef(bool useAlpha,
                      hwcomposer::HwcRect<int>* mRectToCompare = 0);

  /// Does the buffer have a reference buffer copy?
  bool HasRef();

  // Harness says the buffer is transparent
  void SetTransparentFromHarness();
  bool IsActuallyTransparent();

  // Construct an identification string for logging
  char* IdStr(char* str, uint32_t len = HWCVAL_DEFAULT_STRLEN - 1) const;

  // Return type of buffer source as a string
  const char* GetSourceName();

  // Debug - Report contents
  void ReportStatus(int priority, const char* str);

  // Debug - check another buffer not referenced by this one before we delete
  void DbgCheckNoReferenceTo(DrmShimBuffer* buf) const;

  // String version of buffer format
  const char* StrBufFormat();

  // Does the buffer format have plane alpha
  static bool FormatHasPixelAlpha(uint32_t format);
  bool FormatHasPixelAlpha();

 private:
  static bool IsVideoFormat(uint32_t format);
  static bool IsNV12Format(uint32_t format);
  hwcomposer::NativeBufferHandler *bufferHandler_;
};

#endif  // __DrmShimBuffer_h__
