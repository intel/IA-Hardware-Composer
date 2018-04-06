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

#include "HwcHarness.h"
#include "HwchTests.h"
#include "HwchGlTests.h"
#include "HwchInternalTests.h"
#include "HwcvalStatistics.h"

#include <dirent.h>
#include "iservice.h"

#include <ctype.h>

#include <binder/ProcessState.h>

#include "HwchReplayHWCLRunner.h"
#include "HwchReplayDSRunner.h"

#include <hardware/power.h>
#include <hardware/lights.h>

#define VERSION_NUMBER "HwcHarness Test"

#define COMPARE_DRM_OUTPUT 0

using namespace android;
using namespace hwcomposer;

void Wake(bool wake, int backlight) {
  char* libError;
  // Power mode to interactive
  void* libPowerHandle =
      dll_open(HWCVAL_LIBPATH "/hw/power.default.so", RTLD_NOW);
  if (!libPowerHandle) {
    HWCLOGW("Failed to open power.default.so");
    return;
  }

  libError = (char*)dlerror();

  if (libError != NULL) {
    HWCLOGW("In Wake() Error getting libPowerHandle %s", libError);
    return;
  }

  dlerror();

  const char* sym = HAL_MODULE_INFO_SYM_AS_STR;

  power_module* pPowerModule = (power_module*)dlsym(libPowerHandle, sym);

  libError = (char*)dlerror();
  if (libError != NULL) {
    HWCLOGW("In Wake() Error getting symbol %s", libError);
    return;
  }

  HWCLOGD("Setting interactive %s", wake ? "enable" : "disable");
  pPowerModule->setInteractive(pPowerModule, wake ? 1 : 0);
  sleep(1);

#ifdef POWER_HINT_LOW_POWER
  HWCLOGD("Setting power hint %s", wake ? "interaction" : "low power");
  pPowerModule->powerHint(
      pPowerModule, wake ? POWER_HINT_INTERACTION : POWER_HINT_LOW_POWER, 0);
  sleep(1);
#endif

  // Backlight enable
  hw_module_t* module;
  light_device_t* backlightDevice;

  int err =
      hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (hw_module_t const**)&module);
  if (err != 0) {
    HWCLOGE("Failed to get lights module");
    return;
  }

  hw_device_t* device;
  err = module->methods->open(module, LIGHT_ID_BACKLIGHT, &device);
  if (err != 0) {
    HWCLOGW("Failed to open backlight module");
    return;
  }

  backlightDevice = (light_device_t*)device;

  light_state_t state;
  state.color = wake ? backlight : 0;
  state.flashMode = LIGHT_FLASH_NONE;
  state.flashOnMS = 0;
  state.flashOffMS = 0;
  state.brightnessMode = BRIGHTNESS_MODE_USER;

  err = backlightDevice->set_light(backlightDevice, &state);
  if (err == 0) {
    HWCLOGD("Backlight turned %s.", wake ? "on" : "off");
  } else {
    HWCLOGW("Failed to turn %s backlight, status=%d", wake ? "on" : "off", err);
  }
  sleep(1);
}

HwcTestRunner::HwcTestRunner(Hwch::Interface& interface)
    : mInterface(interface),
      mCurrentTest(0),
      mNumPasses(0),
      mNumFails(0),
      mBrief(false),
      mNoShims(false),
      mTestNum(0),
      mAllTests(false),
      mHWCLReplay(false),
      mDSReplay(false),
      mDSReplayNumFrames(2000),
      mReplayMatch(0),
      mReplayFileName(NULL),
      mReplayNoTiming(false),
      mReplayTest(false),
      mWatchdogFps(10.0),
      mWatchdog(this),
      mSystem(Hwch::System::getInstance()),
      mRunTimeStat("run_time") {
  mState = HwcTestState::getInstance();

  // Each test must run in under 10 minutes, OR exceed a frame rate of 10fps.
  mWatchdog.Set(10, mWatchdogFps);
}

int HwcTestRunner::getargs(int argc, char** argv) {
  HwcTestConfig& config = *HwcGetTestConfig();

  // TODO: change option processing so that all options with parameters are of
  // the form
  // -option=<parameter>
  // then we can use GetXxParam() for everything.
  //
  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-h") == 0) ||
        (strcmp(argv[i], "-verbose_help") == 0)) {
      printf(
          "Usage: %s [OPTIONS]...\n"
          "version: %s\n"
          "\t-h                        Usage and main options\n"
          "\t-verbose_help             All help, including some very "
          "specialised options\n"
          "\t-all                      Run all tests\n"
          "\t-t <test name>            Choose test to run (this option can be "
          "repeated)\n"
          "\t-avoid <test name>        Choose test to skip when '-all' is "
          "specified (this option can be repeated)\n"
          "\n"
          "Replay options:\n"
          "\t-replay_hwcl <file>       Replay a test from a Hardware Composer "
          "log file\n"
          "\t-replay_ds <file>         Recreate a HWC scenario from the output "
          "of 'dumpsys'\n"
          "\t-replay_ds_frames <num>   Override the default number of frames "
          "(2000) for a dumpsys replay\n"
          "\t-replay_match <num>       Adjusts the criteria used to track "
          "buffers. Match on:\n"
          "\t  0 - buffer handles that are 'known' to a frame\n"
          "\t  1 - buffer width/height and logical display frame coordinates\n"
          "\t  2 - buffer width/height, crop frame width and logical display "
          "frame width\n"
          "\t  3 - buffer width/height, crop frame width/height and display "
          "frame width/height\n"
          "\t  4 - buffer width/height and either the crop frame or the "
          "display frame coordinates\n\n"
          "\t-replay_no_timing         Run without inter-frame spacing i.e. "
          "send frames to the HWC as fast as possible\n"
          "\t-replay_alpha=n           Sets an alpha value for all replay "
          "layers\n"
          "\t-replay_test              Runs the parser unit-tests and prints "
          "any regular expression mismatches\n"
          "\t-crc                      Enable CRC-based flicker checking "
          "(requires Android build with CONFIG_DEBUG_FS=y)\n"
          "\n"
          "Harness configuration:\n"
          "\t-default_num_buffers      Number of buffers each layer will have, "
          "unless overriden in the code.\n"
          "\t-no_compose               Disable Reference Composer composition\n"
          "\t-no_fill                  For maximum speed, don't fill any of "
          "the display buffers\n"
          "\t-no_shims                 Run without installing the shims\n"
          "\t-no_hdmi                  Don't use any HDMI display that is "
          "connected.\n"
          "\t-hwc_config=<filename>    Configure hwc options via a registry "
          "file.\n"
          "\t-async_buffer_destruction Destroy Gralloc buffers on a separate "
          "thread with random delays\n"
          "\t-screen_disable_method=<list> Configure available methods for "
          "suspend and resume on random tests\n"
          "\t\t<list> = all | [blank [,]] [power [,]] [both]\n"
          "\t-force_setdisplay_fail=<Range>\n"
          "\t                          SetDisplays in <Range> will fail.\n"
          "\t-force_rotation_animation Forces rotations to emulate the Android "
          "rotation animation\n"
          "\t-send_frames=<Range>      Send only frames in <Range> to "
          "onPrepare/onSet (to simplify and speed up testing)\n"
          "\t-render_compression=<opt> Sets the render compression flag in "
          "Gralloc\n"
          "\t  RC    - sets the render compression bit (only) for all buffers\n"
          "\t  CC_RC - flags all buffers as containing Clear Compressed or "
          "Render Compressed content\n"
          "\t  Hint  - follows the hint sent to GL by HWC (see "
          "render_compress_ignore_hint below)\n"
          "\t-render_compression_ignore_hint=<Range> Sets a range for ignoring "
          "hints sent to GL by the HWC (ignored for 'RC' or 'CC_RC')\n\n"
          "Where: <Range> is a comma-separated list of:\n"
          "\t\t<n>: frame numbers\n"
          "\t\t[<n>]-[<m>]: contiguous range of frame numbers\n"
          "\t\t<x>n: every xth frame\n"
          "\t\t<x>r: random, every xth frame on average\n"
          "\n"
          "Stalls:\t<StallConfig>=<p>%%<t><time unit>\n"
          "\tWhere\t<p>=percentage of sample points where the stall will "
          "happen\n"
          "\t\t<t>=duration of stall (units follow)\n"
          "\t\t<time unit>=s|ms|us|ns\n"
          "\t-force_setdisplay_stall=<StallConfig> Stall configuration for "
          "calls to drmModeSetDisplay\n"
          "\t-force_dpms_stall=<StallConfig>       Stall configuration for "
          "calls to configure DPMS on/off\n"
          "\t-force_setmode_stall=<StallConfig>    Stall configuration for "
          "setMode service calls\n"
          "\t-force_blank_stall=<StallConfig>      Stall configuration for "
          "blank calls\n"
          "\t-force_unblank_stall=<StallConfig>    Stall configuration for "
          "unblank calls\n"
          "\t-force_hotplug_stall=<StallConfig>    Stall configuration for "
          "hotplugs\n"
          "\t-force_hotunplug_stall=<StallConfig>  Stall configuraiton for hot "
          "unplugs\n"
          "\t-force_gem_wait_stall=<StallConfig>   Stall configuration for "
          "GEM_WAIT calls\n"
          "\n"
          "Test configuration:\n"
          "\t-val_hwc_composition      Enable validation of HWC composition "
          "against reference composer using SSIM\n"
          "against reference composer using SSIM\n"
          "\t-val_buffer_allocation    Enable test failure from buffer "
          "allocation checks\n"
          "\t-val_displays             Enable test failure from kernel "
          "displays specific checks\n"
          "\t-no_val_hwc               Inhibit test failure from Hardware "
          "Composer specific checks\n"
          "checks\n"
          "\t-val_sf                   Enable test failure from SurfaceFlinger "
          "specific checks\n"
          "\n"
          "\nVirtual display options:\n"
          "\t-virtual_display <w>x<h>  Enables virtual display emulation for a "
          "specified width and height\n"
          "Logging options:\n"
          "\t-brief                    Provide minimal information in stdout, "
          "focus on pass/fail\n"
          "\t-logname=<name>           Set name of results file to "
          "results_<name>.csv\n"
          "\t-shortlog                 Suppress verbose flags in logcat and "
          "hwclog\n"
          "\t-log_pri=V|D|I|W|E|F      Select minimum log priority\n"
          "\n",
          argv[0], VERSION_NUMBER);

      if (strcmp(argv[i], "-verbose_help") == 0) {
        printf(
            "More options:\n"
            "\t-blank_after                      OnBlank should be called "
            "after each test\n"
            "\t-delay_page_flip                  Delay every 5th page flip on "
            "D0 by 500ms to test out-of-order buffer release\n"
            "\t-dump_frames=<Range>              Dump tga files for all the "
            "input buffers on frames with frame numbers in the range\n"
            "\t-randomize_modes                  Randomize the video modes on "
            "each hotplug (and choose a random subset)\n"
            "\t-vsync_delay=n<time unit>         Set delay offset for VSync "
            "synchronization, in us\n"
            "\t-vsync_timeout=n<time unit>       Set timeout for VSync "
            "synchronization, in us (Default=50000)\n"
            "\t-vsync_period=n<time unit>        Set frame period for when "
            "VSyncs don't come, in us (Default=16666)\n"
            "\t-sync_to=compose|prepare|set      Set the event that is "
            "synchronized to the given delay from VSync\n"
            "\t\tWhere: <time unit>=s|ms|us|ns\n"
            "\n"
            "Buffer/Crop/Display frame size control:\n"
            "\t-no_adjust_sizes                  Inhibit all the following "
            "controls\n"
            "\t-min_buf_size                     Default minimum value for "
            "buffer width/height (D=1 pixel)\n"
            "\t-min_buf_width                    Minimum buffer width "
            "(D=min_buf_size)\n"
            "\t-min_buf_height                   Minimum buffer height "
            "(D=min_buf_size\n"
            "\t-min_crop_width                   Minimum crop width "
            "(D=min_buf_width)\n"
            "\t-min_crop_height                  Minimum crop height "
            "(D=min_buf_height)\n"
            "\t-min_display_frame_size           Minimum display frame "
            "width/height (D=2 pixels)\n"
            "\t-min_display_frame_width          Minimum display frame width "
            "(D=min_display_frame_size)\n"
            "\t-min_display_frame_height         Minimum display frame height "
            "(D=min_display_frame_size)\n"
            "\t-min_NV12_crop_width              Minimum crop width for NV12 "
            "buffers (D=min_crop_width or 4 pixels, whichever is greater)\n"
            "\t-min_NV12_crop_height             Minimum crop height for NV12 "
            "buffers (D=min_crop_height or 4 pixels, whichever is greater)\n"
            "\t-NV12_display_frame_alignment     Alignment of source crop "
            "offset and size for YUY2 buffers.\n"
            "\t-min_NV12_display_frame_width     Minimum display frame width "
            "for NV12 buffers (D=min_display_frame_width or 4 pixels, "
            "whichever is greater)\n"
            "\t-min_NV12_display_frame_height    Minimum display frame height "
            "for NV12 buffers (D=min_display_frame_height or 4 pixels, "
            "whichever is greater)\n"
            "\t-NV12_display_frame_alignment     Alignment of display frame "
            "offset and size for NV12 buffers.\n"
            "\t-min_YUY2_crop_width              Minimum crop width for YUY2 "
            "buffers (D=min_crop_width or 4 pixels, whichever is greater)\n"
            "\t-min_YUY2_crop_height             Minimum crop height for YUY2 "
            "buffers (D=min_crop_height or 4 pixels, whichever is greater)\n"
            "\t-YUY2_crop_alignment              Alignment of souce crop "
            "offset and size for YUY2 buffers.\n"
            "\t-min_YUY2_display_frame_width     Minimum display frame width "
            "for YUY2 buffers (D=min_display_frame_width or 4 pixels, "
            "whichever is greater)\n"
            "\t-min_YUY2_display_frame_height    Minimum display frame height "
            "for YUY2 buffers (D=min_display_frame_height or 4 pixels, "
            "whichever is greater)\n"
            "\t-YUY2_display_frame_alignment     Alignment of display frame "
            "offset and size for YUY2 buffers.\n"
            "\n"
            "Some tests have additional test-specific options.\n"
            "\n");
      }

      std::string names;
      Hwch::BaseReg::mHead->AllNames(names);
      std::string testNamesString = std::string("Tests: ") + names;
      printf("%s\n", testNamesString.c_str());
      return 0;
    } else if (strcmp(argv[i], "-t") == 0) {
      ++i;
      if (i < argc) {
        mTestNames.push_back(std::string(argv[i]));
      }
    } else if (strcmp(argv[i], "-avoid") == 0) {
      ++i;
      if (i < argc) {
        mAvoidNames.push_back(std::string(argv[i]));
      }
    } else if (strcmp(argv[i], "-replay_hwcl") == 0) {
      mHWCLReplay = true;

      if (++i < argc) {
        mReplayFileName = argv[i];
        HWCLOGI(" HwcTestBase::SetArgs - replay requested for HWC log: %s",
                mReplayFileName);
      }
    } else if (strcmp(argv[i], "-replay_ds") == 0) {
      mDSReplay = true;

      if (++i < argc) {
        mReplayFileName = argv[i];
        HWCLOGI(" HwcTestBase::SetArgs - replay requested for dumpsys file: %s",
                mReplayFileName);
      }
    } else if (strcmp(argv[i], "-replay_ds_frames") == 0) {
      if (++i < argc) {
        mDSReplayNumFrames = atoi(argv[i]);
        if (!mDSReplayNumFrames) {
          HWCERROR(eCheckCommandLineParam,
                   " HwcTestBase::SetArgs - number of frames for dumpsys "
                   "replay must be > 0 (not %d)",
                   mDSReplayNumFrames);
        } else {
          HWCLOGI(
              " HwcTestBase::SetArgs - dumpsys replay requested for: %d frames",
              mDSReplayNumFrames);
        }
      }
    } else if (strcmp(argv[i], "-replay_match") == 0) {
      if (++i < argc) {
        mReplayMatch = atoi(argv[i]);
        if ((!mReplayMatch) || (mReplayMatch > 4)) {
          HWCERROR(eCheckCommandLineParam,
                   " HwcTestBase::SetArgs - match selection must be between 0 "
                   "and 4 (seen: %d)",
                   mReplayMatch);
        } else {
          HWCLOGI(" HwcTestBase::SetArgs - selected match algorithm: %d frames",
                  mDSReplayNumFrames);
        }
      }
    } else if (strcmp(argv[i], "-virtual_display") == 0) {
      int32_t vd_width = 0, vd_height = 0;

      if ((++i >= argc) ||
          std::sscanf(argv[i], "%dx%d", &vd_width, &vd_height) != 2) {
        std::fprintf(stderr,
                     "Fatal: can not parse virtual display dimensions.\n"
                     "Usage is: -virtual_display <w>x<h> (e.g. "
                     "-virtual_display 1920x1280)\n");
        exit(-1);
      } else if ((vd_width <= 0) || (vd_height <= 0)) {
        std::fprintf(stderr,
                     "Fatal: invalid virtual display dimensions (%dx%d).\n"
                     "Virtual display disabled.\n",
                     vd_width, vd_height);
        exit(-1);
      }

      mSystem.EnableVirtualDisplayEmulation(vd_width, vd_height);
    }
    // keep this last in the option processing
    else if (argv[i][0] == '-') {
      // Test-specific parameters -<p>=<value>
      const char* parstr = argv[i] + 1;
      const char* eq = strchr(parstr, '=');
      std::string parname;
      std::string parval;

      if (eq) {
        parname = std::string(parstr, eq - parstr);
        parval = eq + 1;
      } else {
        parname = parstr;
        parval = "1";
      }

      Hwch::UserParam up(parval);
      mParams.emplace(parname, up);
    }
  }

  SetParams(mParams);

  // Further option processing.
  mAllTests = (GetParam("all") != 0);

  mReplayNoTiming = (GetParam("replay_no_timing") != 0);
  mReplayTest = (GetParam("replay_test") != 0);

  // Minimal logging in stdout (let's not confuse those developers).
  mBrief = (GetParam("brief") != 0);
  config.SetCheck(eOptBrief, mBrief);

  // Log priority.
  // This code will NOT be executed if you use valhwch
  // because the argument is intercepted by the script and converted to an
  // environment variable.
  // This is provided as a fallback in case hwcharness is used directly.
  const char* logPri = GetStrParam("log_pri");
  if (logPri && *logPri) {
    int priority;

    switch (toupper(*logPri)) {
      case 'V':
        priority = ANDROID_LOG_VERBOSE;
        break;

      case 'D':
        priority = ANDROID_LOG_DEBUG;
        break;

      case 'I':
        priority = ANDROID_LOG_INFO;
        break;

      case 'W':
        priority = ANDROID_LOG_WARN;
        break;

      case 'E':
        priority = ANDROID_LOG_ERROR;
        break;

      case 'F':
        priority = ANDROID_LOG_FATAL;
        break;

      default:
        priority = ANDROID_LOG_ERROR;
    }

    config.mMinLogPriority = priority;
  }

  // Arguments FROM NOW ON are gathered so they can be logged if the test fails.
  // Arguments ABOVE this are ignored, because they are about what is logged
  // rather than
  // how the test is run.
  UsedArgs() = "";

  // Enable CRC-based flicker detection
  if (GetParam("crc") != 0) {
    config.SetCheck(eCheckCRC);
  }

  // Option to delay 1 in 5 of D0 page flips by 500ms.
  config.SetCheck(eOptDelayPF, (GetParam("delay_page_flip") != 0));

  // Option to randomize the video modes on HDMI
  config.SetCheck(eOptRandomizeModes, (GetParam("randomize_modes") != 0));

  if (GetParam("no_fill") != 0) {
    // Don't bother filling any buffers
    mSystem.SetNoFill(true);
  }

  // Ignore shims
  // No effect in harness at present, used in valhwch script.
  mNoShims = (GetParam("no_shims") != 0);

  // Do we want buffers destroyed on a separate thread, to test Gralloc/HWC
  // handshake
  config.SetCheck(eOptAsyncBufferDestruction,
                  (GetParam("async_buffer_destruction") != 0));

  const char* str = GetParam("force_setdisplay_fail");
  if (str) {
    // Enable failure spoofing
    EnableDisplayFailSpoof(str);
  }

  const char* rotAnim = GetParam("force_rotation_animation");
  if (rotAnim) {
    // Enable rotation animation
    mSystem.SetRotationAnimation(true);
  }

  // Disable use of GL
  if (GetParam("no_gl")) {
    config.SetCheck(eOptForceCPUFill);
  }

  Hwch::Range range;
  if (GetRangeParam("send_frames", range)) {
    mSystem.SetSendFrames(range);
  }

  // Process the render compression arguments
  std::string renderCompress = GetStrParamLower("render_compression");
  if (renderCompress == "rc") {
    mSystem.SetGlobalRenderCompression(Hwch::Layer::eCompressionRC);
  } else if (renderCompress == "cc_rc") {
    mSystem.SetGlobalRenderCompression(Hwch::Layer::eCompressionCC_RC);
  } else if (renderCompress == "hint") {
    mSystem.SetGlobalRenderCompression(Hwch::Layer::eCompressionHint);
  }

  Hwch::Range rcIgnoreHintRange;
  if (GetRangeParam("render_compression_ignore_hint", rcIgnoreHintRange)) {
    if ((mSystem.GetGlobalRenderCompression() == Hwch::Layer::eCompressionRC) ||
        (mSystem.GetGlobalRenderCompression() ==
         Hwch::Layer::eCompressionCC_RC)) {
      std::fprintf(
          stderr,
          "Warning: render compression ignore hint range specified with "
          "RC or CC_RC option set\n");
    }

    mSystem.SetRCIgnoreHintRange(rcIgnoreHintRange);
  }
  int64_t maxUnblankingLatency =
      GetTimeParamUs("unblanking_time_limit",
                     HWCVAL_MAX_UNBLANKING_LATENCY_DEFAULT_US) *
      HWCVAL_US_TO_NS;
  mState->SetMaxUnblankingLatency(maxUnblankingLatency);

  // Process options for configuring spoofed stalls
  ConfigureStalls();

  // Process options for configuring input frame dump
  ConfigureFrameDump();

  mArgs += UsedArgs();

  return 1;
}

void HwcTestRunner::SetBufferConfig() {
  Hwch::BufferFormatConfigManager& mgr = mSystem.GetBufferFormatConfigManager();
  UsedArgs() = "";

  // Provide a global way to inhibit the fixup; could be useful for replay in
  // particular.
  if (GetParam("no_adjust_sizes")) {
    Hwch::BufferFormatConfig deflt(1, 1, 1, 1, 1, 1, 0.0, 1, 1);
    mgr.SetDefault(deflt);
  } else {
    uint32_t minBufSize = GetIntParam("min_buf_size", 1);
    uint32_t minBufWidth = GetIntParam("min_buf_width", minBufSize);
    uint32_t minBufHeight = GetIntParam("min_buf_height", minBufSize);
    uint32_t minCropSize = GetIntParam("min_crop_size", minBufSize);
    uint32_t minCropWidth =
        GetIntParam("min_crop_width", max(minCropSize, minBufWidth));
    uint32_t minCropHeight =
        GetIntParam("min_crop_height", max(minCropSize, minBufHeight));
    uint32_t minDisplayFrameSize = GetIntParam("min_display_frame_size", 2);
    uint32_t minDisplayFrameWidth =
        GetIntParam("min_display_frame_width", minDisplayFrameSize);
    uint32_t minDisplayFrameHeight =
        GetIntParam("min_display_frame_height", minDisplayFrameSize);

    uint32_t minNV12CropSize =
        GetIntParam("min_NV12_crop_size", max(4U, minCropSize));
    uint32_t minNV12CropWidth =
        GetIntParam("min_NV12_crop_width", max(minNV12CropSize, minCropWidth));
    uint32_t minNV12CropHeight = GetIntParam(
        "min_NV12_crop_height", max(minNV12CropSize, minCropHeight));
    uint32_t minNV12DisplayFrameSize = GetIntParam(
        "min_NV12_display_frame_size", max(4U, minDisplayFrameSize));
    uint32_t minNV12DisplayFrameWidth =
        GetIntParam("min_NV12_display_frame_width",
                    max(minNV12DisplayFrameSize, minDisplayFrameWidth));
    uint32_t minNV12DisplayFrameHeight =
        GetIntParam("min_NV12_display_frame_height",
                    max(minNV12DisplayFrameSize, minDisplayFrameHeight));
    uint32_t nv12DfMask = ~GetIntParam("NV12_display_frame_alignment", 1) - 1;

    uint32_t minYUY2CropSize = GetIntParam("min_YUY2_crop_size", minCropSize);
    uint32_t minYUY2CropWidth =
        GetIntParam("min_YUY2_crop_width", max(minYUY2CropSize, minCropWidth));
    uint32_t minYUY2CropHeight = GetIntParam(
        "min_YUY2_crop_height", max(minYUY2CropSize, minCropHeight));
    uint32_t minYUY2DisplayFrameSize = GetIntParam(
        "min_YUY2_display_frame_size", max(4U, minDisplayFrameSize));
    uint32_t minYUY2DisplayFrameWidth =
        GetIntParam("min_YUY2_display_frame_width",
                    max(minYUY2DisplayFrameSize, minDisplayFrameWidth));
    uint32_t minYUY2DisplayFrameHeight =
        GetIntParam("min_YUY2_display_frame_height",
                    max(minYUY2DisplayFrameSize, minDisplayFrameHeight));
    uint32_t yuy2DfMask = ~GetIntParam("YUY2_display_frame_alignment", 1) - 1;

    float alignment = GetFloatParam("crop_alignment", 0.0);
    float NV12Alignment =
        GetFloatParam("NV12_crop_alignment", max(alignment, float(2.0)));
    float YUY2Alignment = GetFloatParam("YUY2_crop_alignment", alignment);

    Hwch::BufferFormatConfig deflt(
        minDisplayFrameWidth, minDisplayFrameHeight, minBufWidth, minBufHeight,
        max(int(alignment), 1), max(int(alignment), 1), alignment, minCropWidth,
        minCropHeight);
    mgr.SetDefault(deflt);

    // NV12 must not have odd width or height, or small display frame.
    Hwch::BufferFormatConfig nv12(
        minNV12DisplayFrameWidth, minNV12DisplayFrameHeight, minBufWidth,
        minBufHeight, max(int(NV12Alignment), 2), max(int(NV12Alignment), 2),
        NV12Alignment, minNV12CropWidth, minNV12CropHeight, nv12DfMask,
        nv12DfMask);
    mgr.emplace(HAL_PIXEL_FORMAT_YV12, nv12);

    // YUY2 must not have odd width.
    Hwch::BufferFormatConfig yuy2(
        minYUY2DisplayFrameWidth, minYUY2DisplayFrameHeight, minBufWidth,
        minBufHeight, max(int(YUY2Alignment), 2), max(int(YUY2Alignment), 1),
        YUY2Alignment, minYUY2CropWidth, minYUY2CropHeight, yuy2DfMask,
        yuy2DfMask);
    mgr.emplace(HAL_PIXEL_FORMAT_YCbCr_422_I, yuy2);
  }

  mArgs += UsedArgs();
}

void HwcTestRunner::SetRunnerParams() {
  mWatchdogFps = GetFloatParam("watchdog_fps", 10);
}

void HwcTestRunner::EnableDisplayFailSpoof(const char* str) {
  if (str) {
    mState->SetDisplaySpoof(&mDisplayFailSpoof);
    mDisplayFailSpoof.Configure(str);
  } else {
    mState->SetDisplaySpoof(0);
  }
}

void HwcTestRunner::ConfigureStalls() {
  ConfigureStall(Hwcval::eStallSetDisplay, "force_setdisplay_stall");
  ConfigureStall(Hwcval::eStallDPMS, "force_dpms_stall");
  ConfigureStall(Hwcval::eStallSetMode, "force_setmode_stall");
  ConfigureStall(Hwcval::eStallSuspend, "force_blank_stall");
  ConfigureStall(Hwcval::eStallResume, "force_unblank_stall");
  ConfigureStall(Hwcval::eStallHotPlug, "force_hotplug_stall");
  ConfigureStall(Hwcval::eStallHotUnplug, "force_hotunplug_stall");
  ConfigureStall(Hwcval::eStallGemWait, "force_gem_wait_stall");
}

void HwcTestRunner::ConfigureStall(Hwcval::StallType ix,
                                   const char* optionName) {
  const char* optVal = GetParam(optionName);
  if (optVal != 0) {
    // Enable stall in setdisplay, to simulate some effects of GPU hang
    mState->SetStall(ix, Hwcval::Stall(optVal, optionName));
  }
}

void HwcTestRunner::ConfigureFrameDump() {
  Hwch::Range range;
  HWCLOGD("Looking for dump_frames");
  if (GetRangeParam("dump_frames", range)) {
    HWCLOGD("Got dump_frames");
    // Max of 100 images to be dumped whatever the user has selected
    mState->ConfigureImageDump(std::shared_ptr<Hwcval::Selector>(new Hwch::Range(range)), 100);
  }

  if (GetRangeParam("dump_tgt_buffers", range)) {
    HWCLOGD("Got dump_tgt_buffers");
    mState->ConfigureTgtImageDump(std::shared_ptr<Hwcval::Selector>(new Hwch::Range(range)));
  }
}

// Called from watchdog, to log the result on test abort.
void HwcTestRunner::LogTestResult() {
  mEndTime = systemTime(SYSTEM_TIME_MONOTONIC);
  std::string testArgs = mArgs + mCurrentTest->UsedArgs();
  LogTestResult(mTestName.c_str(), testArgs.c_str());
}

void HwcTestRunner::EntryPriorityOverride() {
  HwcTestResult& result = *HwcGetTestResult();

  // Initially reduce the priority of this check so that in the log it appears
  // as a warning
  // It will be returned to ERROR at the end if enough of them happen.
  result.SetFinalPriority(eCheckOnSetLatency, ANDROID_LOG_WARN);
}

void HwcTestRunner::LogTestResult(const char* testName, const char* args) {
  HwcTestResult& result = *HwcGetTestResult();
  HwcTestConfig& config = *HwcGetTestConfig();

  // "Check OnSet Latency" is only an error if it occurs >=5 times.
  result.ConditionalRevertPriority(config, eCheckOnSetLatency, 4);

  // Record a copy of the results for this test
  std::string strTestName(testName);
  mResults[strTestName] = result;

  result.SetStartEndTime(mStartTime, mEndTime);
  result.Log(config, (std::string(testName) + std::string( args)).c_str(),
             mBrief);

  if (!result.IsGlobalFail()) {
    ++mNumPasses;
  } else {
    if (mBrief && (mHwclogPath != "")) {
      printf("Log file is %s\n", mHwclogPath.c_str());
      if ((result.mCheckFailCount[eCheckHwcCompMatchesRef] > 0)) {
        size_t pos = mHwclogPath.find("hwclog_");
        std::string dumpPath = mHwclogPath;
        if ((pos + 8) < dumpPath.length()) {
          dumpPath.insert(pos + 8,"dump_");
          dumpPath.insert(pos + 5, dumpPath.c_str() + pos + 5);
          dumpPath.append(".tgz");

          printf("Images in   %s\n", dumpPath.c_str());
        }
      }
      printf("\n");
    }

    ++mNumFails;
    mFailedTests += strTestName;
    mFailedTests += " ";
  }
}

FILE* HwcTestRunner::OpenCsvFile() {
  std::string resultsPath("/data/validation/hwc/results");

  if (mLogName != "") {
    resultsPath += "_";
    resultsPath += mLogName;
  }

  resultsPath += ".csv";
  FILE* f = fopen(resultsPath.c_str(), "w");
  HWCLOGD_COND(eLogHarness, "Writing %s", resultsPath.c_str());

  if (f == 0) {
    ALOGD("Can't write %s", resultsPath.c_str());
  }

  return f;
}

void HwcTestRunner::WriteDummyCsvFile() {
  FILE* f = OpenCsvFile();

  if (f == 0) {
    return;
  }

  fprintf(f, "Check,Component");

  for (uint32_t j = 0; j < mTestNames.size(); ++j) {
    const std::string& name = mTestNames.at(j);
    fprintf(f, ",%s", name.c_str());
  }

  fprintf(f, "\n");

  fprintf(f, "eCheckRunAbort,HWC");

  for (uint32_t j = 0; j < mTestNames.size(); ++j) {
    fprintf(f, ",1");
  }

  fprintf(f, "\n");
  fclose(f);
  HWCLOGD_COND(eLogHarness, "Dummy csv file written.");
}

void HwcTestRunner::WriteCsvFile() {
  HwcTestConfig& config = *HwcGetTestConfig();
  FILE* f = OpenCsvFile();

  if (f == 0) {
    return;
  }

  fprintf(f, "Check,Component");

  for (std::map<std::string, HwcTestResult>::iterator itr = mResults.begin();itr != mResults.end(); ++itr) {
    fprintf(f, ",%s", itr->first.c_str());
  }

  fprintf(f, "\n");

  for (uint32_t i = 0; i < eHwcTestNumChecks; ++i) {
    HwcCheckConfig checkConfig = config.mCheckConfigs[i];
    if (checkConfig.enable && (checkConfig.priority >= ANDROID_LOG_ERROR)) {
      fprintf(f, "%s,%s", HwcTestConfig::GetDescription(i),
              HwcTestConfig::GetComponentName(i));

      for (std::map<std::string, HwcTestResult>::iterator itr = mResults.begin();itr != mResults.end(); ++itr) {
        HwcTestResult& result = itr->second;

        uint32_t failures = result.mCheckFailCount[i];
        uint32_t evals = result.mCheckEvalCount[i];
        uint32_t finalPriority = result.mFinalPriority[i];

        if (evals > 0) {
          if (finalPriority >= ANDROID_LOG_ERROR) {
            fprintf(f, ",%d", failures);
          } else {
            // Computed final priority for this condition was warning.
            // That means that we passed some overall condition as to how many
            // times this condition
            // may occur, and therefore permitted a downgrade to warning, which
            // here means a pass.
            fprintf(f, ",0");
          }
        } else {
          fprintf(f, ",");
        }
      }

      fprintf(f, "\n");
    }
  }

  fclose(f);
  HWCLOGD_COND(eLogHarness, "Real CSV file written");
}

void HwcTestRunner::ParseCSV(const char* p,
                             std::vector<std::string>& sv) {
  while (p) {
    while (*p && !isprint(*p)) {
      ++p;
    }

    std::string field;
    const char* ep = strchr(p, ',');
    if (ep == 0) {
      for (ep = p; (*ep && isprint(*ep)); ++ep)
        ;

      if (ep > p) {
        field.assign(p, ep - p);
      }
      sv.push_back(field);
      p = 0;
    } else {
      if (ep > p) {
        field.assign(p, ep - p);
      }

      sv.push_back(field);
      p = ep + 1;
    }
  }
}

void HwcTestRunner::CombineFiles(int err) {
  DIR* d;
  struct dirent* dir;

  const char* directory = "/data/validation/hwc/";
  d = opendir(directory);

  // Search for input files matching the pattern
  if (d) {
    // Open output file
    FILE* out = fopen("resultscombined.csv", "w");

    if (out) {
      HWCLOGD_COND(eLogHarness, "Writing %s", "resultscombined.csv");

      if (err) {
        fprintf(out, "Test Pass/Fail/Error,%d,%d,%d\n", 0, 0, 1);
      } else {
        fprintf(out, "Test Pass/Fail/Error,%d,%d,%d\n", mNumPasses, mNumFails,
                0);
      }

      std::vector<std::string> files;
      std::vector<std::string> allTests;
      uint32_t numColumns = 0;
      uint32_t prevNumColumns = 0;

      struct CheckResData {
        std::string component;
        std::vector<std::string> res;
      };

      std::map<std::string, CheckResData> results;

      while ((dir = readdir(d)) != 0) {
        char* name = dir->d_name;
        int namelen = strlen(name);

        if ((strcmp(name, "results.csv") == 0) ||
            ((strncmp(name, "results_", 8) == 0) &&
             (strcmp(name + namelen - 4, ".csv") == 0))) {
          char path[256];
          strcpy(path, directory);
          strcat(path, name);

          // We've found an input file. Open it.
          FILE* in = fopen(path, "r");

          if (in) {
            char* line = 0;
            size_t len = 0;
            std::vector<std::string> tests;

            // Parse header line
            if ((getline(&line, &len, in)) != -1) {
              if (strncmp(line, "Check,Component,", 16) != 0) {
                printf("Invalid results file %s\n", name);
                free(line);
                fclose(in);
                continue;
              }

              // Add to our array of tests
              ParseCSV(line + 16, tests);
              allTests += tests;

              // and ensure we know which file each one came from.
              for (uint32_t i = 0; i < tests.size(); ++i) {
                files.push_back(std::string(name));
              }

              prevNumColumns = numColumns;
              numColumns = allTests.size();

              free(line);
            }

            // Parse each body line
            line = 0;
            len = 0;
            while ((getline(&line, &len, in)) != -1) {
              std::vector<std::string> checkResults;
              ParseCSV(line, checkResults);
              if (checkResults.size() < 3) {
                continue;
              }

              std::string check = checkResults[0];
              std::string component = checkResults[1];
              checkResults.erase(checkResults.cbegin(), checkResults.cbegin() + 2);

              while (checkResults.size() < tests.size()) {
                checkResults.push_back(std::string());
              }

              if (results.find(check) == results.end()) {
                CheckResData rd;
                rd.component = component;
                for (uint32_t i = 0; i < prevNumColumns; ++i) {
                  rd.res.push_back(std::string());
                }

                results.emplace(check, rd);
              }

              CheckResData& rd = results[check];

              if (rd.component != component) {
                printf("CombineFiles: component inconsistency! %s %s %s\n",
                       check.c_str(), component.c_str(),
                       rd.component.c_str());
              }

              rd.res += checkResults;
            }

            free(line);
            fclose(in);
            line = 0;
            len = 0;
          }
        }
      }

      fprintf(out, ",");
      for (uint32_t i = 0; i < files.size(); ++i) {
        fprintf(out, ",%s", files[i].c_str());
      }
      fprintf(out, "\n");

      fprintf(out, "Check,Component");
      for (uint32_t i = 0; i < allTests.size(); ++i) {
        fprintf(out, ",%s", allTests.at(i).c_str());
      }
      fprintf(out, "\n");

      for (std::map<std::string, CheckResData>::iterator itr = results.begin();itr != results.end(); ++itr) {
        const std::string& check = itr->first;
        CheckResData& rd = itr->second;

        fprintf(out, "%s,%s", check.c_str(), rd.component.c_str());

        for (uint32_t r = 0; r < numColumns; ++r) {
          if (r < rd.res.size()) {
            fprintf(out, ",%s", rd.res.at(r).c_str());
          } else {
            fprintf(out, ",");
          }
        }

        fprintf(out, "\n");
      }

      fclose(out);
      HWCLOGD_COND(eLogHarness, "Written resultscombined.csv");
      closedir(d);
    } else {
      printf("ERROR: Failed to open %s for write\n", "resultscombined.csv");
    }
  }
}

void HwcTestRunner::LogSummary() {
  if (!mBrief) {
    if ((mNumFails > 0) && ((mNumPasses + mNumFails) > 1)) {
      printf("Failed Tests: %s\n", mFailedTests.c_str());
    }

    printf("Passed : %d\n", mNumPasses);
    printf("Failed : %d\n", mNumFails);
    printf("Skipped: 0\n");
    printf("Error  : 0\n");
  }
}

int HwcTestRunner::CreateTests() {
  // Provide default output csv file in case the test crashes
  if (GetParam("logname")) {
    mLogName = GetStrParam("logname");
  }

  if (GetParam("hwclogpath")) {
    mHwclogPath = GetStrParam("hwclogpath");
  }

  HwcTestConfig& config = *HwcGetTestConfig();
  int rc = 0;

  bool valHwc = (GetParam("no_val_hwc") == 0);
  bool valDisplays = (GetParam("no_val_displays") == 0);
  bool valBuffers = (GetParam("val_buffer_allocation") != 0);
  bool valSf = (GetParam("val_sf") != 0);

  UsedArgs() = "";
  config.Initialise(valHwc, valDisplays, valBuffers, valSf,
                    (GetParam("val_hwc_composition") != 0));

  // Jenkins
  mArgs += UsedArgs();

  // Use long fence timeout to avoid SEGVs if we are doing
  // composition buffer comparison
  uint32_t fenceTimeoutMs = HWCH_FENCE_TIMEOUT;
  if ((config.mCheckConfigs[eCheckHwcCompMatchesRef].enable) ||
      (config.mCheckConfigs[eCheckSfCompMatchesRef].enable)) {
    HWCLOGD("Set fence timeout to 10 sec");
    fenceTimeoutMs = 10000;  // 10 sec
  }

  fenceTimeoutMs =
      GetTimeParamUs("fence_timeout", fenceTimeoutMs * HWCVAL_MS_TO_US) /
      HWCVAL_MS_TO_US;
  mSystem.SetFenceTimeout(fenceTimeoutMs);

  mTestNum = 0;

  if (mHWCLReplay || mDSReplay || mReplayTest) {
    // Run a replay scenario
    Hwch::ReplayRunner* replay = 0;

    if (mHWCLReplay) {
      int32_t mAlpha = GetIntParam("replay_alpha", 0xFF);

      if (mAlpha < 0) {
        HWCLOGW("Replay alpha value is negative - capping to 0");
        mAlpha = 0;
      } else if (mAlpha > 0xFF) {
        HWCLOGW("Replay alpha value is out-of-range - capping to 255");
        mAlpha = 0xFF;
      }

      replay = new Hwch::ReplayHWCLRunner(mInterface, mReplayFileName,
                                          mReplayMatch, mReplayNoTiming, mAlpha);
    } else if (mDSReplay) {
      replay = new Hwch::ReplayDSRunner(mInterface, mReplayFileName,
                                        mDSReplayNumFrames);
    } else if (mReplayTest) {
      // The unit-tests run on the HWCL regular expressions. Create a dummy HWCL
      // runner
      // so that we can access the parser.
      replay = new Hwch::ReplayHWCLRunner(mInterface, "", 0, false, 0);
    } else {
      HWCERROR(eCheckCommandLineParam,
               "Unsupported sequence of replay command-line options");
      rc = -1;
    }

    replay->SetName((std::string("Replay ") + std::string(mReplayFileName)).c_str());

    if (replay->IsReady() || mReplayTest) {
      // Run the replay or run the unit-tests depending on the command-line
      // options
      if (!mReplayTest) {
        mTests.push_back(replay);
      } else {
#ifdef PARSER_DEBUG
        replay->RunParserUnitTests();
#else
        std::printf("Parser unit tests are disabled in this build.\n");
#endif
        return 1;
      }
    } else {
      rc = -1;
    }
  } else {
    // Build a list of tests to run
    if (mAllTests) {
      Hwch::BaseReg::mHead->AllMandatoryTests(mInterface, mTests);
      for (uint32_t i = 0; i < mTests.size(); ++i) {
        std::string name(mTests.at(i)->GetName());
        bool avoid = false;

        for (uint32_t j = 0; j < mAvoidNames.size(); ++j) {
          if (name == mAvoidNames.at(j)) {
            avoid = true;
            break;
          }
        }

        if (!avoid) {
          mTestNames.push_back(name);
        } else {
          mTests.erase(mTests.cbegin() + i);
          --i;
        }
      }
    } else {
      for (uint32_t i = 0; i < mTestNames.size(); ++i) {
        const std::string& testName = mTestNames[i];
        Hwch::Test* test =
            Hwch::BaseReg::mHead->CreateTest(testName.c_str(), mInterface);

        if (test) {
          mTests.push_back(test);
        } else {
          printf("No such test: %s\n", testName.c_str());
        }
      }
    }
  }

  WriteDummyCsvFile();
  CombineFiles(1);

  return rc;
}

void HwcTestRunner::ConfigureState() {
  // This is stuff that needs to be done after teh HwcTestKernel has been
  // created.
  //
  // Do we connect HDMI if it is available?
  if (GetParam("no_hdmi")) {
    mSystem.SetHDMIToBeTested(false);
    mState->SimulateHotPlug(false);
  }
}

int HwcTestRunner::RunTests() {
  if (mTests.size() == 0) {
    printf("*** No valid test specified.\n");
    return 0;
  }

  int rc = 0;

  // List the tests we are about to run
  std::string allTestNames;
  for (uint32_t i = 0; i < mTests.size(); ++i) {
    Hwch::Test* test = mTests.at(i);

    allTestNames += test->GetName();
    allTestNames += " ";
  }

  printf("RUNNING TESTS: %s\n", allTestNames.c_str());

  if (mState->IsOptionEnabled(eOptKmsgLogging)) {
    mState->LogToKmsg("RUNNING TESTS: %s\n", allTestNames.c_str());
  }

  // Open the statistics file
  mStatsFile = fopen("statistics.csv", "w");

  // Save starting config for each test
  Lock();
  HwcTestConfig& config = *HwcGetTestConfig();
  HwcTestConfig testInitConfig = config;

  // Run the tests in the list
  for (uint32_t i = 0; i < mTests.size(); ++i) {
    mCurrentTest = mTests.at(i);
    mTestName = mCurrentTest->GetName();

    // Restore the config, in case it was modified by the last test
    config = testInitConfig;

    // Clear the test results
    HwcGetTestResult()->Reset(&config);

    // Set the test-specific parameters
    mCurrentTest->SetParams(mParams);
    mCurrentTest->UsedArgs() = "";

    // Reset any statistical counters
    Hwcval::Statistics::getInstance().Clear();

    HWCLOGA("============ Starting test %s ============", mTestName.c_str());
    if (mState->IsOptionEnabled(eOptKmsgLogging)) {
      mState->LogToKmsg("============ Starting test %s ============\n",
                        mTestName.c_str());
    }

    if (!mBrief && (mTests.size() > 1)) {
      printf("TEST: %s\n", mTestName.c_str());
    }

    mWatchdog.Set(10, mWatchdogFps);
    mWatchdog.Start();
    if ((mState->IsCheckEnabled(eCheckHwcCompMatchesRef))) {
      // Composition validation slows everything down
      mWatchdog.Set(10, min(mWatchdogFps, float(4)));
    }

    Unlock();

    // Take copy of check priorities (severities) in results, so the test can
    // amend them
    // based on things like how often they occurred.
    HwcGetTestResult()->CopyPriorities(config);
    EntryPriorityOverride();

    mStartTime = systemTime(SYSTEM_TIME_MONOTONIC);
    rc = mCurrentTest->Run();
    mEndTime = systemTime(SYSTEM_TIME_MONOTONIC);

    mWatchdog.Stop();

    CRCTerminate(config);

    std::string testArgs = mArgs + mCurrentTest->UsedArgs();

    Lock();
    delete mCurrentTest;
    mCurrentTest = 0;

    HWCLOGA("============ Finished test %s %s ============", mTestName.c_str(),
            testArgs.c_str());
    if (mState->IsOptionEnabled(eOptKmsgLogging)) {
      mState->LogToKmsg("============ Finished test %s %s ============\n",
                        mTestName.c_str(), testArgs.c_str());
    }

    ExitChecks();
    mRunTimeStat.Set(double(mEndTime - mStartTime) / HWCVAL_SEC_TO_NS);
    Hwcval::Statistics::getInstance().Dump(mStatsFile, mTestName.c_str());
    mState->ReportFrameCounts();

    // If we think the shims should be running, make sure they are.
    if (!mNoShims) {
      mState->CheckRunningShims(HwcTestState::eHwcShim | HwcTestState::eDrmShim);
    }

    LogTestResult(mTestName.c_str(), testArgs.c_str());

    if (HwcTestState::getInstance()->IsTotalDisplayFail()) {
      mSystem.AddEvent(Hwch::AsyncEvent::eBlank, -1);
      sleep(5);
      // Turn off display
      Wake(false, 0);
      sleep(5);

      printf("\nTOTAL DISPLAY FAIL: PLEASE REBOOT\n");
      Hwch::System::QuickExit(-2);
    }
  }

  config.DisableAllChecks();
  LogSummary();
  WriteCsvFile();
  CombineFiles(0);
  fclose(mStatsFile);
  Unlock();

  std::string parametersNotChecked;
  for (std::map<std::string, Hwch::UserParam>::iterator itr = mParams.begin();itr != mParams.end(); ++itr) {
    Hwch::UserParam& up = itr->second;
    const std::string& pname = itr->first;

    if (!up.mChecked) {
      parametersNotChecked += "-";
      parametersNotChecked += pname;
      parametersNotChecked += " ";
    }
  }

  if (parametersNotChecked != "" && !mBrief) {
    printf("WARNING: Parameters %snot used\n", parametersNotChecked.c_str());
  }

  return rc;
}

void HwcTestRunner::CRCTerminate(HwcTestConfig& config) {
  if (config.mCheckConfigs[eCheckCRC].enable) {
    const int cLoopWaitMilliseconds = 100;
    const int cLoopMax = 10;
    int loop;

    config.SetCheck(eCheckCRC, false);

    for (loop = 0; loop < cLoopMax &&
                       HwcTestState::getInstance()->IsFrameControlEnabled();
         ++loop) {
      usleep(cLoopWaitMilliseconds * 1000);
    }

    if (loop == cLoopMax) {
      HWCLOGW(
          "HwcTestRunner::RunTests - ERROR: TIMED OUT (after %dms) WAITING FOR "
          "FRAME RELEASE",
          loop * cLoopWaitMilliseconds);
    }

    HWCLOGD("HwcTestRunner::RunTests - released after %dms",
            loop * cLoopWaitMilliseconds);
    config.SetCheck(eCheckCRC, true);
  }
}

void HwcTestRunner::ExitChecks() {
  // Close FBT fences
  for (uint32_t disp = 0; disp < HWCVAL_MAX_CRTCS; ++disp) {
    Hwch::Display& display = mSystem.GetDisplay(disp);

    if (display.IsConnected()) {
      Hwch::Layer& layer = display.GetFramebufferTarget();

      if (layer.mBufs != 0) {
        layer.mBufs->CloseAllFences();
      }
    }
  }

  // print out /d/sync in ALOG
  // and search for leaked fences
  FILE* f = fopen("/d/sync", "r");
  const int bufsize = 1024;
  if (f) {
    uint32_t numLeakedFences = 0;
    ALOGD(" ");
    ALOGD("/d/sync on HWC Harness exit:");
    ALOGD("============================");
    char buf[bufsize];

    while (fgets(buf, sizeof(buf), f) != 0) {
      buf[bufsize - 1] = 0;
      int len = strlen(buf);

      while ((len > 0) && !isprint(buf[len - 1])) {
        buf[--len] = '\0';
      }

      if (strstr(buf, "hwcharness_pt signaled") != 0) {
        ++numLeakedFences;
      }

      ALOGD("%s", buf);
    }

    HWCCHECK(eCheckFenceLeak);
    if (numLeakedFences > 0) {
      HWCERROR(eCheckFenceLeak, "%d hwcharness_pt fences", numLeakedFences);
      // The above will set the eror count on this check to 1.
      // Let's set it to the actual number of leaked fences instead
      // by adding a further (numLeakedFences-1) fails.
      ::HwcGetTestResult()->SetFail(eCheckFenceLeak, numLeakedFences - 1);
    }

    fclose(f);
  }

  HWCCHECK(eCheckRunAbort);  // Log that test did not exit prematurely
}

void HwcTestRunner::Lock() {
  mExitMutex.lock();
}

void HwcTestRunner::Unlock() {
  mExitMutex.unlock();
}

void HwcTestRunner::ReportVersion() {
  std::string hwcBinVersion;
  HWCSHANDLE hwcs = HwcService_Connect();
  hwcBinVersion.assign(HwcService_GetHwcVersion(hwcs));
  HwcService_Disconnect(hwcs);

  std::vector<std::string> hwcVersionWords = splitString(hwcBinVersion);

  std::string sha;
 
  if (hwcVersionWords.size() >= 3) {
    if (hwcVersionWords[0].compare("VERSION:") == 0) {
      sha.assign(hwcVersionWords[2]);
    } else {
      // sha is at the index 7, hence changing accordingly
      sha.assign(hwcVersionWords[7]);
    }
  }

  if (sha.compare(S(HWC_VERSION_GIT_SHA)) != 0) {
    // We built against a different HWC source than which we are runnign
    // against.
    if (mBrief || (GetParam("version") != 0)) {
      printf("HWC VERSION INCONSISTENCY:\n");
      printf("HWC version (running):             %s\n", hwcBinVersion.c_str());
      printf("HWC version (for HWCVAL includes): %s %s\n",
             S(HWC_VERSION_GIT_BRANCH), S(HWC_VERSION_GIT_SHA));
      printf("HWCVAL version:                    %s %s\n",
             S(HWCVAL_VERSION_GIT_BRANCH), S(HWCVAL_VERSION_GIT_SHA));
    }

    // If we are not Jenkins, this should be a warning.
    if (!mBrief) {
      HwcGetTestConfig()->mCheckConfigs[eCheckHwcVersion].priority =
          ANDROID_LOG_WARN;
    }

    HWCERROR(
        eCheckHwcVersion,
        "Running HWC version:         %s\nHWCVAL uses include files in %s %s\n",
        hwcBinVersion.c_str(), S(HWC_VERSION_GIT_BRANCH),
        S(HWC_VERSION_GIT_SHA));

  } else if (GetParam("version")) {
    printf("HWC version:    %s\n", hwcBinVersion.c_str());
    printf("HWCVAL version: %s %s\n", S(HWCVAL_VERSION_GIT_BRANCH),
           S(HWCVAL_VERSION_GIT_SHA));
  }
}

int main(int argc, char** argv) {
  Hwch::Interface interface;
  Hwch::AsyncEventGenerator eventGen(interface);
  Hwch::KernelEventGenerator kernelEventGen;

  HwcTestRunner runner(interface);

  // Check command line args
  if (!runner.getargs(argc, argv)) {
    Hwch::System::QuickExit(-1);
  }

  HWCLOGA("Version: %s", VERSION_NUMBER);

  int status = runner.CreateTests();
  if (status) {
    Hwch::System::QuickExit(status);
  }

  int backlight = runner.GetIntParam("backlight", -1);
  Wake(true, backlight);

  runner.SetBufferConfig();

  // Start the thread pool so that services will work
  sp<ProcessState> proc(ProcessState::self());
  ProcessState::self()->startThreadPool();

  // Virtual Display Emulation Support.
  //
  // Note: currently, if the Virtual Display support is enabled, the virtual
  // display is created on display 2 i.e. D2.
  Hwch::System& system = Hwch::System::getInstance();
  if (system.IsVirtualDisplayEmulationEnabled()) {
    HWCLOGI("Initialising Virtual Display Support\n");
    system.GetDisplay(HWCVAL_DISPLAY_ID_VIRTUAL).EmulateVirtualDisplay();
  }

  // Configure choice of patterns
  system.GetPatternMgr().Configure(
      HwcTestState::getInstance()->IsOptionEnabled(eOptForceGlFill),
      HwcTestState::getInstance()->IsOptionEnabled(eOptForceCPUFill));

  // Set up initial hotplug connection state
  runner.ConfigureState();

  // Initialise and prepare the HWC
  interface.Initialise();
  interface.RegisterProcs();
  interface.GetDisplayAttributes();

  if (interface.NumDisplays() == 0) {
    printf("Error: No displays available. Exiting.\n");
    exit(1);
  }

  system.SetDefaultNumBuffers(
      runner.GetIntParam("default_num_buffers", HWCH_DEFAULT_NUM_BUFFERS));

  system.GetVSync().SetVSyncDelay(
      runner.GetTimeParamUs("vsync_delay", 6500));  // default 6.5 ms
  system.GetVSync().SetTimeout(
      runner.GetTimeParamUs("vsync_timeout", 50000));  // default 50 ms
  system.GetVSync().SetRequestedVSyncPeriod(runner.GetTimeParamUs(
      "vsync_period", 16666));  // default 16.666 ms i.e. 60 Hz.

  system.GetKernelEventGenerator().SetEsdConnectorId(
      runner.GetIntParam("esd_connector_id", 0));

  runner.SetRunnerParams();

  if (runner.GetParam("no_gl") == 0) {
    system.EnableGl();
  }

  // What are we synchronizing to?
  const char* syncToStr = runner.GetStrParam("sync_to");
  if (strcmp(syncToStr, "") != 0) {
    system.SetSyncOption(syncToStr);
  }

  if (system.GetSyncOption() == Hwch::eNone) {
    system.GetVSync().Stop();
  } else {
    interface.EventControl(0, HWC_EVENT_VSYNC, 1);

    // HWC no longer supports collection of VSyncs from display 1.
    // Note that when panel is turned off in Extended Mode, Vsyncs from display
    // 1
    // will be forwarded to display 0.
    // interface.EventControl(1, HWC_EVENT_VSYNC, 1);
  }

  // Wait so HWC Log Viewer can catch up
  sleep(1);

  system.CreateFramebufferTargets();

  runner.ReportVersion();

  status = runner.RunTests();

  // Turn off display
  Wake(false, 0);

  // Avoid HWC closedown errors
  // Hopefully this is a temporary fix
  fflush(stdout);
  Hwch::System::QuickExit();

  HWCLOGD("Harness stopping threads before exit...");
  HwcTestState::getInstance()->StopThreads();
  HWCLOGD("Harness stopped threads.");

  system.die();

  HWCLOGD("Leaving main");
  return status;
}
