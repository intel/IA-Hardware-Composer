/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_QUEUE_WORKER_H_
#define ANDROID_QUEUE_WORKER_H_

#include "worker.h"

#include <queue>

namespace android {

template <typename T>
class QueueWorker : public Worker {
 public:
  static const size_t kDefaultMaxQueueSize = 2;
  static const int64_t kTimeoutDisabled = -1;

  QueueWorker(const char *name, int priority)
      : Worker(name, priority),
        max_queue_size_(kDefaultMaxQueueSize),
        queue_timeout_ms_(kTimeoutDisabled),
        idle_timeout_ms_(kTimeoutDisabled),
        idled_out_(false) {
  }

  int QueueWork(std::unique_ptr<T> workitem);

  bool IsWorkPending() const {
    return !queue_.empty();
  }
  bool idle() const {
    return idled_out_;
  }

  int64_t idle_timeout() {
    return idle_timeout_ms_;
  }
  void set_idle_timeout(int64_t timeout_ms) {
    idle_timeout_ms_ = timeout_ms;
  }

  int64_t queue_timeout() {
    return queue_timeout_ms_;
  }
  void set_queue_timeout(int64_t timeout_ms) {
    queue_timeout_ms_ = timeout_ms;
  }

  size_t max_queue_size() const {
    return max_queue_size_;
  }
  void set_max_queue_size(size_t size) {
    max_queue_size_ = size;
  }

 protected:
  virtual void ProcessWork(std::unique_ptr<T> workitem) = 0;
  virtual void ProcessIdle(){}
  virtual void Routine();

  template <typename Predicate>
  int WaitCond(std::unique_lock<std::mutex> &lock, Predicate pred,
               int64_t max_msecs);

 private:
  std::queue<std::unique_ptr<T>> queue_;
  size_t max_queue_size_;
  int64_t queue_timeout_ms_;
  int64_t idle_timeout_ms_;
  bool idled_out_;
};

template <typename T>
template <typename Predicate>
int QueueWorker<T>::WaitCond(std::unique_lock<std::mutex> &lock, Predicate pred,
                             int64_t max_msecs) {
  bool ret = true;
  auto wait_func = [&] { return pred() || should_exit(); };

  if (max_msecs < 0) {
    cond_.wait(lock, wait_func);
  } else {
    auto timeout = std::chrono::milliseconds(max_msecs);
    ret = cond_.wait_for(lock, timeout, wait_func);
  }

  if (!ret)
    return -ETIMEDOUT;
  else if (should_exit())
    return -EINTR;

  return 0;
}

template <typename T>
void QueueWorker<T>::Routine() {
  std::unique_lock<std::mutex> lk(mutex_);
  std::unique_ptr<T> workitem;

  auto wait_func = [&] { return !queue_.empty(); };
  int ret =
      WaitCond(lk, wait_func, idled_out_ ? kTimeoutDisabled : idle_timeout_ms_);
  switch (ret) {
    case 0:
      break;
    case -ETIMEDOUT:
      ProcessIdle();
      idled_out_ = true;
      return;
    case -EINTR:
    default:
      return;
  }

  if (!queue_.empty()) {
    workitem = std::move(queue_.front());
    queue_.pop();
  }
  lk.unlock();
  cond_.notify_all();

  idled_out_ = false;
  ProcessWork(std::move(workitem));
}

template <typename T>
int QueueWorker<T>::QueueWork(std::unique_ptr<T> workitem) {
  std::unique_lock<std::mutex> lk(mutex_);

  auto wait_func = [&] { return queue_.size() < max_queue_size_; };
  int ret = WaitCond(lk, wait_func, queue_timeout_ms_);
  if (ret)
    return ret;

  queue_.push(std::move(workitem));
  lk.unlock();

  cond_.notify_one();

  return 0;
}
};
#endif
