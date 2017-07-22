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


#ifndef OS_ANDROID_HWC_ABSTRACTCOMPOSITIONCHECKER_H
#define OS_ANDROID_HWC_ABSTRACTCOMPOSITIONCHECKER_H

#include <hardware/hwcomposer.h>

namespace hwcomposer {
namespace validation {

// HWC supports version 0 of AbstractCompositionChecker interface
#define ABSTRACTCOMPOSITIONCHECKER_VERSION_SUPPORT_MASK (1<<0)

// This interface is for process internal use only hence not subclass of IInterface.
class AbstractCompositionChecker
{
public:
    class ValLayer : public hwc_layer_1_t
    {
    };

    virtual ~AbstractCompositionChecker()
    {
    }

    // Composition check - Initiate.
    // Returns a context, or null to indicate that this composition is not
    // required by the validation interface.
    virtual void* CreateContext(const char* composer) = 0;

    // Add a source layer to the validation context.
    virtual void AddSource(void* ctx, const ValLayer& layer, const char* debug) = 0;

    // Add the target layer to the validation context, perform the validation,
    // and close the context.
    virtual void CheckComposition(void* ctx, const ValLayer& layer, const char* debug) = 0;

};

}   // validation
}   // hwcomposer

#endif // OS_ANDROID_HWC_ABSTRACTCOMPOSITIONCHECKER_H
