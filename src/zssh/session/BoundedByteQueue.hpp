#pragma once

#include <cstddef>
#include <deque>
#include <stdexcept>
#include <string>
#include <utility>

namespace zssh::session {

class BoundedByteQueue {
 public:
  explicit BoundedByteQueue(std::size_t high_watermark) : high_watermark_(high_watermark) {
    if (high_watermark_ == 0) {
      throw std::invalid_argument("bounded byte queue high watermark must be greater than zero");
    }
  }

  void push(std::string chunk) {
    if (chunk.empty()) {
      throw std::invalid_argument("bounded byte queue does not accept empty chunks");
    }
    if (size_ > high_watermark_ || chunk.size() > high_watermark_ - size_) {
      throw std::length_error("bounded byte queue exceeded high watermark");
    }
    chunks_.push_back(std::move(chunk));
    size_ += chunks_.back().size();
  }

  std::string pop() {
    if (chunks_.empty()) {
      return {};
    }

    std::string chunk = std::move(chunks_.front());
    chunks_.pop_front();
    size_ -= chunk.size();
    return chunk;
  }

  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] std::size_t high_watermark() const { return high_watermark_; }
  [[nodiscard]] bool empty() const { return chunks_.empty(); }

 private:
  std::size_t high_watermark_;
  std::size_t size_{0};
  std::deque<std::string> chunks_;
};

}  // namespace zssh::session
