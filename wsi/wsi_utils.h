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

#ifndef WSI_UTILS_H__
#define WSI_UTILS_H__

#ifndef DISABLE_HOTPLUG_NOTIFICATION
#define SPIN_LOCK(X) X.lock();
#else
#define SPIN_LOCK(X) ((void)0)
#endif

#ifndef DISABLE_HOTPLUG_NOTIFICATION
#define SPIN_UNLOCK(X) X.unlock();
#else
#define SPIN_UNLOCK(X) ((void)0)
#endif

#endif  // WSI_UTILS_H__
