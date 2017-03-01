#include <gtest/gtest.h>
#include <hardware/hardware.h>

#include <chrono>
#include <mutex>

#include "queue_worker.h"

using android::QueueWorker;

#define UNUSED_ARG(x) (void)(x)

struct TestData {
  TestData(int val) : value(val) {
  }
  virtual ~TestData() {
  }

  virtual void CheckValue(int prev_value) {
    ASSERT_EQ(prev_value + 1, value);
  }

  int value;
};

struct TestQueueWorker : public QueueWorker<TestData> {
  TestQueueWorker()
      : QueueWorker("test-queueworker", HAL_PRIORITY_URGENT_DISPLAY), value(0) {
  }

  int Init() {
    return InitWorker();
  }

  void ProcessWork(std::unique_ptr<TestData> data) {
    std::lock_guard<std::mutex> blk(block);
    data->CheckValue(value);
    {
      std::lock_guard<std::mutex> lk(lock);
      value = data->value;
    }
    cond.notify_one();
  }

  void ProcessIdle() {
    ASSERT_FALSE(idle());
  }

  std::mutex lock;
  std::mutex block;
  std::condition_variable cond;
  int value;
};

struct QueueWorkerTest : public testing::Test {
  static const int kTimeoutMs = 1000;
  TestQueueWorker qw;

  virtual void SetUp() {
    qw.Init();
  }
  bool QueueValue(int val) {
    std::unique_ptr<TestData> data(new TestData(val));
    return !qw.QueueWork(std::move(data));
  }

  bool WaitFor(int val, int timeout_ms = kTimeoutMs) {
    std::unique_lock<std::mutex> lk(qw.lock);

    auto timeout = std::chrono::milliseconds(timeout_ms);
    return qw.cond.wait_for(lk, timeout, [&] { return qw.value == val; });
  }
};

struct IdleQueueWorkerTest : public QueueWorkerTest {
  const int64_t kIdleTimeoutMs = 100;

  virtual void SetUp() {
    qw.set_idle_timeout(kIdleTimeoutMs);
    qw.Init();
  }
};

TEST_F(QueueWorkerTest, single_queue) {
  // already isInitialized so should fail
  ASSERT_NE(qw.Init(), 0);

  ASSERT_EQ(qw.value, 0);
  ASSERT_TRUE(QueueValue(1));
  ASSERT_TRUE(WaitFor(1));
  ASSERT_EQ(qw.value, 1);
  ASSERT_FALSE(qw.IsWorkPending());
}

TEST_F(QueueWorkerTest, multiple_waits) {
  for (int i = 1; i <= 100; i++) {
    ASSERT_TRUE(QueueValue(i));
    ASSERT_TRUE(WaitFor(i));
    ASSERT_EQ(qw.value, i);
    ASSERT_FALSE(qw.IsWorkPending());
  }
}

TEST_F(QueueWorkerTest, multiple_queue) {
  for (int i = 1; i <= 100; i++) {
    ASSERT_TRUE(QueueValue(i));
  }
  ASSERT_TRUE(WaitFor(100));
  ASSERT_EQ(qw.value, 100);
  ASSERT_FALSE(qw.IsWorkPending());
}

TEST_F(QueueWorkerTest, blocking) {
  // First wait for inital value to be setup
  ASSERT_TRUE(QueueValue(1));
  ASSERT_TRUE(WaitFor(1));

  // Block processing and fill up the queue
  std::unique_lock<std::mutex> lk(qw.block);
  size_t expected_value = qw.max_queue_size() + 2;
  for (size_t i = 2; i <= expected_value; i++) {
    ASSERT_TRUE(QueueValue(i));
  }

  qw.set_queue_timeout(100);
  // any additional queueing should fail
  ASSERT_FALSE(QueueValue(expected_value + 1));

  // make sure value is not changed while blocked
  {
    std::unique_lock<std::mutex> lock(qw.lock);
    auto timeout = std::chrono::milliseconds(100);
    ASSERT_FALSE(
        qw.cond.wait_for(lock, timeout, [&] { return qw.value != 1; }));
  }
  ASSERT_EQ(qw.value, 1);
  ASSERT_TRUE(qw.IsWorkPending());

  // unblock and wait for value to be reached
  lk.unlock();
  ASSERT_TRUE(WaitFor(expected_value));
  ASSERT_FALSE(qw.IsWorkPending());
}

TEST_F(QueueWorkerTest, exit_slow) {
  struct SlowData : public TestData {
    SlowData(int val) : TestData(val) {
    }
    void CheckValue(int prev_value) {
      UNUSED_ARG(prev_value);

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  };
  std::unique_ptr<SlowData> data(new SlowData(1));
  ASSERT_EQ(qw.QueueWork(std::move(data)), 0);
  data = std::unique_ptr<SlowData>(new SlowData(2));
  ASSERT_EQ(qw.QueueWork(std::move(data)), 0);
  qw.Exit();
  ASSERT_FALSE(qw.initialized());
}

TEST_F(QueueWorkerTest, exit_empty) {
  qw.Exit();
  ASSERT_FALSE(qw.initialized());
}

TEST_F(QueueWorkerTest, queue_worker_noidling) {
  ASSERT_TRUE(QueueValue(1));
  ASSERT_TRUE(WaitFor(1));

  ASSERT_FALSE(qw.idle());
  auto timeout = std::chrono::milliseconds(200);
  std::this_thread::sleep_for(timeout);
  ASSERT_FALSE(qw.idle());
}

TEST_F(IdleQueueWorkerTest, queue_worker_idling) {
  ASSERT_TRUE(QueueValue(1));
  ASSERT_TRUE(WaitFor(1));
  ASSERT_FALSE(qw.idle());

  auto timeout = std::chrono::milliseconds(kIdleTimeoutMs + 10);
  std::this_thread::sleep_for(timeout);
  ASSERT_TRUE(qw.idle());
  ASSERT_TRUE(QueueValue(2));
  ASSERT_TRUE(WaitFor(2));
  ASSERT_FALSE(qw.idle());

  std::this_thread::sleep_for(3 * timeout);
  ASSERT_TRUE(qw.idle());

  ASSERT_TRUE(QueueValue(3));
  ASSERT_TRUE(WaitFor(3));
  for (int i = 4; i <= 100; i++) {
    QueueValue(i);
  }
  ASSERT_FALSE(qw.idle());
  qw.Exit();
  ASSERT_FALSE(qw.initialized());
}