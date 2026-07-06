#include "zssh/cli/CliParser.hpp"

#include "zssh/core/Error.hpp"

#include <cctype>
#include <string_view>

namespace zssh::cli {

namespace {

bool is_blank_string(std::string_view value) {
  if (value.empty()) {
    return true;
  }

  for (const unsigned char ch : value) {
    if (!std::isspace(ch)) {
      return false;
    }
  }

  return true;
}

zssh::core::CommandKind parse_command_kind(std::string_view verb) {
  if (verb == "connect") {
    return zssh::core::CommandKind::Connect;
  }

  if (verb == "exec") {
    return zssh::core::CommandKind::Exec;
  }

  if (verb == "config") {
    return zssh::core::CommandKind::Config;
  }

  if (verb == "keygen") {
    return zssh::core::CommandKind::Keygen;
  }

  if (verb == "proxy") {
    return zssh::core::CommandKind::Proxy;
  }

  if (verb == "copy") {
    return zssh::core::CommandKind::Copy;
  }

  throw zssh::core::Error("unsupported command: " + std::string(verb));
}

std::string join_payload_tokens(const std::vector<std::string>& tokens) {
  std::string joined;
  for (const auto& token : tokens) {
    if (!joined.empty()) {
      joined += ' ';
    }
    joined += token;
  }
  return joined;
}

}  // namespace

namespace {

zssh::core::CommandRequest parse_command_line_impl(int argc, const char* const* argv) {
  if (argc < 3) {
    throw zssh::core::Error("command and profile are required");
  }

  zssh::core::CommandRequest request{};
  request.kind = parse_command_kind(argv[1]);
  request.profile_name = argv[2];
  if (is_blank_string(request.profile_name)) {
    throw zssh::core::Error("profile name must not be blank");
  }

  bool payload_only = false;
  for (int i = 3; i < argc; ++i) {
    const std::string_view token = argv[i];
    if (!payload_only && token == "--") {
      payload_only = true;
      continue;
    }

    if (!payload_only && token == "--json") {
      request.json_output = true;
      continue;
    }

    if (!payload_only && token == "--local-port" && i + 1 < argc) {
      request.local_port = static_cast<std::uint16_t>(
          std::stoi(std::string(argv[++i])));
      continue;
    }

    if (!payload_only && token == "--remote-port" && i + 1 < argc) {
      request.remote_port = static_cast<std::uint16_t>(
          std::stoi(std::string(argv[++i])));
      continue;
    }

    request.positional_args.emplace_back(argv[i]);
  }

  // remote_command keeps a raw space-joined shell-oriented rendering.
  // positional_args remains the parsed payload source of truth after local flag
  // normalization; reserved flags such as --json are consumed unless they appear
  // after the explicit `--` delimiter.
  request.remote_command = join_payload_tokens(request.positional_args);
  if (request.kind == zssh::core::CommandKind::Exec && is_blank_string(request.remote_command)) {
    throw zssh::core::Error("exec command requires a payload");
  }

  if (request.kind == zssh::core::CommandKind::Connect) {
    if (request.json_output) {
      throw zssh::core::Error("connect command does not support --json");
    }
    if (!request.positional_args.empty()) {
      throw zssh::core::Error("connect command does not accept payload tokens");
    }
  }

  if (request.kind == zssh::core::CommandKind::Copy) {
    if (request.positional_args.size() < 2) {
      throw zssh::core::Error("copy requires source and destination paths");
    }
    request.source_path = request.positional_args[0];
    request.destination_path = request.positional_args[1];
  }

  if (request.kind == zssh::core::CommandKind::Proxy) {
    if (request.local_port == 0) {
      throw zssh::core::Error("proxy requires --local-port");
    }
  }

  return request;
}

}  // namespace

zssh::core::CommandRequest parse_command_line(int argc, const char* const* argv) {
  return parse_command_line_impl(argc, argv);
}

zssh::core::CommandRequest parse_command_line(int argc, char* const* argv) {
  return parse_command_line_impl(argc, argv);
}

}  // namespace zssh::cli
