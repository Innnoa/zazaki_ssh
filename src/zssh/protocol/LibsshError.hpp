#pragma once

#include <stdexcept>
#include <string>

namespace zssh::protocol {

enum class SshErrorCategory {
  none,
  auth_failed,
  connect_refused,
  connect_reset,
  connect_timeout,
  host_key_mismatch,
  channel_closed,
  exec_failed,
  keepalive_failed,
  unknown,
};

class LibsshError : public std::runtime_error {
 public:
  LibsshError(SshErrorCategory category, const std::string& message)
      : std::runtime_error(message), category_(category) {}

  [[nodiscard]] SshErrorCategory category() const { return category_; }

 private:
  SshErrorCategory category_;
};

}  // namespace zssh::protocol
