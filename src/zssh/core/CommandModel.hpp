#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace zssh::core {

enum class CommandKind { Connect, Exec, Config, Keygen, Proxy, Copy };

struct CommandRequest {
  CommandKind kind;
  std::string profile_name;
  std::string remote_command;
  std::vector<std::string> positional_args;
  bool json_output{false};
  bool allocate_pty{false};
  std::string source_path;
  std::string destination_path;
  std::uint16_t local_port{0};
  std::uint16_t remote_port{0};
};

struct CommandResult {
  int exit_code{0};
  std::string stdout_text;
  std::string stderr_text;
};

}  // namespace zssh::core
