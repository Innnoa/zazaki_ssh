#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>

namespace zssh::copy {

class RateLimiter {
 public:
  explicit RateLimiter(std::uint64_t bytes_per_second);

  bool try_consume(std::uint64_t bytes, std::chrono::steady_clock::duration elapsed);
  std::chrono::milliseconds wait_for_bytes(std::uint64_t bytes);

 private:
  std::uint64_t rate_;
  double tokens_;
  std::chrono::steady_clock::time_point last_refill_;
  std::mutex mutex_;
};

}  // namespace zssh::copy
