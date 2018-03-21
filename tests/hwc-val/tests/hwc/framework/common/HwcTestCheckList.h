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

//==============================================================================
// TEST OPTIONS
//
// List of options which can be added to the tests.
// Some of these options can be provided alternatively via parameter added to
// the command line.
//
//==============================================================================
// Option to delay 1 in 5 the page flip events on D0 by 500ms. Used to test
// out-of-order fence behaviour.
DECLARE_CHECK(eOptDelayPF, None, INFO,
              "Delay some page flips by around a second", Opt)

// Option to enable destruction of buffers on a separate thread. Used to test
// Gralloc/HWC handshake.
DECLARE_CHECK(eOptAsyncBufferDestruction, None, INFO,
              "Harness defers buffer destruction to a thread", Opt)

// If this option is enabled, VBlank/VSync capture callback is restored
// automatically if
// there is a VSync timeout.
DECLARE_CHECK(eOptAutoRestoreVSync, None, INFO,
              "Restore VSync capture after vsync timeout", Opt)

// Option to enable the new style Multi-Display interface using HWC services
DECLARE_CHECK(eOptNewMds, None, INFO, "New Multi-Display Service interface",
              Opt)

// Option to use input timeout processing and video frame rate detection
// encapsulated within HWC
DECLARE_CHECK(eOptNoMds, None, INFO,
              "Multi-Display capabilities encapsulated within HWC", Opt)

// Set display output format in the style required by Jenkins and valsmoke.
DECLARE_CHECK(eOptBrief, None, INFO, "Set brief mode for standard output", Opt)

// Option to modify mode lists for better hotplug validation
DECLARE_CHECK(eOptRandomizeModes, None, INFO,
              "Randomly change the number and order of modes on a hot plug",
              Opt)

// Option to force all display frames to be inside the screen area.
// Intended for internal use.
DECLARE_CHECK(eOptDispFrameAlwaysInsideScreen, None, INFO,
              "Force display frame to always be inside the screen area", Opt)

// Force buffer filling using GL
DECLARE_CHECK(eOptForceGlFill, None, INFO,
              "Force buffers to be filled using GL", Opt)

// Force buffer filling without GL
DECLARE_CHECK(eOptForceCPUFill, None, INFO,
              "Force buffers to be filled using CPU", Opt)

// Block any SetDisplay calls where we know the contents are invalid
// If invalid contents are detected, the shim will return -1 from the SetDisplay
// without calling DRM.
DECLARE_CHECK(eOptBlockInvalidSetDisplay, None, INFO,
              "Block drmModeSetDisplay call with invalid parameters", Opt)

// Multiple simultaneous blank/unblank is disabled by default because it can
// cause lockup
DECLARE_CHECK(eOptSimultaneousBlank, None, INFO,
              "Multiple simultaneous blank/unblanks permitted", Opt)

// Enable Kmsg Logging (useful for debugging DRM Calls)
DECLARE_CHECK(eOptKmsgLogging, None, INFO, "Enable Kmsg Logging", Opt)

// Pretend the panel is an HDMI for display proxy testing
DECLARE_CHECK(eOptSpoofNoPanel, None, INFO, "Pretend panel is HDMI", Opt)

// Pretend Dynamic Refresh Rate Setting is enabled on the panel
DECLARE_CHECK(eOptSpoofDRRS, None, INFO,
              "Let HWC think DRRS is enabled even if kernel does not think so",
              Opt)
// "Real" VSyncs will be enabled all the time by the shims - only passed on to
// HWC when requested
DECLARE_CHECK(eOptVSyncInterception, None, INFO, "Intercept VSyncs", Opt)

// "Real" Page Flips will be enabled all the time by the shims - only passed on
// to HWC when requested
// This should only be enabled if VSync interception is also enabled.
DECLARE_CHECK(eOptPageFlipInterception, None, INFO, "Intercept Page Flips", Opt)

// Keep frame numbers for each display distinct, even under HWC 1.5
DECLARE_CHECK(eOptDivergeFrameNumbers, None, INFO,
              "Distinct frame numbers for each display even under HWC 1.5", Opt)

//==============================================================================
// TEST LOG ENABLES
//
// When set, they enable log messages from a specific component
//
//==============================================================================
DECLARE_CHECK(eLogAllIoctls, None, INFO, "Enable logs for all DRM IOCTLs", Dbg)
DECLARE_CHECK(
    eLogBuffer, None, INFO,
    "Enable logs for reporting buffer creation, destruction and usage in HWC",
    Dbg)
DECLARE_CHECK(eLogCloning, None, INFO, "Enable logs for cloning", Dbg)
DECLARE_CHECK(eLogCombinedTransform, None, INFO,
              "Enable logs for Combined Transforms", Dbg)
DECLARE_CHECK(eLogCRC, None, INFO,
              "Enable logs for CRC-based flicker detection", Dbg)
DECLARE_CHECK(eLogCroppedTransform, None, INFO,
              "Enable logs for cropped transforms", Dbg)
DECLARE_CHECK(eLogDebugDebug, None, INFO, "Enable logs for debugging the debug",
              Dbg)
DECLARE_CHECK(eLogDrm, None, INFO, "Enable logs for DRM category", Dbg)
DECLARE_CHECK(eLogEventHandler, None, INFO,
              "Enable logs for event handler/VSyncs", Dbg)
DECLARE_CHECK(eLogEventQueue, None, INFO,
              "Enable logs for event queues and event threads", Dbg)
DECLARE_CHECK(eLogFence, None, INFO, "Enable logs for fence issues", Dbg)
DECLARE_CHECK(eLogFlicker, None, INFO, "Enable logs for flicker detection", Dbg)
DECLARE_CHECK(eLogHarness, None, INFO, "Enable logs for HWC harness", Dbg)
DECLARE_CHECK(eLogHarnessVSync, None, INFO,
              "Enable logs for HWC harness VSync processing", Dbg)
DECLARE_CHECK(eLogHotPlug, None, INFO, "Enable logs for hotplug simulation",
              Dbg)
DECLARE_CHECK(eLogHwchInterface, None, INFO,
              "Enable logs for HWC harness interface to HWC", Dbg)
DECLARE_CHECK(eLogLLQ, None, INFO, "Enable logs for Layer List Queue", Dbg)
DECLARE_CHECK(eLogLLQContents, None, INFO,
              "Enable logging of Layer List Queue contents", Dbg)
DECLARE_CHECK(eLogNuclear, None, INFO, "Enable logs for Nuclear DRM", Dbg)
DECLARE_CHECK(eLogParse, None, INFO, "Enable logs for log parser", Dbg)
DECLARE_CHECK(eLogOptionParse, None, INFO,
              "Enable logs for parsing HWC options", Dbg)
DECLARE_CHECK(eLogVideo, None, INFO, "Enable logs for video modes", Dbg)
DECLARE_CHECK(eLogVisibleRegions, None, INFO, "Enable logs for visible regions",
              Dbg)
DECLARE_CHECK(eLogStall, None, INFO, "Enable logs for forced stalls", Dbg)
DECLARE_CHECK(eLogLayerAlloc, None, INFO,
              "Enable logs for harness layer allocation", Dbg)
DECLARE_CHECK(eLogGl, None, INFO, "Enable logs for GL", Dbg)
DECLARE_CHECK(eLogResources, None, INFO,
              "Enable logging of process resource usage", Dbg)
DECLARE_CHECK(eLogRenderCompression, None, INFO,
              "Enable logging for render compression", Dbg)
DECLARE_CHECK(eLogVBlank, None, INFO, "Enable logging for real VBlanks", Dbg)
DECLARE_CHECK(eLogHwcDisplayConfigs, None, INFO,
              "Log all available display configs on each hot plug", Dbg)
DECLARE_CHECK(eLogMosaic, None, INFO, "Enable mosaic display logging", Dbg)

// State counters
DECLARE_CHECK(eCountHwcComposition, None, INFO, "Count of HWC compositions",
              Test)

//==============================================================================
// TEST ERRORS - Test Component
// These are test errors, whose level of severity can be WARN, ERROR or FATAL.
// These all indicate that the test has detected a problem with its internal
// state or has failed to complete successfully.
// All conditions from the test should be considered as suspect if one of these
// arises.
//==============================================================================

// Indicate a software problem or a misconfiguration of the drm shims.
DECLARE_CHECK(eCheckDrmShimFail, Test, FATAL, "Drm Shim Failure", Test)

// The HWC validation harness framework has detected a software problem in the
// design of the test.
DECLARE_CHECK(eCheckFrameworkProgError, Test, ERROR,
              "Error in programming the test framework", Test)

// Generic internal error.
DECLARE_CHECK(eCheckInternalError, Test, ERROR,
              "Internal error detected in shims", Test)

// Command-line parameter or option is invalid
DECLARE_CHECK(eCheckCommandLineParam, Test, ERROR,
              "Invalid command-line parameter or option", Test)

// The composition complexity was such that HWC validation was unable to handle
// the Z-orders correctly.
// Please ignore any Z-order errors. Developers: consider increasing z-order
// nesting depth from 4 to 8.
DECLARE_CHECK(eCheckInternalZOrder, Test, ERROR,
              "Internal Z-order conflict: Ignore Z-order errors", Test)

// Error because MDS protocol is not followed.
DECLARE_CHECK(eCheckMdsProtocol, Test, ERROR, "MDS Protocol not followed", Test)

// The harness has aborted the test because the frame rate has fallen below a
// pre-determined limit.
// Normally this means that something has locked up and the test is unable to
// make any progress.
DECLARE_CHECK(eCheckTooSlow, Test, ERROR, "Frame rate too low", Test)

// Warning on excessive number of buffers which may cause internal leak.
DECLARE_CHECK(eCheckObjectLeak, Test, WARN,
              "Internal data structures have grown very large - possible leak",
              Test)

// Replay file or parser not correct.
DECLARE_CHECK(eCheckReplayFail, Test, FATAL, "Replay Failure", Test)

// Illustrate errors in buffers or buffer objects creation and configuration.
DECLARE_CHECK(eCheckTestBufferAlloc, Test, ERROR,
              "Error in buffer configuration", Test)

// Option selected not valid in this build
DECLARE_CHECK(eCheckFacilityNotAvailable, Test, ERROR,
              "Selected option not available in this configuration", Test)

// HDMI required for some feature of this test
DECLARE_CHECK(eCheckHdmiReq, Test, WARN,
              "HDMI not connected - some test features not exercised", PriWarn)

// Option selected not valid in this build
DECLARE_CHECK(eCheckScreenNotBigEnough, Test, ERROR,
              "Screen not big enough to run this test", Test)

// Any problem with the test which means we should report the problem and
// immediately abort.
DECLARE_CHECK(eCheckSessionFail, Test, FATAL, "Fatal Test Failure", Test)

// Any problem with the test itself -> the test results may not be valid.
DECLARE_CHECK(eCheckTestFail, Test, ERROR, "Test Failure", Test)

// Any problem with GL
DECLARE_CHECK(eCheckGlFail, Test, WARN, "GL failure", Test)
// HWC shim failure to run-time link to real HWC
DECLARE_CHECK(eCheckHwcBind, Test, FATAL,
              "HWC shim failed run-time linking to real HWC", StickyTest)

// Failure to run-time link to DRM
DECLARE_CHECK(eCheckDrmShimBind, Test, FATAL, "Failed run-time linking to DRM",
              StickyTest)

// Failure to run-time link to MDS
DECLARE_CHECK(eCheckMdsBind, Test, ERROR,
              "Failed to bind to Multi-Display Service", StickyTest)

// Run-time failure shimming HWC service
DECLARE_CHECK(eCheckHwcServiceBind, Test, ERROR,
              "Failed to bind to HWC service", StickyTest)

// File error, including file not found
DECLARE_CHECK(eCheckFileError, Test, ERROR, "File access error", Test)

// Png error
DECLARE_CHECK(eCheckPngFail, Test, ERROR, "PNG error", Test)

// Legacy test error
DECLARE_CHECK(eCheckSurfaceSender, Test, ERROR, "Surface sender error", Test)

// Log Parser Error
DECLARE_CHECK(eCheckUnknownHWCAPICall, Test, ERROR, "Unknown HWC API call",
              Test)
DECLARE_CHECK(eCheckLogParserError, Test, ERROR, "Log parser error", Test)

// Failed to query internally copied fence.
// This means either (a) someone else has closed our fence or (b) for some
// reason the
// API we use to query the fence is unable to obtain the data.
DECLARE_CHECK(eCheckFenceQueryFail, Test, WARN,
              "Failed to query internal fence", Test)

// Transparency filter failed to detect a layer we know is transparent
DECLARE_CHECK(eCheckTransparencyDetectionFailure, Test, WARN,
              "Transparency detection failure", PriWarn)

// HWC running version inconsistent with version validation was built for
DECLARE_CHECK(eCheckHwcVersion, Test, ERROR,
              "HWC version inconsistency detected", StickyTest)

// Async event generator can sometimes produce events much more quickly than HWC
// can consume them.
DECLARE_CHECK(eCheckAsyncEventsDropped, Test, WARN,
              "Harness dropped async events because they could not be consumed "
              "fast enough",
              Test)

// HWC has logged a pointer using incorrect formatting (64-bit pointer may be
// logged as 32)
DECLARE_CHECK(
    eCheckBadPointerFormat, Test, ERROR,
    "HWC used incorrect formatting for a pointer value: may be truncated", Test)

//==============================================================================
// VALIDATION FAILURES
//
// These are real errors, whose level of severity can be WARN, ERROR or FATAL.
// They are categorized by component they refer to (HWC, SF, Display...),
// and within each component different categories can be identified
// (i.e. Hwc, HwcDisplay..).
//==============================================================================

//==============================================================================
// VALIDATION FAILURES - HWC Component
//==============================================================================

//=====================================
// Hwc Category
//=====================================

// The OnSet call took longer than the predefined period.
DECLARE_CHECK(eCheckOnSetLatency, HWC, WARN, "Check OnSet Latency", Hwc)

// If the HWC composes into a buffer which is on screen, then it is corrupting
// the state of the display
// and the internal state must be incorrect.
DECLARE_CHECK(eCheckCompToDisplayedBuf, HWC, ERROR,
              "HWC composed to on-screen buffer", Hwc)

// Legacy code.
DECLARE_CHECK(eCheckDelayedOnSetComp, HWC, WARN,
              "HWC has signalled retire fence too early - OR onSet completion "
              "delayed by >5ms - frame not validated",
              Hwc)

// HWC uses GEM WAIT to wait for the GPU to finish a composition.
// A very long wait generally indicates some GPU lockup.
DECLARE_CHECK(eCheckDrmIoctlGemWaitLatency, HWC, ERROR, "Rendering took >1sec",
              Hwc)

// This means that the application has run out of fences or that it has tried to
// duplicate a fence which doesn't exist.
DECLARE_CHECK(eCheckFenceAllocation, HWC, ERROR, "Fence allocation failure",
              Hwc)

// Acquire and release fences are passed through OnSet between HWC and its
// caller (harness or SF). These are limited resources
// and it is important for them to be properly closed.
// Fences not closed appear in /d/sync -> its contents are copied to logcat to
// easier the examination of the source of the leak.
// As small numbers may not be much of an issue, this is just a warning.
DECLARE_CHECK(eCheckFenceLeak, HWC, WARN,
              "Fence leak - fences not closed during test", Hwc)

// Technically it should be possible for a File Descriptor to have the value 0
// and the fence is just
// another FD. However, normally stdin has a value 0 and if the validation finds
// a fence equal to 0,
// it probably means that something has incorrectly closed fence 0, which will
// lead to a lot of problems.
DECLARE_CHECK(eCheckFenceNonZero, HWC, ERROR,
              "Zero fence detected. Has stdin been closed?", Hwc)

// The validation attempts to correlate the completed page flips based on retire
// fences with
// what the HWC tells the validation is the next frame to be validated using the
// logging interface.
// This error indicates that some inconsistency has been found in this
// correlation.
DECLARE_CHECK(eCheckFlipFences, HWC, ERROR,
              "Retire fence state inconsistency with HWC log", Hwc)

// HWC seems to have flipped something to a display for which there is no source
// layer list
DECLARE_CHECK(eCheckUnknownFlipSource, HWC, ERROR,
              "No source layer list for the flip we are trying to validate",
              Hwc)

// An attempt to query gralloc has failed
DECLARE_CHECK(eCheckGrallocDetails, Buffers, ERROR,
              "Failure to obtain correct gralloc details", Hwc)

// This error happens when, despite the fact that the display has generated the
// VSync,
// the HWC has not issued the VSync callback to the harness or SF which have
// requested VSyncs, within the timeout.
// This is an error if it happens >3 times
DECLARE_CHECK(eCheckHwcGeneratesVSync, HWC, WARN,
              "Display has generated VSync but HWC has not forwarded it within "
              "the timeout",
              Hwc)

// This error means that what is on the screen is wrong. There may be an extra
// layer or a missing layer.
// The handles in the layer list have not been fully expressed in the screen.
DECLARE_CHECK(eCheckLayerDisplay, HWC, ERROR,
              "Missing or extra layers on the screen", Hwc)

// When the harness fills a buffer, it waits on the previous release fence
// before starting.
// This message logs the fact that the harness has to wait. This problem can
// happen because
// the buffer protected by the fence is still in use by the screen or the HWC.
// This kind of messages are more common in a double buffer system than in a
// 4-uple buffer system.
DECLARE_CHECK(
    eCheckReleaseFenceWait, HWC, INFO,
    "Wait required on previous Release Fence before buffer can be filled", Hwc)

// This is the same condition as above, but underlines that 100ms have already
// passed.
DECLARE_CHECK(eCheckReleaseFenceTimeout, HWC, WARN,
              "Wait >100ms required on previous Release Fence before buffer "
              "can be filled",
              Hwc)

// This error indicates a bug in the fence management in HWC or an internal
// error in the validation.
DECLARE_CHECK(eCheckRetireFenceSignalledPromptly, HWC, ERROR,
              "Retire fence not signalled for many frames", Hwc)

// The test never reached a conclusion.
DECLARE_CHECK(eCheckRunAbort, HWC, FATAL,
              "Test aborted or locked up - did not complete successfully", Hwc)

// This error is specific to the running of SF. It is not relevant to the
// harness.
DECLARE_CHECK(eCheckSFRestarted, HWC, FATAL, "Surface Flinger Restarted", Hwc)

// This error happens when a buffer previously identified as "SKIP" has been
// placed on the screen in
// a subsequent frame where it doesn't appear in the layer list.
DECLARE_CHECK(eCheckSkipLayerUsage, HWC, WARN,
              "Skip layer used from a different frame", Hwc)

// This indicates that there were more than 120 (number could change)
// consecutive dropped frames on one
// display during the test.
DECLARE_CHECK(eCheckTooManyConsecutiveDroppedFrames, HWC, ERROR,
              "Too many consecutive dropped frames", Hwc)

// More than half of the frames were dropped on one display and there were more
// than 50 frames in the test.
DECLARE_CHECK(eCheckTooManyDroppedFrames, HWC, ERROR,
              "Most frames were dropped", Hwc)

// Inconsistency between the test harness and the shims' calculation of whether
// we should be in extended mode.
DECLARE_CHECK(
    eCheckExtendedModeExpectation, HWC, ERROR,
    "Test expectation of mode selection differs from HWC implementation", Hwc)

// Timeout on hot plug
DECLARE_CHECK(eCheckHotPlugTimeout, HWC, ERROR,
              "Hot plug/unplug attempt not completed inside timeout period",
              Hwc)

// HWC must (normally?) provide a retire fence for every onSet on D0
DECLARE_CHECK(eCheckNoRetireFenceOnPrimary, HWC, ERROR,
              "No retire fence on primary display", Hwc)

// Is HWC using the right DDR mode?
// Can be set by (a) a service call or (b) configured automatically when video
// playing
// on one screen only, if enabled by HWC option.
DECLARE_CHECK(eCheckDDRMode, HWC, ERROR, "Wrong DDR mode selected", Hwc)

// HWC using composition when planes could have been sent direct to the display
DECLARE_CHECK(eCheckUnnecessaryComposition, HWC, ERROR,
              "HWC used composition unnecessarily", Hwc)

// Incorrect blending used in HWC or iVP composition
DECLARE_CHECK(eCheckCompositionBlend, HWC, ERROR,
              "Layer was composed with incorrect blending", Hwc)

// Surfaceflinger fallback composer used. In builds where twostagefallback is
// enabled, this should never happen.
DECLARE_CHECK(eCheckSfFallback, HWC, ERROR,
              "SurfaceFlinger used as fallback composer", Hwc)

// HWC interface behaviour incorrect
DECLARE_CHECK(eCheckHwcInterface, HWC, ERROR,
              "HWC interface returning unsupported values", Hwc)

// HWC has code to restore snapshot layers which may be temporarily lost during
// the rotation animation process.
// The conditions for this can occasionally be met during the Api test but this
// should be really rare
// so if it happens a lot we generate an error
DECLARE_CHECK(eCheckTooManySnapshotsRestored, HWC, ERROR,
              "Looks like rotation animation snapshot code is too aggressive",
              Hwc)

// In VPP or partitioned composition, the target buffer handle must not also be
// one of the sources.
// Clearly this condition is wrong; if not detected it will also break the
// validation.
DECLARE_CHECK(
    eCheckSrcBufAlsoTgt, HWC, ERROR,
    "Composition source buffer is also a render target of the same composition",
    Hwc)

// LLQ overflow. This implies that layer lists are being received from the
// caller (the harness or surface flinger)
// but are not being consumed (page flip or Widi::onFrameReady call).
DECLARE_CHECK(
    eCheckLLQOverflow, HWC, ERROR,
    "Layer list queue overflow. Some layer lists are not being consumed.", Hwc)

//=====================================
// HwcDisplay Category
//=====================================
// self-explanatory
DECLARE_CHECK(eCheckInvalidCrtc, HWC, ERROR, "DRM: Invalid CRTC", HwcDisplay)
DECLARE_CHECK(eCheckDrmCallSuccess, HWC, ERROR, "DRM: call reported failure",
              HwcDisplay)
DECLARE_CHECK(eCheckPlaneIdInvalidForCrtc, HWC, ERROR,
              "DRM: Plane Id not valid for CRTC", HwcDisplay)
DECLARE_CHECK(eCheckIoctlParameters, HWC, ERROR,
              "DRM: Ioctl parameters incorrect", HwcDisplay)
DECLARE_CHECK(eCheckPlaneOffScreen, HWC, ERROR,
              "DRM: Plane is wholly off screen", HwcDisplay)
DECLARE_CHECK(eCheckSetPlaneNeededAfterRotate, HWC, ERROR,
              "DRM: Setplane needed after rotate", HwcDisplay)

// The validation checks that the coordinates in the layer list have been
// correctly transposed to DRM calls.
// This takes into account all the compositions that HWC and SF have performed.
// If there is an inconsistency one or more of these errors can be generated.
DECLARE_CHECK(eCheckPlaneCrop, HWC, ERROR,
              "Layer was displayed with an incorrect source crop", HwcDisplay)
DECLARE_CHECK(eCheckPlaneScale, HWC, ERROR,
              "Layer was displayed with incorrect scaling", HwcDisplay)
DECLARE_CHECK(eCheckPlaneTransform, HWC, ERROR,
              "Layer was displayed with incorrect flip/rotation", HwcDisplay)
DECLARE_CHECK(eCheckPlaneBlending, HWC, ERROR,
              "Layer was displayed with incorrect blending", HwcDisplay)
DECLARE_CHECK(eCheckPixelAlpha, HWC, ERROR, "Pixel alpha was lost for layer",
              HwcDisplay)
DECLARE_CHECK(eCheckPlaneAlpha, HWC, ERROR,
              "Layer was displayed with incorrect plane alpha", HwcDisplay)

// Nuclear parameter validation
DECLARE_CHECK(eCheckInvalidBlend, HWC, ERROR,
              "Unrecognised blend function used in drmModeAtomic", HwcDisplay)

// Very specific check as BXT requires the rearmost plane has to be an opaque
// format (such as RGBX rather than RGBA).
// HWC implements this for all platforms in fact.
DECLARE_CHECK(eCheckBackHwStackPixelFormat, HWC, WARN,
              "Plane at back of HW stack should be an opaque format",
              HwcDisplay)

// This is a requirememnt for BYT and CHT platforms. Not a requirement for BXT.
DECLARE_CHECK(eCheckMainPlaneFullScreen, HWC, ERROR,
              "Main plane allocated buffer size is not full screen", HwcDisplay)

// If HWC is using a crop bigger than the buffer size probably indicates that it
// is confused about what the
// buffer is. This could lead to kernel crash.
DECLARE_CHECK(eCheckBufferTooSmall, HWC, ERROR,
              "Crop should not be bigger than buffer size", HwcDisplay)

// Unless a scaler is in place, the DRM requires source crop and display being
// the same size.
DECLARE_CHECK(eCheckDisplayCropEqualDisplayFrame, HWC, ERROR,
              "Hardware display plane requires source crop and display frame "
              "to be same size",
              HwcDisplay)

// self-explanatory
DECLARE_CHECK(eCheckLayerOrder, HWC, ERROR,
              "Layers have been displayed with an incorrect Z-order",
              HwcDisplay)

// self-explanatory
DECLARE_CHECK(eCheckDrmFence, HWC, ERROR,
              "Fence state incompatible with DRM call", HwcDisplay)

// The screen has been blanked when there is no valid reason.
DECLARE_CHECK(eCheckDisplayDisableInconsistency, HWC, ERROR,
              "Display was disabled when blanking not requested", HwcDisplay)

// Errors have been detected in the power state of the panel. This includes the
// conditions being satisfied for extended mode,
// but the panel not being turned off. Also, if the panel is disabled when there
// is no other valid display, that would be an error.
DECLARE_CHECK(eCheckExtendedModePanelControl, HWC, ERROR,
              "Extended Mode panel control", HwcDisplay)

// In order to prevent buffers being unnecessarily locked to the display, HWC
// should display a blanking buffer on any screen
// which is turned off.
DECLARE_CHECK(eCheckDisabledDisplayBlanked, HWC, WARN,
              "Disabled display was not blanked - existing content should be "
              "removed when display disabled",
              HwcDisplay)

// There is a KPI for the time from the power button being pressed until an
// image appears on the screen being
// no more than ~800msecs. The HWC contributes to this delay because it has to
// set the display mode as well as map
// a buffer to the screen. We currently allow a budget of 200msecs so that any
// regression in the HWC performance can be caught.
DECLARE_CHECK(eCheckUnblankingLatency, HWC, ERROR,
              "Display unblanking (resume) time too long", HwcDisplay)

// ESD checks whether or not the display can get into a bad state, i.e. hw gets
// locked up. If this
// condition is detected and sent to the HWC, it causes a DRM setmode reset.
// This error suggests that the ESD recovery functionality is not working.
DECLARE_CHECK(eCheckEsdRecovery, HWC, ERROR,
              "ESD recovery should complete within 3sec of UEvent", HwcDisplay)

// On BYT and CHT we understand that following a SetCRTC the first frame must be
// a 32bit format, such as RGBA/RGBX.
// If this doesn't happen undefined behaviour can be caused.
DECLARE_CHECK(eCheckFirstFrame32bit, HWC, ERROR,
              "First frame after drmModeSetCrtc must be 32-bit", HwcDisplay)

// On BYT and CHT the hardware only has one register to define the scale factor
// in both X and Y.
// The DRM interface, theoretically allows more arbitrary scalings in both
// directions, but if these are used,
// it can result in undefined behaviour.
DECLARE_CHECK(eCheckPanelFitterConstantAspectRatio, HWC, ERROR,
              "Panel fitter cannot change aspect ratio of the source image",
              HwcDisplay)

// Wrong panel fitter mode used for the required scalings
DECLARE_CHECK(eCheckPanelFitterMode, HWC, ERROR, "Wrong panel fitter mode used",
              HwcDisplay)

// Panel fitter used when no scaling is needed.
DECLARE_CHECK(eCheckPanelFitterUnnecessary, HWC, ERROR,
              "Panel fitter used when no scaling is required", HwcDisplay)

// Panel fitter not recommended with main plane enabled
DECLARE_CHECK(eCheckPanelFitterOutOfSpec, HWC, WARN,
              "Panel fitter use with main plane enabled is not recommended",
              HwcDisplay)

// The display resolution and refresh rate do not match what's expected, which
// should be in this order of priority:
// 1. A refresh rate to match the current video rate, if we are in extended mode
// 2. The user-selected video mode from the IDisplayModeControl interface
// 3. The device's preferred video mode.
DECLARE_CHECK(eCheckDisplayMode, HWC, ERROR, "Wrong display mode selected",
              HwcDisplay)

// Limitations on display formats supported by display planes.
// BYT/CHV do not support NV12 formats.
// BXT does support them but only on first two planes of each display.
DECLARE_CHECK(eCheckPlaneFormatNotSupported, HWC, ERROR,
              "Display plane does not support the buffer's format", HwcDisplay)

// Broxton+
//
// Limitations on Brxton plane and pipe scalers. (Pipe scaler = Panel fitter).
// Scalers must not be enabled when the horizontal source size is greater than
// 4096 pixels.
// Scaler 1 and 2 must not be both scaling the same plane output.
// When scaling is enabled, the scaler input width should be a minimum of 8
// pixels and the height should be minimum of 8 scanlines.
// When the plane scaling is used with YUV 420 planar formats, the height should
// be a minimum of 16 scanlines.
DECLARE_CHECK(eCheckBadScalerSourceSize, HWC, ERROR,
              "Invalid source size for hardware scaling", HwcDisplay)

// Broxton hardware scalers support downscaling by up to but excluding 3x
// except for NV12, which is supported down to but excluding 2x.
DECLARE_CHECK(eCheckScalingFactor, HWC, ERROR,
              "Hardware scaling factor out of permitted range", HwcDisplay)

// Only 2 scalers per pipe (1 on pipe C)
DECLARE_CHECK(eCheckNumScalersUsed, HWC, ERROR, "Too many scalers used",
              HwcDisplay)

// Invalid parameters in drmModeSetDisplay call
DECLARE_CHECK(eCheckSetDisplayParams, HWC, ERROR,
              "Invalid parameters in DRM SetDisplay call", HwcDisplay)

// Invalid parameters in drmAtomic call
// (known as nuclear to avoid confusion with SetDisplay, which is sometimes
// called atomic).
DECLARE_CHECK(eCheckNuclearParams, HWC, ERROR,
              "Invalid parameters in DRM nuclear call", HwcDisplay)

// Check to detect whether a render compressed buffer has been sent to a plane
// that does not support render decompression
DECLARE_CHECK(
    eCheckRCNotSupportedOnPlane, HWC, ERROR,
    "RC content sent to plane that does not support Render Compression",
    HwcDisplay)

// Check to detect if a non render compressed buffer is sent to a render
// compressed plane
DECLARE_CHECK(eCheckRCNormalBufSentToRCPlane, HWC, ERROR,
              "Non Render Compressed buffer sent to RC plane", HwcDisplay)

// Check to detect whether a render compressed buffer has been sent to a plane
// that does not support render decompression
DECLARE_CHECK(
    eCheckRCWithInvalidRotation, HWC, ERROR,
    "RC content can not be sent to a plane with 90/270 degree rotation",
    HwcDisplay)

// Only RGB8888 Y tiled formats are render compressible
DECLARE_CHECK(eCheckRCInvalidFormat, HWC, ERROR,
              "Only RGB8888 Y-tiled formats are render compressible",
              HwcDisplay)

// Check that Aux buffer details match those stored in Gralloc for a given
// buffer
DECLARE_CHECK(eCheckRCAuxDetailsMismatch, HWC, ERROR,
              "Aux buffer details do not match those stored in Gralloc",
              HwcDisplay)

// Check that the tiling format is compatible with RC (i.e. Y-Tiled or Yf-Tiled)
DECLARE_CHECK(eCheckRCInvalidTiling, HWC, ERROR,
              "Tiling format is not valid for use with Render Compression",
              HwcDisplay)

// Check that the tiling format is compatible with RC (i.e. Y-Tiled or Yf-Tiled)
DECLARE_CHECK(eCheckRCSentToVPP, HWC, ERROR,
              "Render Compressed buffers can not be sent to VPP", HwcDisplay)

// Flip while DPMS disabled can lead to kernel hang
DECLARE_CHECK(eCheckNoFlipWhileDPMSDisabled, HWC, ERROR,
              "drmModeSetDisplay/drmModeAtomic while DPMS disabled", HwcDisplay)

//=====================================
// Optional Category
//=====================================
// This optional error check enables the comparison of HWC compositions (such as
// partitioned composer)
// with the HWC validation reference composer. In the first instance an exact
// match comparison is performed (memcmp).
// If this fails, a structural similarity (SSIM) comparison is executed. If the
// resulting SSIM Index falls below
// a certain level, it means that there has been a composition error.
DECLARE_CHECK(eCheckHwcCompMatchesRef, HWC, ERROR,
              "HWC Composition target differs from reference composer", Opt)

//==============================================================================
// VALIDATION FAILURES  - Surface Flinger Component
//==============================================================================
// SF has provided layers which are not in the screen.
DECLARE_CHECK(eCheckLayerOnScreen, SF, WARN,
              "SF error: layer is wholly off screen", Sf)
DECLARE_CHECK(eCheckLayerPartlyOnScreen, SF, INFO,
              "SF layer is partly off screen", Sf)

// This is used for validation of the reference composer against SF.
// It is now quite hard to stimulate because there is very little use of SF
// composition now.
DECLARE_CHECK(eCheckSfCompMatchesRef, SF, ERROR,
              "SF Composition target differs from reference composer", Opt)

// HWC API parameter validation
DECLARE_CHECK(eCheckHwcParams, SF, ERROR, "Invalid HWC API parameters", Sf)
//==============================================================================
// VALIDATION FAILURES  - Display Component
// These errors could be caused by display kernel problems
//==============================================================================
// Optional facility to detect when the display content changes without any
// requested
// change in the planes sent by DRM. This suggests that there will be a visible
// flicker on the display.
DECLARE_CHECK(eCheckCRC, Displays, ERROR,
              "Potential flicker detected by display CRC checking", Opt)

// These checks detect flicker by looking at the relative timing of DRM calls
// and page flip.
// These now have little relevance because of atomic DRM.
DECLARE_CHECK(eCheckFlicker, Displays, ERROR,
              "Potential flicker detected (Unclassified)", Displays)
DECLARE_CHECK(eCheckFlickerClrDepth, Displays, ERROR,
              "Potential flicker detected (colour depth change)", Displays)
// When Max FIFO is disabled, a stall until vsync results, this is currently
// unavoidable so we regard this as a warning
DECLARE_CHECK(eCheckFlickerMaxFifo, Displays, WARN,
              "Potential flicker detected (disabling MAX FIFO)", Displays)

// Vblank/VSync may have been a little late. This check is largely superseded by
// others.
DECLARE_CHECK(eCheckVSyncTiming, Displays, WARN, "VSync timing concern",
              Displays)

// VSync capture was requested using DrmWaitVBlank but the VBlank handler was
// not called within timeout period.
DECLARE_CHECK(eCheckDispGeneratesVSync, Displays, WARN,
              "No VSync received from Display within timeout", Displays)

// Following call to DrmSetDisplay (or equivalent), if page flip event is
// requested, it should occur within the
// display refresh period. We allow a fixed timeout which should be sufficient
// for common displays but,
// if the system is very busy, page flips can be delayed as these are user mode
// events, hence this is a warning.
DECLARE_CHECK(eCheckTimelyPageFlip, Displays, WARN,
              "No Page Flip event received from Display within timeout",
              Displays)

// It is expected that after a SetDisplay call a Page Flip is returned. If this
// doesn't happen and another SetDisplay
// call is executed by the HWC, that implies that the kernel may be locked up
// and the screen could be black.
DECLARE_CHECK(
    eCheckDispGeneratesPageFlip, Displays, ERROR,
    "No Page Flip event received between consecutive calls to SetDisplay",
    Displays)

// Set Display locked up
DECLARE_CHECK(
    eCheckDrmSetDisplayLockup, Displays, FATAL,
    "drmModeSetDisplay/drmModeAtomic did not return within timeout period",
    Displays)

// DPMS enable/disable locked up
DECLARE_CHECK(eCheckDPMSLockup, Displays, FATAL,
              "DPMS Enable/disable did not return within timeout period",
              Displays)

// Timing warnings
DECLARE_CHECK(eCheckDrmSetPropLatency, Displays, INFO,
              "drmModeSetProperty took >1ms", Displays)
DECLARE_CHECK(eCheckDrmSetPropLatencyX, Displays, WARN,
              "drmModeSetProperty took >10ms", Displays)
DECLARE_CHECK(eCheckDrmIoctlLatency, Displays, INFO, "drmIoctl took >1ms",
              Displays)
DECLARE_CHECK(eCheckDrmIoctlLatencyX, Displays, WARN, "drmIoctl took >10ms",
              Displays)

//==============================================================================
// VALIDATION FAILURES - Buffers Component
//==============================================================================
// The buffer sent to DRM appears to have a frame buffer Id which has not been
// opened.
DECLARE_CHECK(eCheckDrmFbId, Buffers, ERROR,
              "DRM: Framebuffer Id consistency problem", Buffers)

// Hwc queried an unknown buffer object. Currently, this is only used in ADF.
DECLARE_CHECK(eCheckBufferObjectUnknown, Buffers, ERROR,
              "Buffer object handle unknown", Buffers)

// Gralloc allocation of a shim internal buffer has failed.
DECLARE_CHECK(eCheckAllocFail, Buffers, ERROR,
              "Gralloc buffer allocation failure - composition failed", Buffers)

// Gralloc query failure
DECLARE_CHECK(eCheckBufferQueryFail, Buffers, ERROR,
              "Gralloc buffer query failure", Buffers)
