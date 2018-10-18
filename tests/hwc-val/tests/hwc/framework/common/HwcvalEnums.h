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

#ifndef __HwcvalEnums_h__
#define __HwcvalEnums_h__

namespace Hwcval {
enum class CompositionType  // STRONGLY TYPED ENUM
{ UNKNOWN,
  SF,   // Composition type Surface Flinger. i.e. FB
  HWC,  // Composition type Hardware Composer. i.e. OV
  TGT   // Composition type Target, i.e. TG/FBT.
};

enum class FrameBufferDrawnType { NotDrawn = 0, OnScreen, ThisFrame };

enum { eNoBufferType = 999 };

enum class BufferContentType {
  ContentNotTested = 0,
  ContentNull,
  ContentNotNull
};

enum class ValidityType {
  Invalid = 0,
  InvalidWithinTimeout,
  Invalidating,
  ValidUntilModeChange,
  Valid,
  Indeterminate
};

enum class BufferSourceType {
  Input,
  SfComp,  // Surface flinger composition target
  PartitionedComposer,
  Writeback,
  Hwc,        // Blanking buffers, or anything else invented by HWC
  Validation  // Anything we have dreamed up ourselves e.g. for composition
              // validation
};

enum { eDisplayIxFixed = 0, eDisplayIxHdmi = 1, eDisplayIxVirtual = 2 };
}

#endif  // __HwcvalEnums_h__
