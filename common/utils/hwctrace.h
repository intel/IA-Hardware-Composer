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

#ifndef COMMON_UTILS_HWCTRACE_H_
#define COMMON_UTILS_HWCTRACE_H_

#include <vector>
#include <string>
#include <chrono>

#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "displayplane.h"
#include "platformdefines.h"

#ifdef _cplusplus
extern "C" {
#endif

// #define ENABLE_DISPLAY_DUMP 1
// #define ENABLE_DISPLAY_MANAGER_TRACING 1
// #define ENABLE_PAGE_FLIP_EVENT_TRACING 1
// #define ENABLE_HOT_PLUG_EVENT_TRACING 1
// #define ENABLE_MOSAIC_DISPLAY_TRACING 1
// #define FUNCTION_CALL_TRACING 1
// #define RESOURCE_CACHE_TRACING 1
// #define SURFACE_PLANE_LAYER_MAP_TRACING 1
// #define SURFACE_DUPLICATE_LAYER_TRACING 1
// #define SURFACE_BASIC_TRACING 1
// #define COMPOSITOR_TRACING 1

// Function call tracing
#ifdef FUNCTION_CALL_TRACING
class TraceFunc {
 public:
  explicit TraceFunc(std::string func_name) {
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
#define CTRACE()  STRACE()
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

#ifdef ENABLE_MOSAIC_DISPLAY_TRACING
#define IMOSAICDISPLAYTRACE ITRACE
#else
#define IMOSAICDISPLAYTRACE(fmt, ...) ((void)0)
#endif

#ifdef COMPOSITOR_TRACING
#define ICOMPOSITORTRACE ITRACE
#else
#define ICOMPOSITORTRACE(fmt, ...) ((void)0)
#endif

#ifdef RESOURCE_CACHE_TRACING
#define ICACHETRACE ITRACE
#else
#define ICACHETRACE ((void)0)
#endif

#ifdef SURFACE_BASIC_TRACING
#define ISURFACETRACE ITRACE
#define SURFACE_TRACING 1
#else
#define ISURFACETRACE ((void)0)
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
#define DUMP_CURRENT_COMPOSITION_PLANES()                                    \
  static int frame_ = 0;                                                     \
  frame_++;                                                                  \
  DUMPTRACE(                                                                 \
      "Dumping DisplayPlaneState of Current Composition planes "             \
      "-----------------------------");                                      \
  DUMPTRACE("Frame: %d", frame_);                                            \
  DUMPTRACE("Total Layers for this Frame: %d", layers.size());               \
  DUMPTRACE("Total Planes in use for this Frame: %d",                        \
            current_composition_planes.size());                              \
  int plane_index = 1;                                                       \
  for (DisplayPlaneState & comp_plane : current_composition_planes) {        \
    DUMPTRACE("Composition Plane State for Index: %d", plane_index);         \
    const std::vector<size_t> &source_layers = comp_plane.GetSourceLayers(); \
    if (comp_plane.NeedsOffScreenComposition()) {                            \
      DUMPTRACE("DisplayPlane state: kRender. Total layers: %lu",            \
                source_layers.size());                                       \
      DUMPTRACE("Layers Index:");                                            \
      for (size_t primary_index : source_layers) {                           \
        DUMPTRACE("index: %d", primary_index);                               \
        layers.at(primary_index).Dump();                                     \
      }                                                                      \
    } else if (comp_plane.Scanout()) {                                       \
      if (source_layers.size() > 1 && !comp_plane.IsSurfaceRecycled())       \
        DUMPTRACE(                                                           \
            "Plane has more than one layer associated when its type is "     \
            "kScanout. This needs to be fixed.");                            \
      DUMPTRACE("DisplayPlane State: kScanout. Total layers: %lu",           \
                source_layers.size());                                       \
      DUMPTRACE("Layers Index:");                                            \
      for (size_t overlay_index : source_layers) {                           \
        DUMPTRACE("index: %d", overlay_index);                               \
        layers.at(overlay_index).Dump();                                     \
      }                                                                      \
    }                                                                        \
    comp_plane.GetDisplayPlane()->Dump();                                    \
    DUMPTRACE("Composition Plane State ends for Index: %d\n", plane_index);  \
    plane_index++;                                                           \
  }                                                                          \
  DUMPTRACE(                                                                 \
      "Dumping DisplayPlaneState of Current Composition planes ends. "       \
      "-----------------------------\n");
#else
#define DUMP_CURRENT_COMPOSITION_PLANES() ((void)0)
#endif

#ifdef SURFACE_PLANE_LAYER_MAP_TRACING
#define DUMP_CURRENT_LAYER_PLANE_COMBINATIONS()                                \
  static int layer_frame_ = 0;                                                 \
  layer_frame_++;                                                              \
  ITRACE(                                                                      \
      "Dumping Layers of Current Composition planes "                          \
      "-----------------------------");                                        \
  ITRACE("Frame: %d", layer_frame_);                                           \
  ITRACE("Total Layers for this Frame: %d", layers.size());                    \
  ITRACE("Total Planes in use for this Frame: %d",                             \
         current_composition_planes.size());                                   \
  int plane_layer_index = 1;                                                   \
  for (DisplayPlaneState & comp_plane : current_composition_planes) {          \
    ITRACE("Composition Plane State for Index: %d", plane_layer_index);        \
    const std::vector<size_t> &source_layers = comp_plane.GetSourceLayers();   \
    if (!comp_plane.GetDisplayPlane()->InUse()) {                              \
      ITRACE("ALERT: Plane Disabled");                                         \
      continue;                                                                \
    }                                                                          \
    if (comp_plane.NeedsOffScreenComposition()) {                              \
      ITRACE("DisplayPlane state: kRender. Total layers: %lu",                 \
             source_layers.size());                                            \
      ITRACE("Layers Index:");                                                 \
      for (const size_t &primary_index : source_layers) {                      \
        ITRACE("index: %d", primary_index);                                    \
      }                                                                        \
    } else if (comp_plane.Scanout()) {                                         \
      if (source_layers.size() > 1 && !comp_plane.IsSurfaceRecycled())         \
        ITRACE(                                                                \
            "Plane has more than one layer associated when its type is "       \
            "kScanout. This needs to be fixed.");                              \
      ITRACE("DisplayPlane State: kScanout. Total layers: %lu",                \
             source_layers.size());                                            \
      ITRACE("Layers Index:");                                                 \
      for (const size_t &overlay_index : source_layers) {                      \
        ITRACE("index: %d", overlay_index);                                    \
      }                                                                        \
    }                                                                          \
    ITRACE("Composition Plane State ends for Index: %d\n", plane_layer_index); \
    plane_layer_index++;                                                       \
  }                                                                            \
  ITRACE(                                                                      \
      "Dumping Layers of Current Composition planes ends. "                    \
      "-----------------------------\n");
#else
#define DUMP_CURRENT_LAYER_PLANE_COMBINATIONS() ((void)0)
#endif

#ifdef SURFACE_DUPLICATE_LAYER_TRACING
#define DUMP_CURRENT_DUPLICATE_LAYER_COMBINATIONS()                           \
  int plane_dup_layer_index = 1;                                              \
  ITRACE("Checking for duplicate layers Within a Plane: \n");                 \
  bool duplicate_found = false;                                               \
  for (DisplayPlaneState & comp_plane : current_composition_planes) {         \
    const std::vector<size_t> &source_layers = comp_plane.GetSourceLayers();  \
    std::vector<size_t> temp;                                                 \
    for (size_t i = 0; i < source_layers.size(); i++) {                       \
      bool found = false;                                                     \
      size_t current_index = source_layers.at(i);                             \
      if (!temp.empty()) {                                                    \
        for (size_t j = 0; j < temp.size(); j++) {                            \
          if (temp.at(j) == current_index) {                                  \
            found = true;                                                     \
            duplicate_found = true;                                           \
            ITRACE(                                                           \
                "ALERT: Same Layer added again for this plane. Plane Index: " \
                "%d "                                                         \
                "Layer "                                                      \
                "Index: %d \n",                                               \
                plane_dup_layer_index, current_index);                        \
            break;                                                            \
          }                                                                   \
        }                                                                     \
      }                                                                       \
      if (!found) {                                                           \
        temp.emplace_back(current_index);                                     \
      }                                                                       \
    }                                                                         \
    plane_dup_layer_index++;                                                  \
  }                                                                           \
  if (!duplicate_found) {                                                     \
    ITRACE("No duplicate layers present within a plane. \n");                 \
  }                                                                           \
  ITRACE("Checking for duplicate layers Within a Plane Ends. \n");            \
  ITRACE("Checking for duplicate layers between different Planes. \n");       \
  duplicate_found = false;                                                    \
  std::vector<std::vector<size_t>> test_layers;                               \
  for (DisplayPlaneState & comp_plane : current_composition_planes) {         \
    const std::vector<size_t> &source_layers = comp_plane.GetSourceLayers();  \
    test_layers.emplace_back();                                               \
    std::vector<size_t> &temp = test_layers.back();                           \
    for (size_t i = 0; i < source_layers.size(); i++) {                       \
      temp.emplace_back(source_layers.at(i));                                 \
    }                                                                         \
  }                                                                           \
  for (size_t i = 0; i < test_layers.size(); i++) {                           \
    const std::vector<size_t> &temp2 = test_layers.at(i);                     \
    for (size_t k = 0; k < current_composition_planes.size(); k++) {          \
      if (i == k) {                                                           \
        continue;                                                             \
      }                                                                       \
      const std::vector<size_t> &source_layers =                              \
          current_composition_planes.at(k).GetSourceLayers();                 \
      for (size_t z = 0; z < source_layers.size(); z++) {                     \
        size_t current_index = source_layers.at(z);                           \
        for (size_t j = 0; j < temp2.size(); j++) {                           \
          if (temp2.at(j) == current_index) {                                 \
            duplicate_found = true;                                           \
            ITRACE(                                                           \
                "ALERT: Same Layer added in more than one plane. Plane "      \
                "Index1: %d "                                                 \
                "Plane Index2 %d "                                            \
                "Layer Index: %d \n",                                         \
                i, k, current_index);                                         \
          }                                                                   \
        }                                                                     \
      }                                                                       \
    }                                                                         \
  }                                                                           \
  if (!duplicate_found) {                                                     \
    ITRACE("No duplicate layers present between different planes. \n");       \
  }                                                                           \
  ITRACE("Checking for duplicate layers between different Plane Ends. \n");   \
  ITRACE("Checking if we missed rendering any layers for this frame. \n");    \
  std::vector<size_t> total_layers;                                           \
  std::vector<size_t> missed_layers;                                          \
  for (DisplayPlaneState & comp_plane : current_composition_planes) {         \
    const std::vector<size_t> &source_layers = comp_plane.GetSourceLayers();  \
    for (size_t i = 0; i < source_layers.size(); i++) {                       \
      total_layers.emplace_back(source_layers.at(i));                         \
    }                                                                         \
  }                                                                           \
  for (size_t i = 0; i < layers.size(); i++) {                                \
    bool found = false;                                                       \
    size_t current_index = static_cast<size_t>(layers.at(i).GetZorder());     \
    for (size_t j = 0; j < total_layers.size(); j++) {                        \
      if (total_layers.at(j) == current_index) {                              \
        found = true;                                                         \
        break;                                                                \
      }                                                                       \
    }                                                                         \
    if (!found) {                                                             \
      missed_layers.emplace_back(current_index);                              \
    }                                                                         \
  }                                                                           \
  if (missed_layers.empty()) {                                                \
    ITRACE("We handled all layers valid for this frame. \n");                 \
  } else {                                                                    \
    for (size_t z = 0; z < missed_layers.size(); z++) {                       \
      size_t current_index = missed_layers.at(z);                             \
      ITRACE("ALERT: Missed layer with index %d for this frame. \n",          \
             current_index);                                                  \
    }                                                                         \
  }                                                                           \
  ITRACE(                                                                     \
      "Finished Checking if we missed rendering any layers for this frame. "  \
      "\n");
#else
#define DUMP_CURRENT_DUPLICATE_LAYER_COMBINATIONS() ((void)0)
#endif

// _cplusplus
#ifdef _cplusplus
}
#endif

#endif  // COMMON_UTILS_HWCTRACE_H_
