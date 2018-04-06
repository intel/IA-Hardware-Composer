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

#include "DrmShimBuffer.h"
#include "BufferObject.h"
#include <cutils/log.h>
#include "HwcTestState.h"
#include "DrmShimPlane.h"
#include "DrmShimChecks.h"
#include "HwcTestUtil.h"
#include "HwcTestDebug.h"
#include "drm_fourcc.h"
#include "SSIMUtils.h"
#include <math.h>
#include <utils/Atomic.h>
#include "public/nativebufferhandler.h"

using namespace Hwcval;

uint32_t DrmShimBuffer::mCount = 0;
uint32_t DrmShimBuffer::mCompMismatchCount = 0;
static int sNumBufCopies = 0;

// Usual constructor, when we recognise a new buffer passed into OnSet
DrmShimBuffer::DrmShimBuffer(HWCNativeHandle handle,
                             Hwcval::BufferSourceType bufferSource)
    : mHandle(handle),
      mDsId(0),
      mAcquireFenceFd(-1),
      mNew(true),
      mUsed(false),
      mBufferSource(bufferSource),
      mBlanking(false),
      mBlack(false),
      mFbtDisplay(-1),
      mTransparentFromHarness(false),
      mBufferIx(0),
      mToBeCompared(0),
      mAppearanceCount(0),
      mBufferContent(Hwcval::BufferContentType::ContentNotTested) {
  for (uint32_t i = 0; i < HWCVAL_MAX_CRTCS; ++i) {
    mLastHwcFrame[i] = 0xfffffffe;
    mLastOnSetFrame[i] = 0;
  }

  ++mCount;
  HWCLOGD_COND(eLogBuffer, "DrmShimBuffer::DrmShimBuffer Created buf@%p", this);
}

DrmShimBuffer::~DrmShimBuffer() {
  if (mAcquireFenceFd > 0) {
    CloseFence(mAcquireFenceFd);
  }

  FreeBufCopies();

  --mCount;
  HWCLOGD_COND(eLogBuffer, "DrmShimBuffer::~DrmShimBuffer Deleted buf@%p",
               this);
}

void DrmShimBuffer::FreeBufCopies() {
  if (mBufCpy) {
    --sNumBufCopies;
  }
  /*munsih FIX_ME free buffer*/
  mBufCpy = 0;
  mRefBuf = 0;
}

HWCNativeHandle DrmShimBuffer::GetHandle() const {
  return mHandle;
}

bool DrmShimBuffer::IsOpen() {
  return (GetOpenCount() != 0);
}

uint32_t DrmShimBuffer::GetOpenCount() {
  return mBos.size();
}

DrmShimBuffer* DrmShimBuffer::AddBo(std::shared_ptr<HwcTestBufferObject> bo) {
  mBos.emplace(bo);
  return this;
}

DrmShimBuffer* DrmShimBuffer::RemoveBo(std::shared_ptr<HwcTestBufferObject> bo) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  char strbuf2[HWCVAL_DEFAULT_STRLEN];

  int ix = mBos.erase(bo);

  if (ix ==  0) {
    HWCLOGI_COND(eLogBuffer, "DrmShimBuffer::RemoveBo %s not found in %s",
                 bo->IdStr(strbuf), IdStr(strbuf2));
  }
  return this;
}

DrmShimBuffer* DrmShimBuffer::RemoveBo(int fd, uint32_t boHandle) {
  for (HwcTestBufferObjectVectorItr itr = mBos.begin(); itr != mBos.end(); ++itr) {
    std::shared_ptr<HwcTestBufferObject> bo = *itr;
    if ((bo->mFd == fd) && (bo->mBoHandle == boHandle)) {
      mBos.erase(bo);
      break;
    }
  }

  return this;
}

HwcTestBufferObjectVector& DrmShimBuffer::GetBos() {
  return mBos;
}

DrmShimBuffer* DrmShimBuffer::SetNew(bool isNew) {
  mNew = isNew;
  return this;
}

bool DrmShimBuffer::IsNew() {
  return mNew;
}

DrmShimBuffer* DrmShimBuffer::SetUsed(bool used) {
  mUsed = used;
  return this;
}

bool DrmShimBuffer::IsUsed() {
  return mUsed;
}

DrmShimBuffer* DrmShimBuffer::SetCompositionTarget(
    Hwcval::BufferSourceType bufferSource) {
  mBufferSource = bufferSource;
  return this;
}

Hwcval::BufferSourceType DrmShimBuffer::GetSource() {
  return mBufferSource;
}

bool DrmShimBuffer::IsCompositionTarget() {
  return ((mBufferSource != Hwcval::BufferSourceType::Input) &&
          (mBufferSource != Hwcval::BufferSourceType::Hwc));
}

DrmShimBuffer* DrmShimBuffer::SetBlanking(bool blanking) {
  mBlanking = blanking;
  return this;
}

bool DrmShimBuffer::IsBlanking() {
  return mBlanking;
}

DrmShimBuffer* DrmShimBuffer::SetBlack(bool black) {
  mBlack = black;
  return this;
}

bool DrmShimBuffer::IsBlack() {
  return mBlack;
}

DrmShimBuffer* DrmShimBuffer::SetFbtDisplay(uint32_t displayIx) {
  mFbtDisplay = (int32_t)displayIx;
  return this;
}

bool DrmShimBuffer::IsFbt() {
  return (mFbtDisplay >= 0);
}

uint32_t DrmShimBuffer::GetFbtDisplay() {
  return (uint32_t)mFbtDisplay;
}

bool DrmShimBuffer::IsFbtDisplay0() {
  return (mFbtDisplay == 0);
}

uint32_t DrmShimBuffer::NumFbIds() const {
  return mFbIds.size();
}

DrmShimBuffer* DrmShimBuffer::SetDsId(int64_t dsId) {
  mDsId = dsId;
  return this;
}

int64_t DrmShimBuffer::GetDsId() {
  return mDsId;
}

DrmShimBuffer* DrmShimBuffer::SetGlobalId(int id) {
  return this;
}

int DrmShimBuffer::GetGlobalId() const {
  return -1;
}

DrmShimBuffer* DrmShimBuffer::UpdateResolveDetails() {
  return this;
}

bool DrmShimBuffer::IsRenderCompressed() {
  return false;
}

bool DrmShimBuffer::IsRenderCompressibleFormat() {
  return false;
}

uint32_t DrmShimBuffer::GetWidth() {
  return mHandle->meta_data_.width_;
}

uint32_t DrmShimBuffer::GetHeight() {
  return mHandle->meta_data_.height_;
}

uint32_t DrmShimBuffer::GetAllocWidth() {
  return mHandle->meta_data_.width_;
}

uint32_t DrmShimBuffer::GetAllocHeight() {
  return mHandle->meta_data_.height_;
}

uint32_t DrmShimBuffer::GetUsage() {
  return mHandle->meta_data_.usage_;
}

uint32_t DrmShimBuffer::GetFormat() const {
  return mHandle->meta_data_.native_format_;
}

uint32_t DrmShimBuffer::GetDrmFormat() {
  return mHandle->meta_data_.format_;
}

// Determines whether this buffer is a video format
bool DrmShimBuffer::IsVideoFormat(uint32_t format) {
  return ((format == DRM_FORMAT_YVU420) ||
          (format == DRM_FORMAT_NV12_Y_TILED_INTEL) ||
          (format == DRM_FORMAT_YUYV)) ||
         (format == DRM_FORMAT_NV12);
}

bool DrmShimBuffer::IsVideoFormat() {
  return IsVideoFormat(mHandle->meta_data_.format_);
}

// Determines whether this buffer is a video format
bool DrmShimBuffer::IsNV12Format(uint32_t format) {
  return ((format == DRM_FORMAT_NV12_Y_TILED_INTEL) ||
          (format == DRM_FORMAT_NV12));
}

// Determines whether this buffer is a video format
bool DrmShimBuffer::IsNV12Format() {
  return IsNV12Format(mHandle->meta_data_.format_);
}

DrmShimBuffer::FbIdData* DrmShimBuffer::GetFbIdData(uint32_t fbId) {
  return (mFbIds.find(fbId) != mFbIds.end()) ? &mFbIds.at(fbId) : nullptr;
}

uint32_t DrmShimBuffer::GetPixelFormat(uint32_t fbId) {

  if (mFbIds.find(fbId) != mFbIds.end()) {
    const FbIdData& fbIdData = mFbIds.at(fbId);
    return fbIdData.pixelFormat;
  } else {
    return 0;
  }
}

DrmShimBuffer::FbIdVector& DrmShimBuffer::GetFbIds() {
  return mFbIds;
}

void DrmShimBuffer::AddCombinedFrom(DrmShimTransform& child) {
  mCombinedFrom.push_back(child);
}

DrmShimTransform* DrmShimBuffer::FirstCombinedFrom() {
  if (mCombinedFrom.size() > 0) {
    mBufferIx = 0;
    return &(mCombinedFrom.at(0));
  }
  return (DrmShimTransform*)0;
}

DrmShimTransform* DrmShimBuffer::NextCombinedFrom() {
  ++mBufferIx;
  if (mBufferIx < mCombinedFrom.size()) {
    return &(mCombinedFrom.at(mBufferIx));
  }
  return (DrmShimTransform*)0;
}

void DrmShimBuffer::RemoveCurrentCombinedFrom() {
  if (mBufferIx < mCombinedFrom.size()) {
    mCombinedFrom.erase(mCombinedFrom.cbegin() + mBufferIx--);
  }
}

// Is buf one of the buffers that this one was composed from?
bool DrmShimBuffer::IsCombinedFrom(std::shared_ptr<DrmShimBuffer> buf) {
  if (buf.get() == this) {
    return true;
  } else {
    for (uint32_t i = 0; i < mCombinedFrom.size(); ++i) {
      DrmShimTransform& transform = mCombinedFrom.at(i);
      if (transform.GetBuf()->IsCombinedFrom(buf)) {
        return true;
      }
    }

    return false;
  }
}

// Recursively expand a transform using the "combined from" lists in its
// constituent DrmShimBuffer.
// The result will be a list of all the constituent transforms that should align
// with the original layer list.
void DrmShimBuffer::AddSourceFBsToList(DrmShimSortedTransformVector& list,
                                       DrmShimTransform& thisTransform,
                                       uint32_t sources) {
  char str[HWCVAL_DEFAULT_STRLEN];
  char strbuf2[HWCVAL_DEFAULT_STRLEN];

  sources |= 1 << static_cast<int>(GetSource());

  HWCLOGV_COND(eLogCombinedTransform,
               "DrmShimBuffer::AddSourceFBsToList Enter: transform@%p, buf@%p, "
               "sources 0x%x",
               &thisTransform, thisTransform.GetBuf().get(), sources);

  if (mCombinedFrom.size() > 0) {
    HWCLOGV_COND(eLogCombinedTransform, "%s transform@%p adding srcs %s:",
                 IdStr(str), &thisTransform,
                 DrmShimTransform::SourcesStr(sources, strbuf2));
    for (uint32_t i = 0; i < mCombinedFrom.size(); ++i) {
      DrmShimTransform& transform = mCombinedFrom.at(i);
      DrmShimTransform combinedTransform(transform, thisTransform,
                                         eLogCombinedTransform);
      transform.GetBuf()->AddSourceFBsToList(list, combinedTransform, sources);
    }
    HWCLOGV_COND(eLogCombinedTransform,
                 "DrmShimBuffer::AddSourceFBsToList Exit: transform@%p, "
                 "buf@%p, sources 0x%x",
                 &thisTransform, thisTransform.GetBuf().get(), sources);
    return;
  }

  thisTransform.SetSources(sources);
  list.emplace(thisTransform);

  // End of real function, start of debug info
  if (HWCCOND(eLogCombinedTransform)) {
    DrmShimSortedTransformVector tmp(list);
    DrmShimTransform& copyTransform = thisTransform;
    DrmShimSortedTransformVectorItr itrtemp = tmp.begin();

    HWCLOGV("  Adding original %s transform@%p->%p list size %d srcs: %s.",
            IdStr(str), &thisTransform, &copyTransform, list.size(),
            DrmShimTransform::SourcesStr(sources, strbuf2));

    copyTransform.Log(ANDROID_LOG_VERBOSE, "  Added FB");

    for (DrmShimSortedTransformVectorItr itr = list.begin(); itr != list.end(); ++itr) {
      DrmShimTransform tr = *itr;
      if (tr.GetBuf() != NULL) {
        HWCLOGI("%s", tr.GetBuf()->IdStr(str));
      } else {
        HWCLOGI("buf@0");
      }

      if (tr.GetBuf() == thisTransform.GetBuf()) {
        continue;
      }

      if (itrtemp != tmp.end()) {
        HWCERROR(eCheckInternalError,
                 "TRANSFORM MISMATCH: TOO MANY TRANSFORMS IN RESULT");
        continue;
      }

      DrmShimTransform sr = *itrtemp;
      if (tr.GetBuf() != sr.GetBuf()) {
        HWCERROR(eCheckInternalError,
                 "TRANSFORM MISMATCH: RESULT CONTAINS buf@%p NOT IN SOURCE",
                 tr.GetBuf().get());
        continue;
      }

      itrtemp++;
    }  // End for list.size()
    if (itrtemp != tmp.end()) {
      HWCERROR(eCheckInternalError,
               "TRANSFORM MISMATCH: NOT ALL SOURCES COPIED");
    }
  }

  HWCLOGV_COND(eLogCombinedTransform,
               "DrmShimBuffer::AddSourceFBsToList Exit: transform@%p, buf@%p, "
               "sources 0x%x",
               &thisTransform, thisTransform.GetBuf().get(), sources);
}

void DrmShimBuffer::SetAllCombinedFrom(
    const DrmShimTransformVector& combinedFrom) {
  HWCLOGD("SetAllCombinedFrom: buf@%p handle %p combined from %d transforms",
          this, mHandle, combinedFrom.size());
  mCombinedFrom = combinedFrom;
}

const DrmShimTransformVector& DrmShimBuffer::GetAllCombinedFrom() {
  return mCombinedFrom;
}

uint32_t DrmShimBuffer::NumCombinedFrom() const {
  return mCombinedFrom.size();
}

void DrmShimBuffer::Unassociate() {
  mCombinedFrom.clear();

  mFbtDisplay = -1;
}

DrmShimBuffer* DrmShimBuffer::SetLastHwcFrame(Hwcval::FrameNums fn,
                                              bool isOnSet) {
  mLastHwcFrame = fn;

  if (isOnSet) {
    mLastOnSetFrame = fn;
  }

  return this;
}

// This function determines whether a buffer is still 'current', i.e. the
// content is unchanged.
// HWC (from HWC 2.0 changes onwards) will do this by looking to see when the
// reference count of the buffer
// goes to zero.
//
// That is too complex for us right now. So we are looking to see if the buffer
// was used in the last frame
// on ANY of the displays.
//
// I suspect that this will need some work for HWC 2.0 to get it fully working.
// For example if one of the
// displays is turned off, it may appear that the buffer is still current.
bool DrmShimBuffer::IsCurrent(Hwcval::FrameNums fn) {
  for (uint32_t d = 0; d < HWCVAL_MAX_CRTCS; ++d) {
    if (fn[d] == HWCVAL_UNDEFINED_FRAME_NUMBER) {
      continue;
    }

    if ((mLastHwcFrame[d] + 1) >= fn[d]) {
      return true;
    }
  }

  return false;
}

const char* DrmShimBuffer::GetHwcFrameStr(char* str, uint32_t len) {
  if (HWCVAL_MAX_CRTCS >= 3) {
    snprintf(str, len, "frame:%d.%d.%d", mLastHwcFrame[0], mLastHwcFrame[1],
             mLastHwcFrame[2]);
  }

  return str;
}

void DrmShimBuffer::SetToBeCompared(bool toBeCompared) {
  mToBeCompared = toBeCompared;
}

bool DrmShimBuffer::IsToBeComparedOnce() {
  return (android_atomic_swap(0, &mToBeCompared) != 0);
}

bool DrmShimBuffer::IsToBeCompared() {
  return mToBeCompared;
}

// Set local copy of the buffer contents
// so we can do comparisons after the original buffer has been deallocated
void DrmShimBuffer::SetBufCopy(HWCNativeHandle& buf) {
  if ((mBufCpy == 0) && buf) {
    ++sNumBufCopies;
    if (sNumBufCopies > 10) {
      HWCLOGI("%d copies of buffers stored for transparency filter detection",
              sNumBufCopies);
    }
  } else if (mBufCpy && (buf == 0)) {
    --sNumBufCopies;
  }
  mBufCpy = buf;
}

HWCNativeHandle  DrmShimBuffer::GetBufCopy() {
  return mBufCpy;
}

bool DrmShimBuffer::HasBufCopy() {
  return (mBufCpy != 0);
}

DrmShimBuffer* DrmShimBuffer::IncAppearanceCount() {
  ++mAppearanceCount;
  return this;
}

void DrmShimBuffer::ResetAppearanceCount() {
  mAppearanceCount = 0;
}

uint32_t DrmShimBuffer::GetAppearanceCount() {
  return mAppearanceCount;
}

void DrmShimBuffer::SetRef(HWCNativeHandle& buf) {
  mRefBuf = buf;
}

uint32_t DrmShimBuffer::GetBpp() {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  HWCLOGD_COND(eLogFlicker, "%s format %s", IdStr(strbuf), StrBufFormat());

  switch (GetDrmFormat()) {
    case DRM_FORMAT_RGB332:
    case DRM_FORMAT_BGR233:
      return 8;

    case DRM_FORMAT_XRGB4444:
    case DRM_FORMAT_XBGR4444:
    case DRM_FORMAT_RGBX4444:
    case DRM_FORMAT_BGRX4444:
    case DRM_FORMAT_ARGB4444:
    case DRM_FORMAT_ABGR4444:
    case DRM_FORMAT_RGBA4444:
    case DRM_FORMAT_BGRA4444:
    case DRM_FORMAT_XRGB1555:
    case DRM_FORMAT_XBGR1555:
    case DRM_FORMAT_RGBX5551:
    case DRM_FORMAT_BGRX5551:
    case DRM_FORMAT_ARGB1555:
    case DRM_FORMAT_ABGR1555:
    case DRM_FORMAT_RGBA5551:
    case DRM_FORMAT_BGRA5551:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_BGR565:

    case DRM_FORMAT_YUYV:
      return 16;

    case DRM_FORMAT_RGB888:
    case DRM_FORMAT_BGR888:
      return 24;

    default:
      return 32;
  }
}

// Return a string specifying the buffer format.
const char* DrmShimBuffer::StrBufFormat() {
  *((uint32_t*)&mStrFormat) = GetDrmFormat();
  mStrFormat[4] = '\0';
  return mStrFormat;
}

// Return the identification string of the DrmShimBuffer
// This logs out all the interesting identification info including gralloc
// handle, buffer objects and framebuffer IDs.
char* DrmShimBuffer::IdStr(char* str, uint32_t len) const {
  uint32_t n = snprintf(str, len, "buf@%p handle %p " BUFIDSTR " 0x%x ", this,
                        mHandle, GetGlobalId());
  if (n >= len)
    return str;

  if (HWCCOND(eLogBuffer)) {
    for (HwcTestBufferObjectVectorItr itr = mBos.begin(); itr != mBos.end(); ++itr) {
      std::shared_ptr<HwcTestBufferObject> bo = *itr;
      n += bo->FullIdStr(str + n, len - n);
      if (n >= len)
        return str;

      n += snprintf(str + n, len - n, " ");
      if (n >= len)
        return str;

      // If the bo's reverse pointer to the buffer is wrong, log out what it
      // actually points to.
      std::shared_ptr<DrmShimBuffer> boBuf = bo->mBuf;
      if (boBuf.get() != this) {
        snprintf(str + n, len - n, "(!!!buf=%p) ", boBuf.get());
      }
    }
  }

  for (std::map<uint32_t, FbIdData>::const_iterator fbitr = mFbIds.begin(); fbitr != mFbIds.end(); ++fbitr) {
    if (fbitr == mFbIds.begin()) {
      n += snprintf(str + n, len - n, "FB %d",fbitr->first);
    } else {
      n += snprintf(str + n, len - n, ",%d", fbitr->first);
    }

    if (n >= len)
      return str;
  }

  if (mDsId > 0) {
    n += snprintf(str + n, len - n, "DS %" PRIi64, mDsId);
    if (n >= len)
      return str;
  }

  if (HWCVAL_MAX_CRTCS >= 3) {
    n += snprintf(str + n, len - n, " (last seen %s)",
                  std::string(mLastOnSetFrame).c_str());
  }

  return str;
}

// Return the buffer source as a string
const char* DrmShimBuffer::GetSourceName() {
  switch (mBufferSource) {
    case Hwcval::BufferSourceType::Input:
      return "Input";
    case Hwcval::BufferSourceType::PartitionedComposer:
      return "PartitionedComposer";
    case Hwcval::BufferSourceType::Writeback:
      return "Writeback";
    default:
      return "Unknown source";
  }
}

// Report buffer status for debug purposes
void DrmShimBuffer::ReportStatus(int priority, const char* str) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  // For efficiency, filter the logging at this point
  if (HwcGetTestConfig()->IsLevelEnabled(priority)) {
    HWCLOG(priority, "%s: %s %s %s", str, IdStr(strbuf), GetSourceName(),
           mBlanking ? "+Blanking" : "-Blanking");
    HWCLOG(priority, "  Size %dx%d Alloc %dx%d DrmFormat %x Usage %x",
           mHandle->meta_data_.width_, mHandle->meta_data_.height_,
           mHandle->meta_data_.pitches_[0], mHandle->meta_data_.height_,
           mHandle->meta_data_.usage_);
    if (mCombinedFrom.size() > 0) {
      char linebuf[200];
      char entrybuf[100];
      bool commaNeeded = false;
      linebuf[0] = '\0';

      for (uint32_t i = 0; i < mCombinedFrom.size(); ++i) {
        // Just for safety, never likely to happen
        if (strlen(linebuf) > (sizeof(linebuf) - sizeof(entrybuf))) {
          strcat(linebuf, "...");
          break;
        }

        DrmShimTransform& transform = mCombinedFrom.at(i);

        char str[HWCVAL_DEFAULT_STRLEN];
        sprintf(entrybuf, "%s", transform.GetBuf()->IdStr(str));

        if (commaNeeded) {
          strcat(linebuf, ", ");
        }
        strcat(linebuf, entrybuf);
        commaNeeded = true;
      }
      HWCLOG(priority, "  CombinedFrom: %s", linebuf);
    }
  }
}

// Debugging function
void DrmShimBuffer::DbgCheckNoReferenceTo(DrmShimBuffer* buf) const {
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  for (uint32_t cfix = 0; cfix < NumCombinedFrom(); ++cfix) {
    const DrmShimTransform& from = mCombinedFrom.at(cfix);
    if (from.GetConstBuf() == buf) {
      HWCERROR(eCheckInternalError,
               "Deleting %s which is referenced in combinedFrom %s",
               buf->IdStr(strbuf), from.GetConstBuf()->IdStr(strbuf));
    }
  }
}

// Determine if the buffer is transparent.
// The transparency state is cached so that the determination is done at most
// once for each buffer
// This checking should only be done for buffers that are in front of a NV12
// layer and have remained in the layer list
// for a long time.
bool DrmShimBuffer::IsBufferTransparent(const hwcomposer::HwcRect<int>& rect) {
  uint32_t logLevel = ANDROID_LOG_DEBUG;
  if (mTransparentFromHarness) {
    logLevel = ANDROID_LOG_WARN;
  }

  HWCLOG(
      logLevel,
      "DrmShimBuffer::IsBufferTransparent entry buf@%p handle %p %s rect(%d, "
      "%d, %d, %d)",
      this, mHandle,
      (mBufferContent == BufferContentType::ContentNull)
          ? "Null"
          : ((mBufferContent == BufferContentType::ContentNotNull) ? "Not Null"
                                                                   : ""),
      rect.left, rect.top, rect.right, rect.bottom);

  if (mBufferContent == BufferContentType::ContentNotTested) {

    if (HasBufCopy()) {
      // We MUST query the buffer copy for details rather than just using
      // mHandle,
      // because it will have a different pitch to the original buffer seeing as
      // we have
      // requested a copy in linear memory.

      if (mBufCpy) {
        HWCERROR(eCheckGrallocDetails,
                 "DrmShimBuffer::IsBufferTransparent can't get info for buf@%p "
                 "handle %p copy %p",
                 this, mHandle, mBufCpy->handle_);

        return false;
      }

      mBufferContent = IsBufferTransparent(mBufCpy,rect)
                           ? BufferContentType::ContentNull
                           : BufferContentType::ContentNotNull;
    } else {
      mBufferContent = BufferContentType::ContentNotNull;
    }
  }

  HWCLOG(
      logLevel, "DrmShimBuffer::IsBufferTransparent exit buf@%p handle%p %s",
      this, mHandle,
      (mBufferContent == BufferContentType::ContentNull)
          ? "Null"
          : ((mBufferContent == BufferContentType::ContentNotNull) ? "Not Null"
                                                                   : ""));

  return (mBufferContent == BufferContentType::ContentNull);
}

void DrmShimBuffer::SetTransparentFromHarness() {
  mTransparentFromHarness = true;
}

bool DrmShimBuffer::IsActuallyTransparent() {
  return mTransparentFromHarness;
}

void DrmShimBuffer::ReportCompositionMismatch(
    uint32_t lineWidthBytes, uint32_t lineStrideCpy, uint32_t lineStrideRef,
    double SSIMIndex, unsigned char* cpyData, unsigned char* refData) {
  ++mCompMismatchCount;

  // Do some stats
  uint32_t numMismatchBytes = 0;
  uint64_t sumOfSquares = 0;
  int mismatchLine = -1;

  for (int i = 0; i < mHandle->meta_data_.height_; ++i) {
    uint8_t* realDataLine = cpyData + (i * lineStrideCpy);
    uint8_t* refDataLine = refData + (i * lineStrideRef);

    uint64_t lineSumOfSquares = 0;

    for (uint32_t j = 0; j < lineWidthBytes; ++j) {
      if (realDataLine[j] != refDataLine[j]) {
        if (mismatchLine < 0) {
          mismatchLine = i;
        }

        ++numMismatchBytes;
        int diff = ((int)realDataLine[j]) - ((int)refDataLine[j]);
        lineSumOfSquares += diff * diff;
      }
    }

    sumOfSquares += lineSumOfSquares;
  }

  uint32_t numBytes = mHandle->meta_data_.height_ * lineWidthBytes;
  double meanSquares = ((double)sumOfSquares) / numBytes;
  double rms = sqrt(meanSquares);

  double percentageMismatch = 100.0 * ((double)numMismatchBytes) / numBytes;

  HWCERROR(IsFbt() ? eCheckSfCompMatchesRef : eCheckHwcCompMatchesRef,
           "CompareWithRef: Composition mismatch %d with real buffer handle %p "
           "from frame:%d at line %d",
           mCompMismatchCount, mHandle, mLastHwcFrame, mismatchLine);
  // NB %% does not work
  HWCLOGE(
      "  -- %2.6f%%%% of bytes mismatch; RMS = %3.6f; SSIM index = %f "
      "(frame:%d)",
      percentageMismatch, rms, SSIMIndex, mLastHwcFrame);

  // If we haven't already made too many files, dump the SF and reference data
  // to TGA files so we can examine them later
  if ((mCompMismatchCount * 2) < MAX_SF_MISMATCH_DUMP_FILES) {
    HwcTestDumpBufferToDisk("real", mCompMismatchCount, mBufCpy,
                            DUMP_BUFFER_TO_TGA);
    HwcTestDumpBufferToDisk("ref", mCompMismatchCount, mRefBuf,
                            DUMP_BUFFER_TO_TGA);
  }
}

// Compare the contents of the buffer with the reference composition using SSIM
// (Structural Similarity algorithm).
bool DrmShimBuffer::CompareWithRef(bool useAlpha,
                                   hwcomposer::HwcRect<int>* rectToCompare) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  if (mRefBuf == 0) {
    HWCERROR(eCheckInternalError, "CompareWithRef: %s NO REF!!", IdStr(strbuf));
    return false;
  } else {
    HWCLOGD("CompareWithRef: %s mem@%p compared with ref handle %p",
            IdStr(strbuf), mRefBuf->handle_);
  }

  unsigned char* cpyData;
  unsigned char* refData;
  uint32_t left = 0;
  uint32_t top = 0;
  uint32_t right = mBufCpy->meta_data_.width_;
  uint32_t bottom = mBufCpy->meta_data_.height_;
  uint32_t width = right - left;
  uint32_t height = bottom - top;

  bufferHandler_->Map(mBufCpy, left, top, width, height, 0,(void **) &cpyData, 0);
  if (cpyData == NULL) {
    HWCLOGW("CompareWithRef: Failed to lock cpy buffer");
    return false;
  }

  bufferHandler_->Map(mRefBuf, left, top, width, height, 0, (void **)&refData, 0);
  if (refData == NULL) {
    HWCLOGW("CompareWithRef: Failed to lock ref buffer");
    bufferHandler_->UnMap(mRefBuf,(void **)&refData);
    return false;
  }

  if (rectToCompare) {
    left = rectToCompare->left;
    top = rectToCompare->top;
    right = rectToCompare->right;
    bottom = rectToCompare->bottom;
  }

  // Compare data line by line
  uint32_t bytesPerPixel =
      mBufCpy->meta_data_.pitches_[0] / mBufCpy->meta_data_.width_;
  uint32_t lineWidthBytes = width * bytesPerPixel;
  HWCLOGD(
      "CompareWithRef: Comparing real %p ref %p (%d, %d) %dx%d Pitch %d Bytes "
      "Per Pixel %d",
      cpyData, refData, left, top, width, height,
      mBufCpy->meta_data_.pitches_[0], bytesPerPixel);

  bool same = true;
  for (uint32_t i = 0; i < height; ++i) {
    uint8_t* realDataLine = cpyData +
                            ((i + top) * mBufCpy->meta_data_.pitches_[0]) +
                            (left * bytesPerPixel);
    uint8_t* refDataLine = refData +
                           ((i + top) * mRefBuf->meta_data_.pitches_[0]) +
                           (left * bytesPerPixel);

    if (memcmp(realDataLine, refDataLine, lineWidthBytes) != 0) {
      same = false;
      break;
    }
  }

  if (!same) {
    // SSIM comparison algorithm

    double SSIMIndex = 0;
    dssim_info* dinf = new dssim_info();

    // load image content in the row pointers

    unsigned char* BufRowPointers[height];
    unsigned char* RefRowPointers[height];

    for (uint32_t i = 0; i < height; ++i) {
      // assign pointer for row
      BufRowPointers[i] = cpyData +
                          ((i + top) * mBufCpy->meta_data_.pitches_[0]) +
                          (left * bytesPerPixel);
    }

    for (uint32_t i = 0; i < height; ++i) {
      // assign pointer for row
      RefRowPointers[i] = refData +
                          ((i + top) * mRefBuf->meta_data_.pitches_[0]) +
                          (left * bytesPerPixel);
    }

    // set up timing information

    int64_t startTime = ns2ms(systemTime(SYSTEM_TIME_MONOTONIC));

    // SSIM preliminary calculations

    const int blur_type = ebtLinear;  // TODO - remove hard-coded option
   bool hasAlpha = (GetFormat() == DRM_FORMAT_ABGR8888) && useAlpha;

    DoSSIMCalculations(dinf, (dssim_rgba**)BufRowPointers,
                       (dssim_rgba**)RefRowPointers, width, height, blur_type,
                       hasAlpha);

    // calculate SSIM index averaged on channels

    double channelResult[CHANS];
    for (int ch = 0; ch < CHANS; ++ch) {
      channelResult[ch] = GetSSIMIndex(&dinf->chan[ch]);
    }

    HWCLOGD("SSIM indices per channel: %f %f %f", channelResult[0],
            channelResult[1], channelResult[2]);

    for (int ch = 0; ch < CHANS; ch++) {
      SSIMIndex += channelResult[ch];
    }
    SSIMIndex /= double(CHANS);

    // retrieve time information

    HWCLOGD("%s SSIM index = %.6f", __FUNCTION__, SSIMIndex);
    HWCLOGD("%s SSIM algorithm execution time in milliseconds: %llu",
            __FUNCTION__,
            (ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)) - startTime));

    // deallocations

    delete (dinf);

    // END SSIM comparison algorithm

    if (SSIMIndex < HWCVAL_SSIM_ACCEPTANCE_LEVEL) {
      ReportCompositionMismatch(lineWidthBytes, mBufCpy->meta_data_.pitches_[0],
                                mRefBuf->meta_data_.pitches_[0], SSIMIndex,
                                cpyData, refData);
    } else {
      HWCLOGI(
          "CompareWithRef: %s: Comparison passed with SSIM Index = %.6f "
          "(frame:%d)",
          IdStr(strbuf), SSIMIndex, mLastHwcFrame);
    }
  } else {
    HWCLOGI("CompareWithRef: %s comparison pass (identical)", IdStr(strbuf));
  }

  // This matches the potential error in ReportCompositionMismatch()
  HWCCHECK(IsFbt() ? eCheckSfCompMatchesRef : eCheckHwcCompMatchesRef);

  bufferHandler_->UnMap(mBufCpy,(void **) &cpyData);
  bufferHandler_->UnMap(mRefBuf,(void **)&refData);

  FreeBufCopies();

  return same;
}

bool DrmShimBuffer::HasRef() {
  return (mRefBuf != 0);
}


const char* DrmShimBuffer::ValidityStr(Hwcval::ValidityType valid) {
  switch (valid) {
    case ValidityType::Invalid: {
      return "Invalid";
    }
    case ValidityType::InvalidWithinTimeout: {
      return "Invalid within timeout";
    }
    case ValidityType::Invalidating: {
      return "Invalidating";
    }
    case ValidityType::ValidUntilModeChange: {
      return "Valid until mode change";
    }
    case ValidityType::Valid: {
      return "Valid";
    }
    case ValidityType::Indeterminate: {
      return "Indeterminate";
    }
    default: { return "Unknown"; }
  }
}

uint32_t DrmShimBuffer::GetAuxOffset() {
  return 0;
}

uint32_t DrmShimBuffer::GetAuxPitch() {
  return 0;
}

bool DrmShimBuffer::FormatHasPixelAlpha(uint32_t format) {
  return HasAlpha(format);
}

bool DrmShimBuffer::FormatHasPixelAlpha() {
  return FormatHasPixelAlpha(GetFormat());
}
