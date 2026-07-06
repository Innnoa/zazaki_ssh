#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zssh::copy {

enum class CopyDirection { LocalToRemote, RemoteToLocal };

struct CopyRequest {
  std::string profile_name;
  std::string source_path;
  std::string destination_path;
  CopyDirection direction{CopyDirection::LocalToRemote};
  bool recursive{false};
  bool preserve_attrs{false};
  bool resume{false};
  int port{0};
  std::string key_path;
  std::uint32_t concurrency{1};
  std::uint64_t rate_limit_bytes_per_sec{0};
  std::vector<std::string> include_patterns;
  std::vector<std::string> exclude_patterns;
  bool json_output{false};
};

}  // namespace zssh::copy
