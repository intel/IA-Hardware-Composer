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

#ifndef OS_ANDROID_HWC_IVIDEO_CONTROL_H
#define OS_ANDROID_HWC_IVIDEO_CONTROL_H

#include "hwcservicehelper.h"
#include <utils/RefBase.h>
#include <binder/IInterface.h>

namespace hwcomposer {

/**
 * Allows control of Video processing
 * DEPRECATED: This is now a compataibilty layer over the supported API
 * and will be removed!  NO additional entry points should be added here.
 */
class IVideoControl : public android::IInterface
{
public:
    DECLARE_META_INTERFACE(VideoControl);

    // The control enables a the protected video subsystem to control when to replace any
    // encrypted content with a default bitmap (usually black).

    // Enable the display of encrypted buffers with the specified sessionID and instanceID.
    // This will take effect from the next composed frame.
    // Any previously enabled instanceID will be disabled (replaced by the default image)
    status_t enableEncryptedSession( uint32_t sessionID, uint32_t instanceID )
    {
        return HwcService_Video_EnableEncryptedSession(mHwcConn, sessionID, instanceID);
    }

    // Disable specific encrypted session.
    // This call will trigger the HWC to remove any encrypted buffers with the specified sessionID
    // from the screen and replace with a default image.
    // The function will block until the screen no longer contains any encrypted data with this session.
    // This should be called by any subsystem that knows that a specific encrypted video session is about to
    // become invalid.
    status_t disableEncryptedSession( uint32_t sessionID )
    {
        return HwcService_Video_DisableEncryptedSession(mHwcConn, sessionID);
    }

    // Disable all protected sessions.
    // This call will trigger the HWC to remove any encrypted buffers from the screen and replace
    // with a default image.
    // The function will block until the screen no longer contains any encrypted data with any session.
    // This should be called by any subsystem that knows that all encrypted video sessions are about to
    // become invalid.
    status_t disableAllEncryptedSessions( )
    {
        return HwcService_Video_DisableAllEncryptedSessions(mHwcConn);
    }

    // Return whether or not the specified session/instance is enabled.
    bool isEncryptedSessionEnabled( uint32_t sessionID, uint32_t instanceID )
    {
        return HwcService_Video_IsEncryptedSessionEnabled(mHwcConn, sessionID, instanceID);
    }

    enum EDisplayId
    {
        eWired,
        eWireless,
    };

    enum EDisplayStatus
    {
        eInsecure,
        eSecure,
    };

    // Update the protection status of a display.
    status_t updateStatus( EDisplayId, EDisplayStatus )
    {
        // NOT IMPLEMENTED IN LEGACY API WRAPPER
        return -1;
    }

    // Hint provided by the application about the global optimization mode for the driver
    enum EOptimizationMode
    {
        eNormal = HWCS_OPTIMIZE_NORMAL,
        eVideo = HWCS_OPTIMIZE_VIDEO,
        eCamera = HWCS_OPTIMIZE_CAMERA,
    };
    status_t setOptimizationMode(EOptimizationMode mode)
    {
        return HwcService_Video_SetOptimizationMode(mHwcConn, (EHwcsOptimizationMode)mode);
    }

private:
    HwcServiceConnection mHwcConn;
};

class BnVideoControl : public android::BnInterface<IVideoControl>
{
public:
    /*virtual status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t)
    {
        return BBinder::onTransact(code, data, reply, flags);
    }*/
};

} // namespace services

#endif // OS_ANDROID_HWC_IVIDEO_CONTROL_H
