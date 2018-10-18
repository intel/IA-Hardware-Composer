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

#include "HwchInputGenerator.h"
#include "HwcTestState.h"

#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

Hwch::InputGenerator::InputGenerator()
    : hwcomposer::HWCThread( android::PRIORITY_NORMAL, "HwchInputGenerator"), \
      mRunning(false), mActive(false), mKeypressFailed(false) {
  mRunning = true;
  Open();
}

Hwch::InputGenerator::~InputGenerator() {
}

void Hwch::InputGenerator::Open() {
  mFd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  ALOG_ASSERT(mFd > 0);

  int ret = ioctl(mFd, UI_SET_EVBIT, EV_KEY);
  if (ret) {
    HWCERROR(eCheckInternalError,
             "Hwch::InputGenerator::Keypress ioctl UI_SET_EVBIT returned %d",
             ret);
    mKeypressFailed = true;
    return;
  }

  ret = ioctl(mFd, UI_SET_KEYBIT, KEY_A);
  if (ret) {
    HWCERROR(eCheckInternalError,
             "Hwch::InputGenerator::Keypress ioctl UI_SET_KEYBIT returned %d",
             ret);
    mKeypressFailed = true;
    return;
  }

  struct uinput_user_dev uidev;

  memset(&uidev, 0, sizeof(uidev));

  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-sample");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1234;
  uidev.id.product = 0xfedc;
  uidev.id.version = 1;

  ret = write(mFd, &uidev, sizeof(uidev));
  if (ret < 0) {
    mKeypressFailed = true;
    HWCERROR(
        eCheckInternalError,
        "Hwch::InputGenerator::Keypress Failed to write uinput_user_dev (%d)",
        ret);
    return;
  }

  ret = ioctl(mFd, UI_DEV_CREATE);
  if (ret) {
    mKeypressFailed = true;
    HWCERROR(eCheckInternalError,
             "Hwch::InputGenerator::Keypress failed to create spoof keypress "
             "device (%d)",
             ret);
    return;
  }
}

void Hwch::InputGenerator::Keypress() {
  if (mKeypressFailed) {
    return;
  }

  int ret = 0;

  if (mFd == 0) {
    Open();
  }

  // Key down
  WriteEvent(EV_KEY, KEY_A, 1);

  // Synchronize
  WriteEvent(EV_SYN, 0, 0);

  // Key up
  WriteEvent(EV_KEY, KEY_A, 0);

  // Synchronize
  WriteEvent(EV_SYN, 0, 0);
}

int Hwch::InputGenerator::WriteEvent(int type, int code, int value) {
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));

  ev.type = type;
  ev.code = code;
  ev.value = value;

  int ret = write(mFd, &ev, sizeof(ev));

  if (ret < 0) {
    HWCERROR(eCheckInternalError,
             "Hwch::InputGenerator::Keypress failed to write type %d code %d "
             "value %d",
             type, code, value);
  } else {
    HWCLOGV_COND(eLogVideo,
                 "Hwch::InputGenerator::Keypress wrote %d bytes (%d %d %d)",
                 ret, type, code, value);
  }

  return ret;
}

// Set mode to input active,
void Hwch::InputGenerator::SetActive(bool active) {
  if (active) {
    Keypress();

    if (!mRunning) {
      Resume();
      mRunning = true;
    }
  } else {
    if (mActive) {
      mInactiveTime = systemTime(SYSTEM_TIME_MONOTONIC) +
                      (HWCVAL_US_TO_NS * mTimeoutPeriodUs);
      HWCLOGD_COND(eLogVideo,
                   "Stopping keypress generation. input timeout stability "
                   "expected after %dus at %f",
                   mTimeoutPeriodUs, double(mInactiveTime) / HWCVAL_SEC_TO_NS);
    }
  }

  mActive = active;
}

void Hwch::InputGenerator::Stabilize() {
  if (!mActive) {
    if (mInactiveTime) {
      int64_t t = systemTime(SYSTEM_TIME_MONOTONIC);
      int us = int((mInactiveTime - t) / HWCVAL_US_TO_NS);

      if (us > 0) {
        HWCLOGD_COND(eLogVideo, "Waiting %dus until stability at %f", us,
                     double(mInactiveTime) / HWCVAL_SEC_TO_NS);
        usleep(us);
      }
    }
  }
}

void Hwch::InputGenerator::SetActiveAndWait(bool active) {
  SetActive(active);
  Stabilize();
}

// Thread functions
void Hwch::InputGenerator::HandleRoutine() {

    if (mActive) {
      Keypress();
    }

    usleep(mKeypressIntervalUs);
}

const uint32_t Hwch::InputGenerator::mKeypressIntervalUs = 1 * HWCVAL_SEC_TO_US;
const uint32_t Hwch::InputGenerator::mTimeoutPeriodUs = 4 * HWCVAL_SEC_TO_US;
