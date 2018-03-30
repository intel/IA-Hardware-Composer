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

#define LOG_TAG "MONITOR_TEST"

#include <cutils/memory.h>

#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/Vector.h>

#include <unistd.h>

#include "test_base.h"
#include "HwcTestLog.h"

/** \addtogroup  HwcTestMonitor
    \ingroup UseCaseTests
    @{
        \brief A test that sets up the shims for testing with all checks
   enabled.
                This test provides no surfaces to surface flinger and it is used
   to
                monitor a normal use of a system and to debug the shims.
    @}
*/


class HwcTestTest : public HwcTestBase {
 public:
  HwcTestTest(int argc, char** argv);

  /// Create surfaces and start test
  int Run(void);

  /// Set checks required by the shims
  int SetChecks(void);
};

HwcTestTest::HwcTestTest(int argc, char** argv) : HwcTestBase(argc, argv) {
  mTestName = "hwc_util";
}

int HwcTestTest::SetChecks(void) {
  SetDefaultChecks();

  return 0;
}

int HwcTestTest::Run(void) {
  status_t initOK = InitialiseChecks();

  if (initOK != NO_ERROR) {
    HWCERROR(eCheckSessionFail, "Binder error: %d", initOK);
    return 1;
  }

  return 0;
}

int main(int argc, char** argv) {
  int rc = 0;

  HwcTestTest test(argc, argv);

  test.SetTestEndType(HwcTestBase::etetUserDriven);
  test.SetChecks();

  if (argc > 1) {
    if (strcmp(argv[1], "start") == 0) {
      printf("Starting checks and logging\n");
      test.Run();
    } else if (strcmp(argv[1], "restart") == 0) {
      HwcTestConfig oldConfig;
      test.GetOldConfig(oldConfig);

      printf("Stopping checks\n");
      test.DebriefChecks(false);
      printf("Restarting checks\n");
      test.Run();

      test.LogTestResult(oldConfig);
    } else if (strcmp(argv[1], "stop") == 0) {
      HwcTestConfig oldConfig;
      test.GetOldConfig(oldConfig);

      printf("Stopping checks\n");
      test.DebriefChecks();

      // Turn down the logging to standard level now that the testing is
      // complete
      // This may prevent the unattended system from locking up.
      test.SetLoggingLevelToDefault();

      test.LogTestResult(oldConfig);
    }
  }

  return rc;
}
