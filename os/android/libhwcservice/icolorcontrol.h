/*
// Copyright (c) 2017 Intel Corporation
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

#ifndef OS_ANDROID_HWC_ICOLOR_CONTROL_H
#define OS_ANDROID_HWC_ICOLOR_CONTROL_H

#include <binder/IInterface.h>
#include <binder/Parcel.h>

namespace hwcomposer {


using namespace android;


/**
 * Allows tuning display color
 */
class IColorControl : public IInterface
{
public:
    DECLARE_META_INTERFACE(ColorControl);

    virtual status_t restoreDefault() = 0;
    virtual status_t getColorParam(int32_t *value, int32_t *startvalue, int32_t *endvalue) = 0;
    virtual status_t setColorParam(int32_t value) = 0;
};


/**
 */
class BnColorControl : public BnInterface<IColorControl>
{
public:
    virtual status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t);
};


} // namespace services

#endif // OS_ANDROID_HWC_IDISPLAY_CONTROL_H
