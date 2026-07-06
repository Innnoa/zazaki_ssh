#pragma once

#include <cstddef>
#include <stdexcept>

namespace zssh::session {

class BackpressureController {
 public:
  BackpressureController(std::size_t pause_threshold, std::size_t resume_threshold)
      : pause_threshold_(pause_threshold), resume_threshold_(resume_threshold) {
    if (pause_threshold == 0 || resume_threshold == 0) {
      throw std::invalid_argument("backpressure thresholds must be greater than zero");
    }
    if (resume_threshold >= pause_threshold) {
      throw std::invalid_argument("resume threshold must be < pause threshold");
    }
  }

  [[nodiscard]] bool should_pause(std::size_t buffered_bytes, std::size_t incoming_bytes = 0);
  [[nodiscard]] bool should_resume(std::size_t buffered_bytes);

 private:
  std::size_t pause_threshold_;
  std::size_t resume_threshold_;
  bool paused_{false};
};

}  // namespace zssh::session
