#include "zssh/session/BackpressureController.hpp"

namespace zssh::session {

bool BackpressureController::should_pause(std::size_t buffered_bytes, std::size_t incoming_bytes) {
  if (paused_) {
    return false;
  }

  if (buffered_bytes >= pause_threshold_ ||
      (incoming_bytes != 0 && incoming_bytes >= pause_threshold_ - buffered_bytes)) {
    paused_ = true;
    return true;
  }

  return false;
}

bool BackpressureController::should_resume(std::size_t buffered_bytes) {
  if (!paused_) {
    return false;
  }

  if (buffered_bytes <= resume_threshold_) {
    paused_ = false;
    return true;
  }

  return false;
}

}  // namespace zssh::session
