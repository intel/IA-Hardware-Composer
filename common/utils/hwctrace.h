/*
// Copyright (c) 2016 Intel Corporation
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

#ifndef HWC_TRACE_H
#define HWC_TRACE_H

#include <string>
#include <errno.h>
#include <stdio.h>
#include <chrono>
#include <time.h>

#include "displayplane.h"

#include <platformdefines.h>

#ifdef _cplusplus
extern "C" {
#endif

//#define ENABLE_DISPLAY_DUMP 1
//#define ENABLE_DISPLAY_MANAGER_TRACING 1
//#define ENABLE_PAGE_FLIP_EVENT_TRACING 1
//#define ENABLE_HOT_PLUG_EVENT_TRACING 1
//#define FUNCTION_CALL_TRACING 1
#define COMPOSITOR_TRACING 1

// Function call tracing
#ifdef FUNCTION_CALL_TRACING
class TraceFunc {
 public:
  TraceFunc(std::string func_name) {
    func_name_ = func_name;
    ITRACE("Calling ----- %s", func_name_.c_str());
    t_ = std::chrono::high_resolution_clock::now();
  }
  ~TraceFunc() {
    std::chrono::high_resolution_clock::time_point t2 =
        std::chrono::high_resolution_clock::now();
    ITRACE(
        "Total time spent in --- %s Time(msec): %lld", func_name_.c_str(),
        std::chrono::duration_cast<std::chrono::microseconds>(t2 - t_).count());
    ITRACE("Leaving --- %s", func_name_.c_str());
  }

 private:
  std::chrono::high_resolution_clock::time_point t_;
  std::string func_name_;
};
#define CTRACE() TraceFunc hwctrace(__func__);
#else
#define CTRACE() ((void)0)
#endif

// Arguments tracing
#if 0
#define ATRACE(fmt, ...) VTRACE("%s(args): " fmt, __func__, ##__VA_ARGS__);
#else
#define ATRACE(fmt, ...) ((void)0)
#endif

// Useful Debug tracing
#ifdef ENABLE_DISPLAY_DUMP
#define DUMPTRACE ITRACE
#else
#define DUMPTRACE(fmt, ...) ((void)0)
#endif

// Page Flip event tracing
#ifdef ENABLE_PAGE_FLIP_EVENT_TRACING
#define IPAGEFLIPEVENTTRACE ITRACE
#else
#define IPAGEFLIPEVENTTRACE(fmt, ...) ((void)0)
#endif

#ifdef ENABLE_DISPLAY_MANAGER_TRACING
#define IDISPLAYMANAGERTRACE ITRACE
#else
#define IDISPLAYMANAGERTRACE(fmt, ...) ((void)0)
#endif

#ifdef ENABLE_HOT_PLUG_EVENT_TRACING
#define IHOTPLUGEVENTTRACE ITRACE
#else
#define IHOTPLUGEVENTTRACE(fmt, ...) ((void)0)
#endif

#ifdef COMPOSITOR_TRACING
#define ICOMPOSITORTRACE ITRACE
#else
#define ICOMPOSITORTRACE(fmt, ...) ((void)0)
#endif

// Errors
#define PRINTERROR() strerror(-errno)

// Helper to abort the execution if object is not initialized.
// This should never happen if the rules below are followed during design:
// 1) Create an object.
// 2) Initialize the object immediately.
// 3) If failed, delete the object.
// These helpers should be disabled and stripped out of release build

#define RETURN_X_IF_NOT_INIT(X)                                              \
  do {                                                                       \
    CTRACE();                                                                \
    if (false == mInitialized) {                                             \
      LOG_ALWAYS_FATAL("%s: Object is not initialized! Line = %d", __func__, \
                       __LINE__);                                            \
      return X;                                                              \
    }                                                                        \
  } while (0)

#if 1
#define RETURN_FALSE_IF_NOT_INIT() RETURN_X_IF_NOT_INIT(false)
#define RETURN_VOID_IF_NOT_INIT() RETURN_X_IF_NOT_INIT()
#define RETURN_NULL_IF_NOT_INIT() RETURN_X_IF_NOT_INIT(0)
#else
#define RETURN_FALSE_IF_NOT_INIT() ((void)0)
#define RETURN_VOID_IF_NOT_INIT() ((void)0)
#define RETURN_NULL_IF_NOT_INIT() ((void)0)
#endif

// Helper to log error message, call de-initializer and return false.
#define DEINIT_AND_RETURN_FALSE(...) \
  do {                               \
    ETRACE(__VA_ARGS__);             \
    deinitialize();                  \
    return false;                    \
  } while (0)

#define DEINIT_AND_DELETE_OBJ(X) \
  if (X) {                       \
    X->deinitialize();           \
    delete X;                    \
    X = NULL;                    \
  }

#define WARN_IF_NOT_DEINIT()                                                 \
  CTRACE();                                                                  \
  if (mInitialized) {                                                        \
    LOG_ALWAYS_FATAL("%s: Object is not deinitialized! Line = %d", __func__, \
                     __LINE__);                                              \
  }

#ifdef ENABLE_DISPLAY_DUMP
#define DUMP_CURRENT_COMPOSITION_PLANES()                                  \
  frame_++;                                                                \
  DUMPTRACE(                                                               \
      "Dumping DisplayPlaneState of Current Composition planes "           \
      "-----------------------------");                                    \
  DUMPTRACE("Frame: %d", frame_);                                          \
  DUMPTRACE("Total Layers for this Frame: %d", layers.size());             \
  DUMPTRACE("Total Planes in use for this Frame: %d",                      \
            current_composition_planes.size());                            \
  int plane_index = 1;                                                     \
  for (DisplayPlaneState & comp_plane : current_composition_planes) {      \
    DUMPTRACE("Composition Plane State for Index: %d", plane_index);       \
    const std::vector<size_t> &source_layers = comp_plane.source_layers(); \
    switch (comp_plane.GetCompositionState()) {                            \
      case DisplayPlaneState::State::kRender:                              \
        DUMPTRACE("DisplayPlane state: kRender. Total layers: %lu",        \
                  source_layers.size());                                   \
        DUMPTRACE("Layers Index:");                                        \
        for (size_t primary_index : source_layers) {                       \
          DUMPTRACE("index: %d", primary_index);                           \
          layers.at(primary_index).Dump();                                 \
        }                                                                  \
        break;                                                             \
      case DisplayPlaneState::State::kScanout:                             \
        if (source_layers.size() > 1)                                      \
          DUMPTRACE(                                                       \
              "Plane has more than one layer associated when its type is " \
              "kScanout. This needs to be fixed.");                        \
        DUMPTRACE("DisplayPlane State: kScanout. Total layers: %lu",       \
                  source_layers.size());                                   \
        DUMPTRACE("Layers Index:");                                        \
        for (size_t overlay_index : source_layers) {                       \
          DUMPTRACE("index: %d", overlay_index);                           \
          layers.at(overlay_index).Dump();                                 \
        }                                                                  \
        break;                                                             \
      default:                                                             \
        break;                                                             \
    }                                                                      \
    comp_plane.plane()->Dump();                                            \
    DUMPTRACE("Composition Plane State ends for Index: %d", plane_index);  \
    plane_index++;                                                         \
  }                                                                        \
  DUMPTRACE(                                                               \
      "Dumping DisplayPlaneState of Current Composition planes ends. "     \
      "-----------------------------");
#else
#define DUMP_CURRENT_COMPOSITION_PLANES() ((void)0)
#endif

// _cplusplus
#ifdef _cplusplus
}
#endif

#endif /* HWC_TRACE_H */
