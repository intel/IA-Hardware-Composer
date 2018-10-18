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

#include <iomanip>

#include "re2/re2.h"
#include "re2/stringpiece.h"
#include "util/util.h"

#include "HwchFrame.h"
#include "HwchLayer.h"
#include "HwchPattern.h"
#include "HwcTestState.h"
#include "HwcTestLog.h"
#include "HwchReplayParser.h"

Hwch::ReplayParser::ReplayParser()
    : hwcl_onset_regex(hwcl_onset_string),
      hwcl_onset_1533_regex(hwcl_onset_1533_string),
      hwcl_layer_regex_hdr(hwcl_layer_string_hdr),
      hwcl_layer_1533_regex_hdr(hwcl_layer_1533_string_hdr),
      hwcl_layer_regex_vbr(hwcl_layer_string_vbr),
      hwcl_layer_regex_trl(hwcl_layer_string_trl),
      ds_display_regex(ds_display_string),
      ds_layer_regex(ds_layer_string),
      hotplug_connected_regex(hotplug_connected_string),
      hotplug_disconnected_regex(hotplug_disconnected_string),
      blank_regex(blank_string),
      unblank_regex(unblank_string) {
  // Check that the regex compilation was successful
  if (!hwcl_onset_regex.ok() || !hwcl_onset_1533_regex.ok() ||
      !hwcl_layer_regex_hdr.ok() || !hwcl_layer_1533_regex_hdr.ok() ||
      !hwcl_layer_regex_vbr.ok() || !hwcl_layer_regex_trl.ok() ||
      !ds_display_regex.ok() || !ds_layer_regex.ok()) {
    HWCERROR(eCheckInternalError, "Fatal error compiling regular expressions");
    mRegexCompilationSuccess = false;
  } else {
    mRegexCompilationSuccess = true;
  }
}

bool Hwch::ReplayParser::ParseHwclLayerIndex(const std::string& str,
                                             uint32_t& val) {
  return RE2::PartialMatch(str, hwcl_layer_regex_hdr, &val) ||
         RE2::PartialMatch(str, hwcl_layer_1533_regex_hdr, &val);
}

bool Hwch::ReplayParser::ParseHwclLayerHandle(const std::string& str,
                                              uint64_t& val) {
  void* np = NULL;

  return RE2::PartialMatch(str, hwcl_layer_regex_hdr, np, np, RE2::Hex(&val)) ||
         RE2::PartialMatch(str, hwcl_layer_1533_regex_hdr, np, np,
                           RE2::Hex(&val));
}

bool Hwch::ReplayParser::ParseHwclLayerTransform(const std::string& str,
                                                 uint32_t& val) {
  void* np = NULL;

  return RE2::PartialMatch(str, hwcl_layer_regex_hdr, np, np, np, np, &val) ||
         RE2::PartialMatch(str, hwcl_layer_1533_regex_hdr, np, np, np, np,
                           &val);
}

bool Hwch::ReplayParser::ParseHwclLayerBlending(const std::string& str,
                                                uint32_t& val) {
  if (str == "OP" || str == "BL" || str == "CV") {
    val = str == "OP" ? HWC_BLENDING_NONE : str == "BL" ? HWC_BLENDING_PREMULT
                                                        : HWC_BLENDING_COVERAGE;

    return true;
  }

  return false;
}

bool Hwch::ReplayParser::ParseHwclLayerColourSpace(const std::string& str,
                                                   uint32_t& val) {
  if (str == "RGBA" || str == "RGBX" || str == "RGB" || str == "565" ||
      str == "BGRA" || str == "5551" || str == "4444" || str == "YV12" ||
      str == "RAW" || str == "BLOB" || str == "IMPL" || str == "422s" ||
      str == "420s" || str == "422i" || str == "NV12X" || str == "NV12" ||
      str == "NV12Y" || str == "NV12L" || str == "NV12T" || str == "???") {
    val =
        str == "RGBA"
            ? HAL_PIXEL_FORMAT_RGBA_8888
            : str == "RGBX"
                  ? HAL_PIXEL_FORMAT_RGBX_8888
                  : str == "RGB"
                        ? HAL_PIXEL_FORMAT_RGB_888
                        : str == "565"
                              ? HAL_PIXEL_FORMAT_RGB_565
                              : str == "BGRA"
                                    ? HAL_PIXEL_FORMAT_BGRA_8888
                                    :
#if ANDROID_VERSION < 440
                                    str == "5551"
                                        ? HAL_PIXEL_FORMAT_RGBA_5551
                                        : str == "4444"
                                              ? HAL_PIXEL_FORMAT_RGBA_4444
                                              :
#endif
                                              str == "YV12"
                                                  ? HAL_PIXEL_FORMAT_YV12
                                                  :
#ifdef HAL_PIXEL_FORMAT_RAW_SENSOR
                                                  str == "RAW"
                                                      ? HAL_PIXEL_FORMAT_RAW_SENSOR
                                                      :
#endif
                                                      str == "BLOB"
                                                          ? HAL_PIXEL_FORMAT_BLOB
                                                          : str == "IMPL"
                                                                ? HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
                                                                : str == "422s"
                                                                      ? HAL_PIXEL_FORMAT_YCbCr_422_SP
                                                                      : str == "420s"
                                                                            ? HAL_PIXEL_FORMAT_YCrCb_420_SP
                                                                            : str == "422i"
                                                                                  ? HAL_PIXEL_FORMAT_YCbCr_422_I
                                                                                  : str == "NV12"
                                                                                        ? HAL_PIXEL_FORMAT_NV12
                                                                                        : str == "NV12Y"
                                                                                              ? HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL
                                                                                              : 0x0;

    return true;
  }

  return false;
}

bool Hwch::ReplayParser::IsHwclLayer(const std::string& line) {
  // Use a 'StringPiece' so that RE2 can match incrementally
  re2::StringPiece input(line);

  // Just check the header and trailer - skip additional visible regions
  return (RE2::Consume(&input, hwcl_layer_regex_hdr) ||
          RE2::Consume(&input, hwcl_layer_1533_regex_hdr)) &&
         RE2::FindAndConsume(&input, hwcl_layer_regex_trl);
}

bool Hwch::ReplayParser::IsHwclLayerFramebufferTarget(const std::string& line) {
  std::string comp;
  void* np = NULL;

  return ((RE2::PartialMatch(line, hwcl_layer_regex_hdr, np, &comp) ||
           RE2::PartialMatch(line, hwcl_layer_1533_regex_hdr, np, &comp)) &&
          (comp == "TG"));
}

bool Hwch::ReplayParser::IsHwclLayerSkip(const std::string& line) {
  int32_t flags;
  void* np = NULL;

  return (
      RE2::PartialMatch(line, hwcl_layer_regex_trl, np, np, np, &flags, np) &&
      (flags & HWC_SKIP_LAYER));
}

bool Hwch::ReplayParser::IsHwclLayerUnsupported(const std::string& line) {
  std::string colour_space;
  void* np = NULL;

  return ((RE2::PartialMatch(line, hwcl_layer_regex_hdr, np, np, np, np, np, np,
                             np, &colour_space) ||
           RE2::PartialMatch(line, hwcl_layer_1533_regex_hdr, np, np, np, np,
                             np, np, np, &colour_space)) &&
          (colour_space == "???"));
}

bool Hwch::ReplayParser::ParseHotPlug(const std::string& line,
                                      bool& connected) {
  if (RE2::FullMatch(line, hotplug_connected_regex)) {
    connected = true;
    return true;
  } else if (RE2::FullMatch(line, hotplug_disconnected_regex)) {
    connected = false;
    return true;
  }

  return false;
}

bool Hwch::ReplayParser::ParseBlanking(const std::string& line, bool& blank,
                                       int32_t& disp) {
  if (RE2::FullMatch(line, blank_regex, &disp)) {
    blank = true;
    return true;
  } else if (RE2::FullMatch(line, unblank_regex, &disp)) {
    blank = false;
    return true;
  }

  return false;
}

bool Hwch::ReplayParser::ParseHwclOnSet(const std::string& line, int32_t& secs,
                                        int32_t& msecs, int32_t& nsecs,
                                        int32_t& frame, int32_t& disp,
                                        int32_t& flags) {
  int32_t ret_fence = 0, acq_fence = 0;
  uint32_t buf = 0;
  std::string text;
  re2::StringPiece nsecs_sp, tid_sp, frame_sp;

  if (RE2::FullMatch(line, hwcl_onset_regex, &secs, &msecs, &nsecs_sp, &tid_sp,
                     &disp, &frame_sp, &ret_fence, RE2::Hex(&buf), &acq_fence,
                     &flags, &text)) {
    // Extract the number of nanoseconds (if present)
    nsecs = 0;
    if (!nsecs_sp.empty()) {
      RE2::FullMatch(nsecs_sp, "(\\d+)", &nsecs);
    }

    // Extract the transaction id (if present)
    int32_t tid = 0;
    if (!tid_sp.empty()) {
      RE2::FullMatch(tid_sp, "(\\d+)", &tid);
    }

    // Extract the frame identifier (if present)
    if (!frame_sp.empty()) {
      RE2::FullMatch(frame_sp, "(\\d+)", &frame);
    } else {
      frame = -1;
    }

    HWCLOGV_IF(PARSER_DEBUG,
               "Parser output: %ds %03dms %06dns TID:%04d D%d onSet"
               " Entry frame:%d Fd:%d outBuf:0x%x outFd:%d flags:%d%s",
               secs, msecs, nsecs, tid, disp, frame, ret_fence, buf, acq_fence,
               flags, text.c_str());

    HWCLOGV_IF(PARSER_DEBUG, "Original line: %s", line.c_str());

    return true;
  } else if (RE2::FullMatch(line, hwcl_onset_1533_regex, &secs, &msecs, &disp,
                            &ret_fence, RE2::Hex(&buf), &acq_fence, &flags,
                            &text)) {
    HWCLOGV_IF(PARSER_DEBUG,
               "Parser output: %ds %03dms %06dns D%d onSet Entry Fd:%d"
               " outBuf:0x%x outFd:%d Flags:%d%s",
               secs, msecs, nsecs, disp, ret_fence, buf, acq_fence, flags,
               text.c_str());

    HWCLOGV_IF(PARSER_DEBUG, "Original line: %s", line.c_str());

    return true;
  }

  return false;
}

bool Hwch::ReplayParser::ParseHwclLayer(const std::string& line,
                                        Hwch::Layer& layer) {
  // Use a 'StringPiece' so that RE2 can match incrementally
  re2::StringPiece input(line);

  int32_t layer_num = 0, fb = 0, transform = 0, width = 0, height = 0,
          refresh = 0;
  int32_t sc_left = 0, sc_top = 0, sc_right = 0, sc_bot = 0;
  float sc_left_f = 0, sc_top_f = 0, sc_right_f = 0, sc_bot_f = 0;
  int32_t df_left = 0, df_top = 0, df_right = 0, df_bot = 0;
  int32_t vr_left = 0, vr_top = 0, vr_right = 0, vr_bot = 0;
  int32_t acq_fence = 0, rel_fence = 0, hints = 0, flags = 0;
  uint32_t plane_alpha = 0, usage = 0;
  uint64_t handle = 0;
  std::string comp, fb_s, blend, colour_space, hints_s, flags_s;

  // Look for the start (i.e. header) of a layer
  bool match_hwcnext = false, match_1533 = false;

#if PARSER_DEBUG
  // Format the parsed values as if they were in the HWC log.
  // This enables a direct comparison between the original
  // string and what has been parsed.
  std::string debug;
#endif

  // Example HWC-next match:
  //
  // 0 OV 0xb7af3070:22:0 60 BL:FF RGBA 1920x1200    0.0,   0.0,1920.0,1200.0
  //    0,   0,1920,1200 36 -1 V:   0,   0,1920,1200 U:20000900 Hi:0 Fl:0 A B
  if (RE2::Consume(&input, hwcl_layer_regex_hdr, &layer_num, &comp,
                   RE2::Hex(&handle), &fb_s, &transform, &refresh, &blend,
                   RE2::Hex(&plane_alpha), &colour_space, &width, &height,
                   &sc_left_f, &sc_top_f, &sc_right_f, &sc_bot_f, &df_left,
                   &df_top, &df_right, &df_bot, &acq_fence, &rel_fence,
                   &vr_left, &vr_top, &vr_right, &vr_bot)) {
    match_hwcnext = true;

#if PARSER_DEBUG
    // re2::StringAppendF does not appear to support the '#' format specifier
    // (i.e.
    // to include the '0x' with hex prints). Use a string stream to format the
    // handle
    // for printing so that it matches the original input.
    std::stringstream padded_handle;
    padded_handle << std::showbase << std::setw(mHexPrintWidth) << std::hex
                  << handle;

    re2::StringAppendF(&debug,
                       "  %d %2s %s:%2s:%d %d %2s:%02X %-4s "
                       "%4dx%-4d %6.1f,%6.1f,%6.1f,%6.1f %4d,%4d,%4d,%4d %d %d "
                       "V:%4d,%4d,%4d,%4d ",
                       layer_num, comp.c_str(),
                       handle ? padded_handle.str().c_str() : "       0x0",
                       fb_s.c_str(), transform, refresh, blend.c_str(),
                       plane_alpha, colour_space.c_str(), width, height,
                       sc_left_f, sc_top_f, sc_right_f, sc_bot_f, df_left,
                       df_top, df_right, df_bot, acq_fence, rel_fence, vr_left,
                       vr_top, vr_right, vr_bot);
#endif
  }

  // Example 15.33 match:
  //
  // 0 OV 0xb91e51e0:69:0 OP:FF 422i  1280x960 0,   0,1280, 960->
  //  141, 112,1144, 864 -1 -1 V: 141, 112,1144, 864
  if (RE2::Consume(&input, hwcl_layer_1533_regex_hdr, &layer_num, &comp,
                   RE2::Hex(&handle), &fb, &transform, &blend,
                   RE2::Hex(&plane_alpha), &colour_space, &width, &height,
                   &sc_left, &sc_top, &sc_right, &sc_bot, &df_left, &df_top,
                   &df_right, &df_bot, &acq_fence, &rel_fence, &vr_left,
                   &vr_top, &vr_right, &vr_bot)) {
    match_1533 = true;

#if PARSER_DEBUG
    // re2::StringAppendF does not appear to support the '#' format specifier
    // (i.e.
    // to include the '0x' with hex prints). Use a string stream to format the
    // handle
    // for printing so that it matches the original input.
    std::stringstream padded_handle;
    padded_handle << std::showbase << std::setw(mHexPrintWidth) << std::hex
                  << handle;

    re2::StringAppendF(&debug,
                       "    %d %2s %s:%2d:%d %2s:%2X %-5s "
                       "%4dx%-4d %4d,%4d,%4d,%4d->%4d,%4d,%4d,%4d %d %d "
                       "V:%4d,%4d,%4d,%4d ",
                       layer_num, comp.c_str(),
                       handle ? padded_handle.str().c_str() : "       0x0", fb,
                       transform, blend.c_str(), plane_alpha,
                       colour_space.c_str(), width, height, sc_left, sc_top,
                       sc_right, sc_bot, df_left, df_top, df_right, df_bot,
                       acq_fence, rel_fence, vr_left, vr_top, vr_right, vr_bot);
#endif
  }

  if (match_hwcnext || match_1533) {
    // The header match was successful. Check the strings for validity,
    // update the layer and store the result for later.
    uint32_t blend_val = 0, colour_space_val = 0;
    if (!(ParseHwclLayerBlending(blend, blend_val) &&
          ParseHwclLayerColourSpace(colour_space, colour_space_val))) {
      HWCLOGE_IF(PARSER_DEBUG,
                 "Layer header matched, but blending or "
                 "colour space string is malformed: %s",
                 line.c_str());
      return false;
    }

    layer.SetBlending(blend_val);
    layer.SetTransform(transform);
    layer.SetPlaneAlpha(plane_alpha);
    layer.SetLogicalDisplayFrame(
        Hwch::LogDisplayRect(df_left, df_top, df_right, df_bot));
    layer.mFormat = colour_space_val;
    layer.mWidth = width;
    layer.mHeight = height;

    if (match_hwcnext) {
      layer.SetCrop(
          Hwch::LogCropRect(sc_left_f, sc_top_f, sc_right_f, sc_bot_f));
    } else {
      layer.SetCrop(Hwch::LogCropRect(sc_left, sc_top, sc_right, sc_bot));
    }

    // This line has the header of a layer - look for any further
    // visible regsions. Example match:
    //
    // 1882,0,1920,1200
    while (RE2::Consume(&input, hwcl_layer_regex_vbr, &vr_left, &vr_top,
                        &vr_right, &vr_bot)) {
#if PARSER_DEBUG
      re2::StringAppendF(&debug, "%4d,%4d,%4d,%4d ", vr_left, vr_top, vr_right,
                         vr_bot);
#endif
    }

    // Look for the usage, hints and flags. Example match:
    //
    // U:00000b02 Hi:0:CLR Fl:0:SKIP
    if (RE2::Consume(&input, hwcl_layer_regex_trl, RE2::Hex(&usage), &hints,
                     &hints_s, RE2::Hex(&flags), &flags_s)) {
      layer.SetFlags(flags);

#if PARSER_DEBUG
      re2::StringAppendF(&debug, "U:%08x Hi:%d%s Fl:%x%s", usage, hints,
                         hints_s.c_str(), flags, flags_s.c_str());

      if ((flags & HWC_SKIP_LAYER) == 0) {
        HWCLOGE_IF(debug != line,
                   "Replay parser output does not match "
                   "original input:\nParser output: %s\nOriginal line: %s",
                   debug.c_str(), line.c_str());

        HWCLOGV("Parser output: %s", debug.c_str());
        HWCLOGV("Original line: %s", line.c_str());
      }
#endif

      return true;
    } else {
      HWCLOGE_IF(PARSER_DEBUG,
                 "Layer header matched, but trailer is "
                 "malformed: %s",
                 line.c_str());
    }
  }

  return false;
}

/*
 * Functions for parsing files generated by dumpsys
 */

bool Hwch::ReplayParser::IsDSLayer(const std::string& line) {
  return RE2::FullMatch(line, ds_layer_regex);
}

bool Hwch::ReplayParser::IsDSLayerFramebufferTarget(const std::string& line) {
  std::string comp;

  return (RE2::FullMatch(line, ds_layer_regex, &comp) && comp == "FB TARGET");
}

bool Hwch::ReplayParser::ParseDSProfile(const std::string& line,
                                        std::string& profile) {
  void* np = NULL;

  return RE2::FullMatch(line, ds_layer_regex, np, np, np, np, np, np, np, np,
                        np, np, np, np, np, np, np, np, np, &profile);
}

bool Hwch::ReplayParser::ParseDSDisplay(const std::string& line, int32_t& disp,
                                        int32_t& width, int32_t& height) {
  std::string xdpi_str, ydpi_str;
  int32_t refresh = 0;

  if (RE2::FullMatch(line, ds_display_regex, &disp, &width, &height, &xdpi_str,
                     &ydpi_str, &refresh)) {
    HWCLOGV_IF(PARSER_DEBUG,
               "Parser output:  Display[%d] : %dx%d, xdpi=%s,"
               " ydpi=%s, refresh=%d",
               disp, width, height, xdpi_str.c_str(), ydpi_str.c_str(),
               refresh);

    HWCLOGV_IF(PARSER_DEBUG, "Original line: %s", line.c_str());

    return true;
  }

  return false;
}

bool Hwch::ReplayParser::ParseDSLayer(const std::string& line,
                                      Hwch::Layer& layer) {
  std::string comp_type, name, profile, refresh_str;
  int32_t hints = 0, flags = 0, tr = 0;
  uint32_t buf_handle = 0, blend = 0, format = 0;
  float sc_top = 0.0, sc_left = 0.0, sc_bot = 0.0, sc_right = 0.0,
        refresh = 0.0;
  int32_t df_top = 0, df_left = 0, df_bot = 0, df_right = 0;

  if (RE2::FullMatch(line, ds_layer_regex, &comp_type, RE2::Hex(&buf_handle),
                     &hints, &flags, &tr, RE2::Hex(&blend), RE2::Hex(&format),
                     &sc_left, &sc_top, &sc_right, &sc_bot, &df_left, &df_top,
                     &df_right, &df_bot, &name, &refresh_str, &profile)) {
    layer.SetBlending(blend);
    layer.SetTransform(tr);
    layer.SetCrop(Hwch::LogCropRect(sc_left, sc_top, sc_right, sc_bot));
    layer.SetLogicalDisplayFrame(
        Hwch::LogDisplayRect(df_left, df_top, df_right, df_bot));

    layer.mWidth = sc_right;
    layer.mHeight = sc_bot;
    layer.mFormat = format;

    refresh = std::atof(refresh_str.c_str());

    if (layer.mPattern != NULL) {
      // Set the update frequency
      layer.GetPattern().SetUpdateFreq(refresh ? refresh
                                               : mDefaultDSUpdateFreq);
    }

    HWCLOGV_IF(PARSER_DEBUG,
               "Parser output: %11s | %8x | %08x | %08x | %02x "
               "| %05x | %08x | [ %6.1f, %6.1f, %6.1f, %6.1f] |"
               "[ %4d, %4d, %4d, %4d] %s: %.3f\n",
               comp_type.c_str(), buf_handle, hints, flags, tr, blend, format,
               sc_left, sc_top, sc_right, sc_bot, df_left, df_top, df_right,
               df_bot, name.c_str(), refresh);

    HWCLOGV_IF(PARSER_DEBUG, "Original line: %s", line.c_str());

    return true;
  }

  return false;
}

/*
 * Unit-Test Framework
 *
 * This function provides a unit-testing framework that is designed to prevent
 *regressions
 * when modifying the regular expressions. The intention is that test-cases can
 *be added
 * easily as parsing mismatches are encountered in practice. Then, by running
 *the tests from
 * the command-line (using the -replay_test argument), the developer can ensure
 *that they have
 * not introduced any regressions.
 */

#ifdef PARSER_DEBUG
bool Hwch::ReplayParser::RunParserUnitTests() {
  const char* tests[] = {
      // PreETM 'onSet' string (from
      // DualDisplay-BasicPlayback-H264-CloneMode.log)
      "9042s 464ms 821307ns D0 onSet Entry Fd:-1 outBuf:0x0 outFd:-1 Flags:0",

      // PreETM layer string (from DualDisplay-BasicPlayback-H264-CloneMode.log)
      "1 FB 0xf78825c0: 0:0 OP:FF NV12T 1280x736     0,   0,1280, 720->   "
      "0,  79,1920,1159 -1 -1 V:   0,  79,1920,1159 U:00002900 Hi:0 "
      "Fl:20000000",

      // Parse failure identified by Srinivas Kulkarni (email: 18/12/14 @ 11:16)
      "  0 OV 0xf690d1a0:20:060 OP:FF RGBX 1920x1280    0.0,  "
      "40.0,1920.0,1240.0"
      "    0,   0,1920,1200 116 -1 V:   0,   0,1920,1200 U:00000900 Hi:0 Fl:0 "
      "SO SC",

      // Dumpsys display string (from work with Oleg - 04/12/14)
      "  Display[0] : 1920x1200, xdpi=254.000000, ydpi=254.000000, "
      "refresh=16666666",

      // Dumpsys layer string (from work with Oleg - 04/12/14)
      "        HWC | b83db7a0 | 00000000 | 00000000 | 00 | 00100 | 7fa00f00 | "
      "[    0.0,   "
      "40.0, 1920.0, 1240.0] | [    0,    0, 1920, 1200] "
      "com.android.systemui.ImageWallpaper",

      // New notation adopted after the logging was migrated into HWC
      "  1 TG        0x0:--:0 60 BL:FF ???      0x0       0.0,   "
      "0.0,1920.0,1200.0    0,   "
      "0,1920,1200 -1 -1 V:   0,   0,1920,1200 U:00000000 Hi:0 Fl:0 OP DISABLE",

      "  1 TG        0x0:20:0 60 BL:FF ???      0x0       0.0,   "
      "0.0,1920.0,1200.0    0,   "
      "0,1920,1200 -1 -1 V:   0,   0,1920,1200 U:00000000 Hi:0 Fl:0 OP DISABLE",

      // Log sent to me by Gary (29/04/15) with 64 bit handles
      "  0 OV 0x7f72edc2d060: 0:0 60 OP:FF NV12Y  864x480     0.0,   0.0, "
      "854.0, 480.0    0,"
      "  40,1280, 759 -1 -1 V:   0,  40,1280, 759 U:00006900 Hi:0 Fl:0 OP V "
      "ENCRYPT(S:0, I:5) S SC",

      // HotPlug Events
      "17796s 455ms DrmDisplay 2 Crtc:22 HotPlug connected",

      "17822s 833ms DrmDisplay 2 Crtc:22 HotPlug disconnected",

      // Blank/Unblank Events
      "17783s 238ms HardwareManager::onBlank Display 0 Blank SURFACE_FLINGER",

      "17786s 902ms HardwareManager::onBlank Display 0 Unblank SURFACE_FLINGER"

      // Legacy (15.33) Skip Layer Support
      "     5 FB 0x0: 0:0 BL:FF ???      0x0       0,   0,   0,   0->   0, "
      "983,1920,1200 -1 -1 "
      "V:   0, 983,1920,1200 U:00000000 Hi:0 Fl:1:SKIP"};

  // Dummy variables for the parsing functions to write into
  int32_t secs = 0, msecs = 0, nsecs = 0, frame = 0, flags = 0, disp = 0,
          width = 0, height = 0;
  int32_t session = 0, instance = 0;
  Hwch::Layer test_layer;
  bool connected, blank;

  int32_t sz = sizeof(tests) / sizeof(const char*);
  int32_t num_failed = 0;
  for (int32_t i = 0; i < sz; ++i) {
    if (!ParseHwclOnSet(tests[i], secs, msecs, nsecs, frame, disp, flags) &&
        !ParseHwclLayer(tests[i], test_layer) &&
        !ParseDSDisplay(tests[i], disp, width, height) &&
        !ParseDSLayer(tests[i], test_layer) &&
        !ParseHotPlug(tests[i], connected) &&
        !ParseBlanking(tests[i], blank, disp) &&
        !IsHwclLayerSkip(tests[i])) {
      std::printf("Regular expression match failed for: %s\n", tests[i]);
      ++num_failed;
    }
  }

  std::printf("Passed %d (of %d) regular expression tests\n", sz - num_failed,
              sz);

  return (num_failed == 0);
}
#endif
