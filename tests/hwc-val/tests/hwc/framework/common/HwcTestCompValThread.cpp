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

#include "HwcTestCompValThread.h"
#include "DrmShimTransform.h"
#include "HwcTestKernel.h"
#include "HwcTestUtil.h"

#include "HwcvalHwc2Content.h"

using namespace Hwcval;

HwcTestCompValThread::HwcTestCompValThread()
    : hwcomposer::HWCThread(0, "CompValThread"), mValSeq(0), mFenceForClosure(0), mConsecutiveAbortedCompareCount(0) {
}

HwcTestCompValThread::~HwcTestCompValThread() {
  // Close any remaining pending fence
  QueueFenceForClosure(0);
}

bool HwcTestCompValThread::Compose(std::shared_ptr<DrmShimBuffer> buf,
                                   Hwcval::LayerList& sources,
                                   Hwcval::ValLayer& dest) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  HWCLOGD("HwcTestCompValThread::Compose Enter %s", buf->IdStr(strbuf));

  if ((buf->GetDrmFormat() != DRM_FORMAT_ABGR8888) &&
      (buf->GetDrmFormat() != DRM_FORMAT_XBGR8888)) {
    HWCLOGD(
        "Can't validate composition of buf@%p handle %p because it is format "
        "0x%x, not RGBA/RGBX",
        buf.get(), buf->GetHandle(), buf->GetDrmFormat());
    return false;
  }

  {
    // Is the thread already busy?
    Trylock l(mMutex);

    if (!l.IsLocked() || (mBuf.get() != 0) || (mBufToCompare.get() != 0)) {
      HWCLOGD(
          "HwcTestCompValThread busy, compose not done. mBuf=%p, "
          "mBufToCompare=%p",
          mBuf.get(), mBufToCompare.get());
      SkipComp(buf);
      return false;
    }

    if (buf->HasRef()) {
      HWCLOGD(
          "HwcTestCompValThread::Compose aborted because buffer %s already has "
          "reference composition",
          buf->IdStr(strbuf));
      return false;
    }

    buf->SetToBeCompared();

    mBufToCompose = buf;
    mBuf = buf;
    mRectToCompare = dest.GetDisplayFrame();
  }

  HWCLOGD("HwcTestCompValThread::Compose buf@%p handle %p %s", mBuf.get(),
          dest.GetHandle(), mBuf->GetHwcFrameStr(strbuf));
  uint32_t numSources = sources.GetNumLayers();
  hwcval_layer_t valSources[numSources];

  // Allocate a big buffer for
  uint32_t maxRects = 1024;
  hwc_rect_t rectsBuf[maxRects];
  uint32_t rectsRemaining = maxRects;
  hwc_rect_t* pRect = rectsBuf;

  for (uint32_t i = 0; i < numSources; ++i) {
    ValLayer& layer = sources.GetLayer(i);

    HwcvalLayerToHwc2("HwcTestCompValThread::Compose: input", i, valSources[i],
                      layer, pRect, rectsRemaining);
    valSources[i].compositionType = HWC2_COMPOSITION_CLIENT;
  }

  // Replace the original dest buffer in the layer list with one of our own
  // Get size of target so we can allocate one the same

  if (!dest.GetHandle()) {
    // If we have no handle, then don't attempt to compose
    ClearLocked(mBuf);
    return true;
  }

  HWCLOGV(
      "HwcTestCompValThread::Compose allocating buffer display %dx%d format %x "
      "usage %x",
      dest.GetHandle()->meta_data_.width_, dest.GetHandle()->meta_data_.height_,
      dest.GetHandle()->meta_data_.format_);

  HWCNativeHandle grallocBuf;
  bufferHandler->CreateBuffer(
      dest.GetHandle()->meta_data_.width_, dest.GetHandle()->meta_data_.height_,
      dest.GetHandle()->meta_data_.format_, &grallocBuf);
  bufferHandler->CopyHandle(grallocBuf, &grallocBuf);
  bufferHandler->ImportBuffer(grallocBuf);

  // Copy the destination layer but give it an orphaned DrmShimBuffer so we can
  // use the handle we want.
  HwcvalLayerToHwc2("HwcTestCompValThread::Compose: dest", 0, mDest, dest,
                    pRect, rectsRemaining);
  mDest.gralloc_handle = grallocBuf;

  if (mDest.gralloc_handle == 0) {
    HWCERROR(eCheckTestBufferAlloc,
             "HwcTestCompValThread::Compose: Failed to allocate buffer %dx%d "
             "format %d usage %d",
             dest.GetHandle()->meta_data_.width_,
             dest.GetHandle()->meta_data_.height_,
             dest.GetHandle()->meta_data_.format_);
    SkipComp(mBuf);
    ClearLocked(mBuf);
    return false;
  }

  // Perform the reference composition
  int st =
      mComposer.Compose(numSources, valSources, &mDest, true);
  HWCLOGV("HwcTestCompValThread::Compose, about to CpyRef buf@%p handle %p %s",
          mBuf.get(), mDest.gralloc_handle, mBuf->GetHwcFrameStr(strbuf));
  mUseAlpha = (mDest.blending != HWC_BLENDING_NONE);

  if (st == 0) {
    mBuf->SetRef(grallocBuf);
  } else {
    HWCLOGW(
        "HwcTestCompValThread::Compose Reference composition failed to CpyRef "
        "buf@%p handle %p %s",
        mBuf.get(), mDest.gralloc_handle, mBuf->GetHwcFrameStr(strbuf));
    mBuf = 0;
  }

  // Signal the fence
  ++mValSeq;

  ClearLocked(mBufToCompose);

  HWCLOGD("HwcTestCompValThread::Compose Exit");

  return true;
}

void HwcTestCompValThread::Compare(std::shared_ptr<DrmShimBuffer> buf) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  if ((buf.get() == 0) || (buf->GetHandle() == 0) || mBuf.get() == 0) {
    return;
  }

  HWCLOGD("HwcTestCompValThread::Compare %s", buf->IdStr(strbuf));

  {
    Trylock l(mMutex);
    if (!l.IsLocked()) {
      HWCLOGV(
          "HwcTestCompValThread::Compare failed to gain lock so handle %p not "
          "compared",
          buf->GetHandle());
      return;
    }

    if (!buf->IsToBeComparedOnce() || buf != mBuf) {
      if (mBuf.get() == 0) {
        return;
      }

      // We only want to compare the buffer we have composed
      HWCLOGD(
          "HwcTestCompValThread::Compare Not comparing buffer %p as we are "
          "waiting for buffer %p",
          buf->GetHandle(), mBuf->GetHandle());
      ++mConsecutiveAbortedCompareCount;

      if ((mConsecutiveAbortedCompareCount > 20) &&
          (mBufToCompose.get() == 0) && (mBufToCompare.get() == 0)) {
        HWCLOGD(
            "HwcTestCompValThread::Compare Abandoning reference composition of "
            "buffer %p",
            mBuf->GetHandle());
        mConsecutiveAbortedCompareCount = 0;
        mBuf->FreeBufCopies();
        SkipComp(mBuf);
        mBuf = 0;
      }

      return;
    }

    mBufToCompare = buf;
  }

  // Take a copy of the "real" composition so we can compare it with the
  // reference.
  HWCNativeHandle bufCopy =
      mComposer.CopyBuf(mBufToCompare->GetHandle());
  mBufToCompare->SetBufCopy(bufCopy);

  // Trigger the thread to start the comparison
  HWCLOGD("HwcTestCompValThread::Compare Signal. mBufToCompare %s",
          mBufToCompare->IdStr(strbuf));
  mCondition.signal();

  HWCLOGD("HwcTestCompValThread::Compare Exit");
}

bool HwcTestCompValThread::GetWork() {
  HWCVAL_LOCK(_l, mMutex);

    if (mBufToCompare.get())
      return true;
    else
      return false;
    mCondition.wait(mMutex);
}

void HwcTestCompValThread::ClearLocked(std::shared_ptr<DrmShimBuffer>& buf) {
  HWCVAL_LOCK(_l, mMutex);
  buf = 0;
}

void HwcTestCompValThread::SkipComp(std::shared_ptr<DrmShimBuffer>& buf) {
  if (buf->IsFbt()) {
    ++HwcGetTestResult()->mSfCompValSkipped;
  } else {
    ++HwcGetTestResult()->mHwcCompValSkipped;
  }
}

void HwcTestCompValThread::HandleRoutine() {
  HWCLOGD("HwcTestCompValThread::threadLoop starting");

  while (GetWork()) {
    if (mBufToCompare.get()) {
      DoCompare();
    }

    HWCLOGD("HwcTestCompValThread Idle");
  }

}

void HwcTestCompValThread::QueueFenceForClosure(int fence) {
  // The idea here is just to put off closing the fence until we are fairly sure
  // the OnSet has completed
  // As the compares take a while, this code makes this extremely likely to be
  // true.
  // but if occasional problems arise with this, we could make a vector of
  // fences to be closed and close them
  // all at the end of the OnSet. Of course if we do this we would have to
  // protect the vector with a mutex
  // which could cause its own problems.
  int f = android_atomic_swap(fence, &mFenceForClosure);

  if (f > 0) {
    CloseFence(f);
  }
}

void HwcTestCompValThread::KillThread() {
  mCondition.signal();
  Exit();
}

void HwcTestCompValThread::DoCompare() {
  HWCLOGD("HwcTestCompValThread::DoCompare handle %p",
          mBufToCompare->GetHandle());

  // Now we have consumed the composed buffer
  mBuf = 0;  // dont lock because main thread is waiting for us
  char strbuf[HWCVAL_DEFAULT_STRLEN];

  if (!mBufToCompare->HasRef()) {
    HWCLOGD("DoCompare returning, %s no ref buf", mBufToCompare->IdStr(strbuf));
    ClearLocked(mBufToCompare);
  } else {
    HWCLOGV("HwcTestCompValThread about to proceed with comparison");
    std::shared_ptr<DrmShimBuffer> buf = mBufToCompare;
    HWCNativeHandle bufCopy = buf->GetBufCopy();

    if (bufCopy) {
      buf->CompareWithRef(mUseAlpha, &mRectToCompare);

      if (buf->IsFbt()) {
        ++HwcGetTestResult()->mSfCompValCount;
      } else {
        ++HwcGetTestResult()->mHwcCompValCount;
      }
    } else {
      HWCLOGD("HwcTestCompValThread: Buffer copy failed, comparison skipped");
    }

    HWCLOGV("HwcTestCompValThread::DoCompare clearing mBufToCompare");
    ClearLocked(mBufToCompare);
  }
  HWCLOGD("HwcTestCompValThread::DoCompare Exit");
}

bool HwcTestCompValThread::IsBusy() {
  HWCVAL_LOCK(_l, mMutex);

  return ((mBufToCompose.get() != 0) || (mBufToCompare.get() != 0));
}

void HwcTestCompValThread::WaitUntilIdle() {
  // First wait to ensure any pending compositions have at least started
  usleep(200 * HWCVAL_USLEEP_1MS);  // 200ms

  // Now wait until Composition Validation thread has been idle for at least
  // 10ms.
  uint32_t idleCount = 0;

  while (idleCount < 10) {
    usleep(HWCVAL_USLEEP_1MS);

    if (IsBusy()) {
      idleCount = 0;
    } else {
      ++idleCount;
    }
  }
}

void HwcTestCompValThread::TakeCopy(std::shared_ptr<DrmShimBuffer> buf) {
  ATRACE_CALL();
  if (!buf->HasBufCopy()) {
    char strbuf[HWCVAL_DEFAULT_STRLEN];
    HWCLOGD("Taking copy (for transparency detection) of %s",
            buf->IdStr(strbuf));
    HWCNativeHandle bufCopy = CopyBuf(buf);
    buf->SetBufCopy(bufCopy);
  }
}

void HwcTestCompValThread::TakeTransformedCopy(const hwcval_layer_t* layer,
                                               std::shared_ptr<DrmShimBuffer> buf,
                                               uint32_t width,
                                               uint32_t height) {
  char strbuf[HWCVAL_DEFAULT_STRLEN];
  ATRACE_CALL();

  // Get destination graphic buffer
  HWCNativeHandle spDestBuffer;
  bufferHandler->CreateBuffer(width, height, buf->GetDrmFormat(),
                              &spDestBuffer);
  bufferHandler->CopyHandle(spDestBuffer, &spDestBuffer);
  bufferHandler->ImportBuffer(spDestBuffer);

  HWCLOGD("TakeTransformedCopy: %s", buf->IdStr(strbuf));
  hwcval_layer_t srcLayer = *layer;
  srcLayer.compositionType = HWC2_COMPOSITION_CLIENT;
  srcLayer.blending = HWC_BLENDING_NONE;

  hwcval_layer_t tgtLayer;
  tgtLayer.gralloc_handle = spDestBuffer;
  tgtLayer.compositionType = HWC2_COMPOSITION_CLIENT;
  tgtLayer.hints = 0;
  tgtLayer.flags = 0;
  tgtLayer.transform = 0;
  tgtLayer.blending = HWC_BLENDING_PREMULT;
  tgtLayer.sourceCropf.left = 0.0;
  tgtLayer.sourceCropf.top = 0.0;
  tgtLayer.sourceCropf.right = float(width);
  tgtLayer.sourceCropf.bottom = float(height);
  tgtLayer.displayFrame.left = 0;
  tgtLayer.displayFrame.top = 0;
  tgtLayer.displayFrame.right = width;
  tgtLayer.displayFrame.bottom = height;
  tgtLayer.visibleRegionScreen.numRects = 1;
  tgtLayer.visibleRegionScreen.rects = &srcLayer.displayFrame;
  tgtLayer.acquireFence = -1;
  tgtLayer.releaseFence = -1;
  tgtLayer.planeAlpha = 255;

  mComposer.Compose(1, &srcLayer, &tgtLayer, false);

  buf->SetBufCopy(spDestBuffer);
}

HWCNativeHandle HwcTestCompValThread::CopyBuf(
    std::shared_ptr<DrmShimBuffer> buf) {
  return mComposer.CopyBuf(buf->GetHandle());
}
