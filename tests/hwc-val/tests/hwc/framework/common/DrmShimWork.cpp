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

#include "DrmShimWork.h"
#include "DrmShimChecks.h"

// AddFbItem
Hwcval::Work::AddFbItem::AddFbItem(int fd, uint32_t boHandle, uint32_t fbId,
                                   uint32_t width, uint32_t height,
                                   uint32_t pixelFormat)
    : Item(fd),
      mBoHandle(boHandle),
      mFbId(fbId),
      mWidth(width),
      mHeight(height),
      mPixelFormat(pixelFormat),
      mAuxPitch(0),
      mAuxOffset(0) {
  mHasAuxBuffer = false;
}

Hwcval::Work::AddFbItem::AddFbItem(int fd, uint32_t boHandle, uint32_t fbId,
                                   uint32_t width, uint32_t height,
                                   uint32_t pixelFormat, uint32_t auxPitch,
                                   uint32_t auxOffset, __u64 modifier)
    : Item(fd),
      mBoHandle(boHandle),
      mFbId(fbId),
      mWidth(width),
      mHeight(height),
      mPixelFormat(pixelFormat),
      mAuxPitch(auxPitch),
      mAuxOffset(auxOffset),
      mModifier(modifier) {
  mHasAuxBuffer = true;
}

Hwcval::Work::AddFbItem::~AddFbItem() {
}

void Hwcval::Work::AddFbItem::Process() {
  DrmShimChecks* checks =
      static_cast<DrmShimChecks*>(HwcTestState::getInstance()->GetTestKernel());
  checks->DoWork(*this);
}

// RmFbItem
Hwcval::Work::RmFbItem::RmFbItem(int fd, uint32_t fbId)
    : Item(fd), mFbId(fbId) {
}

Hwcval::Work::RmFbItem::~RmFbItem() {
}

void Hwcval::Work::RmFbItem::Process() {
  DrmShimChecks* checks =
      static_cast<DrmShimChecks*>(HwcTestState::getInstance()->GetTestKernel());
  checks->DoWork(*this);
}
