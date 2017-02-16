#include <gtest/gtest.h>
#include <hardware/hardware.h>

#include <chrono>

#include "worker.h"

using android::Worker;

struct TestWorker : public Worker {
  TestWorker()
      : Worker("test-worker", HAL_PRIORITY_URGENT_DISPLAY),
        value(0),
        enabled_(false) {
  }

  int Init() {
    return InitWorker();
  }

  void Routine() {
    Lock();
    if (!enabled_) {
      int ret = WaitForSignalOrExitLocked();
      if (ret == -EINTR) {
        Unlock();
        return;
      }
      // should only reached here if it was enabled
      if (!enabled_)
        printf("Shouldn't reach here while disabled %d %d\n", value, ret);
    }
    value++;
    Unlock();
  }

  void Control(bool enable) {
    bool changed = false;
    Lock();
    if (enabled_ != enable) {
      enabled_ = enable;
      changed = true;
    }
    Unlock();

    if (enable && changed)
      Signal();
  }

  int value;

 private:
  bool enabled_;
};

struct WorkerTest : public testing::Test {
  TestWorker worker;

  virtual void SetUp() {
    worker.Init();
  }

  void small_delay() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
};

TEST_F(WorkerTest, test_worker) {
  // already isInitialized so should fail
  ASSERT_TRUE(worker.initialized());

  int val = worker.value;
  small_delay();

  // value shouldn't change when isInitialized
  ASSERT_EQ(val, worker.value);

  worker.Control(true);
  small_delay();

  // while locked, value shouldn't be changing
  worker.Lock();
  val = worker.value;
  small_delay();
  ASSERT_EQ(val, worker.value);
  worker.Unlock();

  small_delay();
  // value should be different now
  ASSERT_NE(val, worker.value);

  worker.Control(false);
  worker.Lock();
  val = worker.value;
  worker.Unlock();
  small_delay();

  // value should be same
  ASSERT_EQ(val, worker.value);

  worker.Exit();
  ASSERT_FALSE(worker.initialized());
}

TEST_F(WorkerTest, exit_while_running) {
  worker.Control(true);

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  worker.Exit();
}
