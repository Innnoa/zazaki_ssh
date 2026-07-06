#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace zssh::config {

struct AuthConfig {
  std::string method;
  std::string username;
  std::string password_command;
  std::string key_path;
  bool use_agent{true};
};

struct ProfileConfig {
  std::string host;
  int port{22};
  AuthConfig auth;
  std::string theme{"catppuccin-mocha"};
};

struct DefaultsConfig {
  std::uint32_t stdout_high_watermark_bytes{262144};
  std::uint32_t stderr_high_watermark_bytes{262144};
  std::uint32_t stdin_low_watermark_bytes{65536};
};

struct AppConfig {
  std::unordered_map<std::string, ProfileConfig> profiles;
  DefaultsConfig defaults;
};

}  // namespace zssh::config
