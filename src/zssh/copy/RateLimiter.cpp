#include "zssh/copy/RateLimiter.hpp"

#include <algorithm>

namespace zssh::copy {

RateLimiter::RateLimiter(std::uint64_t bytes_per_second)
    : rate_(bytes_per_second), tokens_(static_cast<double>(bytes_per_second)),
      last_refill_(std::chrono::steady_clock::now()) {}

bool RateLimiter::try_consume(std::uint64_t bytes,
                              std::chrono::steady_clock::duration elapsed) {
  std::lock_guard<std::mutex> lock(mutex_);

  double elapsed_sec = std::chrono::duration<double>(elapsed).count();

  tokens_ = std::min(tokens_ + rate_ * elapsed_sec, static_cast<double>(rate_));

  if (tokens_ >= static_cast<double>(bytes)) {
    tokens_ -= static_cast<double>(bytes);
    return true;
  }
  return false;
}

std::chrono::milliseconds RateLimiter::wait_for_bytes(std::uint64_t bytes) {
  std::lock_guard<std::mutex> lock(mutex_);

  double needed = static_cast<double>(bytes) - tokens_;
  if (needed <= 0.0) return std::chrono::milliseconds(0);

  double wait_sec = needed / static_cast<double>(rate_);
  return std::chrono::milliseconds(static_cast<std::int64_t>(wait_sec * 1000.0));
}

}  // namespace zssh::copy
