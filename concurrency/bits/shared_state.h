#pragma once

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

#include "result_box.h"

namespace concurrency {

namespace detail {

template<typename T>
class shared_state {
public:
  shared_state() = default;
  shared_state(const shared_state&) = delete;
  shared_state(shared_state&&) = delete;

  template<typename U>
  void set_value(U&& u) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (retrieved_)
      throw future_error(future_errc::future_already_retrieved);
    box_.set_value(std::forward<U>(u));
    cv_.notify_all();
  }

  void set_exception(std::exception_ptr error) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (retrieved_)
      throw future_error(future_errc::future_already_retrieved);
    box_.set_exception(error);
    cv_.notify_all();
  }

  void wait() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
  }

  template<typename Rep, typename Period>
  future_status wait_for(const std::chrono::duration<Rep, Period>& rel_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool wait_res = cv_.wait_for(lock, rel_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? future_status::ready : future_status::timeout;
  }

  template <typename Clock, typename Duration>
  future_status wait_until(const std::chrono::time_point<Clock, Duration>& abs_time) {
    std::unique_lock<std::mutex> lock(mutex_);
    const bool wait_res = cv_.wait_until(lock, abs_time, [this] {
      return box_.get_state() != detail::box_state::empty;
    });
    return wait_res ? future_status::ready : future_status::timeout;
  }

  T get() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {return box_.get_state() != detail::box_state::empty;});
    if (retrieved_)
      throw future_error(future_errc::future_already_retrieved);
    retrieved_ = true;
    return box_.get();
  }

  bool is_ready() {
    std::unique_lock<std::mutex> lock(mutex_);
    return !retrieved_ && box_.get_state() != detail::box_state::empty;
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  result_box<T> box_;
  bool retrieved_ = false;
};

} // namespace detail

} // namespace concurrency