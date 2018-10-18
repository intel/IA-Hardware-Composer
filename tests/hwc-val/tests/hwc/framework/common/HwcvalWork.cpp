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

#include "HwcvalWork.h"
#include "HwcTestKernel.h"
#include "HwcTestState.h"

#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
static uint32_t itemCount = 0;
#endif

Hwcval::Work::Item::Item(int fd) : mFd(fd) {
#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
  if (++itemCount > 500) {
    HWCLOGW("%d work items in transit", itemCount);
  }
#endif
}

Hwcval::Work::Item::~Item() {
#ifdef HWCVAL_RESOURCE_LEAK_CHECKING
  --itemCount;
#endif
}

// GemOpenItem
Hwcval::Work::GemOpenItem::GemOpenItem(int fd, int id, uint32_t boHandle)
    : Item(fd), mId(id), mBoHandle(boHandle) {
}

Hwcval::Work::GemOpenItem::~GemOpenItem() {
}

void Hwcval::Work::GemOpenItem::Process() {
  HwcTestState::getInstance()->GetTestKernel()->DoGem(*this);
}

// GemCloseItem
Hwcval::Work::GemCloseItem::GemCloseItem(int fd, uint32_t boHandle)
    : Item(fd), mBoHandle(boHandle) {
}

Hwcval::Work::GemCloseItem::~GemCloseItem() {
}

void Hwcval::Work::GemCloseItem::Process() {
  HwcTestState::getInstance()->GetTestKernel()->DoGem(*this);
}

// GemCreateItem
Hwcval::Work::GemCreateItem::GemCreateItem(int fd, uint32_t boHandle)
    : Item(fd), mBoHandle(boHandle) {
}

Hwcval::Work::GemCreateItem::~GemCreateItem() {
}

void Hwcval::Work::GemCreateItem::Process() {
  HwcTestState::getInstance()->GetTestKernel()->DoGem(*this);
}

Hwcval::Work::GemWaitItem::GemWaitItem(int fd, uint32_t boHandle,
                                       int32_t status, int64_t delayNs)
    : Item(fd), mBoHandle(boHandle), mStatus(status), mDelayNs(delayNs) {
}

Hwcval::Work::GemWaitItem::~GemWaitItem() {
}

void Hwcval::Work::GemWaitItem::Process() {
  HwcTestState::getInstance()->GetTestKernel()->DoGem(*this);
}

// PrimeItem
Hwcval::Work::PrimeItem::PrimeItem(int fd, uint32_t boHandle, int dmaHandle)
    : Item(fd), mBoHandle(boHandle), mDmaHandle(dmaHandle) {
}

Hwcval::Work::PrimeItem::~PrimeItem() {
}

void Hwcval::Work::PrimeItem::Process() {
  HwcTestState::getInstance()->GetTestKernel()->DoPrime(*this);
}

// Queue
Hwcval::Work::Queue::Queue() : EventQueue("Hwcval::Work::Queue") {
}

Hwcval::Work::BufferFreeItem::BufferFreeItem(HWCNativeHandle handle)
    : Item(0), mHandle(handle) {
}

Hwcval::Work::BufferFreeItem::~BufferFreeItem() {
}

void Hwcval::Work::BufferFreeItem::Process() {
  HwcTestState::getInstance()->GetTestKernel()->DoBufferFree(*this);
}

Hwcval::Work::Queue::~Queue() {
}

void Hwcval::Work::Queue::Process() {
  ATRACE_CALL();
  std::shared_ptr<Item> item;

  while (Pop(item)) {
    item->Process();
  }
};
