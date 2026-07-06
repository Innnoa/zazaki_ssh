#include "zssh/cli/CommandDispatcher.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include "zssh/cli/CliParser.hpp"
#include "zssh/config/ConfigLoader.hpp"
#include "zssh/core/Error.hpp"
#include "zssh/platform/IPlatformServices.hpp"
#include "zssh/protocol/ISshAdapter.hpp"
#include "zssh/renderer/ITerminalRenderer.hpp"
#include "zssh/session/SessionManager.hpp"

namespace zssh::cli {
namespace {

CommandRuntimeFactory& exec_runtime_factory() {
  static CommandRuntimeFactory factory = internal::make_default_exec_runtime_factory();
  return factory;
}

std::optional<std::filesystem::path> read_non_empty_env_path(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    return std::nullopt;
  }
  return std::filesystem::path(value);
}

std::optional<std::filesystem::path> find_user_config_path() {
  if (const auto home_path = read_non_empty_env_path("HOME")) {
    const auto candidate = *home_path / ".config" / "zssh" / "zssh.yaml";
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  if (const auto user_profile_path = read_non_empty_env_path("USERPROFILE")) {
    const auto candidate = *user_profile_path / ".config" / "zssh" / "zssh.yaml";
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  if (const auto appdata_path = read_non_empty_env_path("APPDATA")) {
    const auto candidate = *appdata_path / "zssh" / "zssh.yaml";
    if (std::filesystem::exists(candidate)) {
      return candidate;
    }
  }

  return std::nullopt;
}

std::filesystem::path resolve_config_path() {
  if (const auto env_path = read_non_empty_env_path("ZSSH_CONFIG_PATH")) {
    return *env_path;
  }

  const auto working_copy_path = std::filesystem::current_path() / "zssh.yaml";
  if (std::filesystem::exists(working_copy_path)) {
    return working_copy_path;
  }

  if (const auto user_config_path = find_user_config_path()) {
    return *user_config_path;
  }

  std::string message = "unable to locate zssh config; checked ./zssh.yaml";
  if (const auto home_path = read_non_empty_env_path("HOME")) {
    message += " and " + (*home_path / ".config" / "zssh" / "zssh.yaml").string();
  }
  if (const auto user_profile_path = read_non_empty_env_path("USERPROFILE")) {
    message += " and " + (*user_profile_path / ".config" / "zssh" / "zssh.yaml").string();
  }
  if (const auto appdata_path = read_non_empty_env_path("APPDATA")) {
    message += " and " + (*appdata_path / "zssh" / "zssh.yaml").string();
  }
  message += "; set ZSSH_CONFIG_PATH to override";
  throw zssh::core::Error(message);
}

zssh::protocol::SshConnectionConfig make_connection_config(
    const std::string& profile_name,
    const zssh::config::ProfileConfig& profile) {
  return {
      .profile_name = profile_name,
      .host = profile.host,
      .port = profile.port,
      .auth = {
          .method = profile.auth.method,
          .username = profile.auth.username,
          .password_command = profile.auth.password_command,
          .key_path = profile.auth.key_path,
          .use_agent = profile.auth.use_agent,
      },
  };
}

void append_hex_digit(std::string& escaped, unsigned char value) {
  constexpr std::array<char, 16> kHexDigits = {
      '0', '1', '2', '3', '4', '5', '6', '7',
      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
  };
  escaped.push_back(kHexDigits[value & 0x0f]);
}

void append_unicode_escape(std::string& escaped, unsigned char value) {
  escaped += "\\u00";
  append_hex_digit(escaped, static_cast<unsigned char>((value >> 4) & 0x0f));
  append_hex_digit(escaped, value);
}

bool is_continuation_byte(unsigned char value) {
  return (value & 0xc0U) == 0x80U;
}

std::size_t valid_utf8_sequence_length(std::string_view text, std::size_t index) {
  const auto remaining = text.size() - index;
  const auto lead = static_cast<unsigned char>(text[index]);

  if (lead < 0x80U) {
    return 1;
  }

  if (remaining >= 2 && lead >= 0xc2U && lead <= 0xdfU) {
    const auto b1 = static_cast<unsigned char>(text[index + 1]);
    return is_continuation_byte(b1) ? 2U : 0U;
  }

  if (remaining >= 3) {
    const auto b1 = static_cast<unsigned char>(text[index + 1]);
    const auto b2 = static_cast<unsigned char>(text[index + 2]);
    if (!is_continuation_byte(b2)) {
      return 0;
    }
    if (lead == 0xe0U) {
      return (b1 >= 0xa0U && b1 <= 0xbfU) ? 3U : 0U;
    }
    if ((lead >= 0xe1U && lead <= 0xecU) || (lead >= 0xeeU && lead <= 0xefU)) {
      return is_continuation_byte(b1) ? 3U : 0U;
    }
    if (lead == 0xedU) {
      return (b1 >= 0x80U && b1 <= 0x9fU) ? 3U : 0U;
    }
  }

  if (remaining >= 4) {
    const auto b1 = static_cast<unsigned char>(text[index + 1]);
    const auto b2 = static_cast<unsigned char>(text[index + 2]);
    const auto b3 = static_cast<unsigned char>(text[index + 3]);
    if (!is_continuation_byte(b2) || !is_continuation_byte(b3)) {
      return 0;
    }
    if (lead == 0xf0U) {
      return (b1 >= 0x90U && b1 <= 0xbfU) ? 4U : 0U;
    }
    if (lead >= 0xf1U && lead <= 0xf3U) {
      return is_continuation_byte(b1) ? 4U : 0U;
    }
    if (lead == 0xf4U) {
      return (b1 >= 0x80U && b1 <= 0x8fU) ? 4U : 0U;
    }
  }

  return 0;
}

std::string escape_json(std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size());

  for (std::size_t index = 0; index < text.size();) {
    const auto ch = static_cast<unsigned char>(text[index]);
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        ++index;
        break;
      case '"':
        escaped += "\\\"";
        ++index;
        break;
      case '\b':
        escaped += "\\b";
        ++index;
        break;
      case '\f':
        escaped += "\\f";
        ++index;
        break;
      case '\n':
        escaped += "\\n";
        ++index;
        break;
      case '\r':
        escaped += "\\r";
        ++index;
        break;
      case '\t':
        escaped += "\\t";
        ++index;
        break;
      default:
        if (ch < 0x20) {
          append_unicode_escape(escaped, ch);
          ++index;
          break;
        }
        if (ch < 0x80) {
          escaped += static_cast<char>(ch);
          ++index;
          break;
        }

        const auto sequence_length = valid_utf8_sequence_length(text, index);
        if (sequence_length == 0) {
          append_unicode_escape(escaped, ch);
          ++index;
          break;
        }

        escaped.append(text.substr(index, sequence_length));
        index += sequence_length;
        break;
    }
  }

  return escaped;
}

std::string render_exec_json(const zssh::core::CommandRequest& request,
                             const zssh::core::CommandResult& result) {
  return "{"
      "\"command\":\"" + escape_json(request.remote_command) +
      "\",\"profile\":\"" + escape_json(request.profile_name) +
      "\",\"exit_code\":" + std::to_string(result.exit_code) +
      ",\"stdout\":\"" + escape_json(result.stdout_text) +
      "\",\"stderr\":\"" + escape_json(result.stderr_text) +
      "\"}";
}

int run_exec_command(const zssh::core::CommandRequest& request) {
  const auto config = zssh::config::load_config(resolve_config_path());
  const auto profile_it = config.profiles.find(request.profile_name);
  if (profile_it == config.profiles.end()) {
    throw zssh::core::Error("unknown profile: " + request.profile_name);
  }
  const auto connection = make_connection_config(request.profile_name, profile_it->second);

  auto runtime = exec_runtime_factory()();
  if (!runtime.ssh) {
    throw zssh::core::Error("exec runtime factory returned incomplete dependencies");
  }

  zssh::session::SessionManager session_manager(*runtime.ssh);
  if (request.json_output) {
    const auto result = session_manager.exec(connection, request.remote_command);
    std::cout << render_exec_json(request, result) << '\n';
    return result.exit_code;
  }

  if (!runtime.platform) {
    throw zssh::core::Error("non-json exec runtime requires platform services");
  }

  runtime.platform->create_console();
  zssh::core::CommandResult result;
  result.exit_code = 0;
  const auto response = session_manager.exec(
      connection,
      request.remote_command,
      {
          [&runtime, &result](std::string_view chunk) {
            result.exit_code = 1;
            if (runtime.renderer) {
              runtime.renderer->write_stdout(std::string(chunk));
            }
            std::cout << chunk;
            runtime.platform->pump_events_once();
          },
          [&runtime, &result](std::string_view chunk) {
            result.exit_code = 1;
            if (runtime.renderer) {
              runtime.renderer->write_stderr(std::string(chunk));
            }
            std::cerr << chunk;
            runtime.platform->pump_events_once();
          },
      });
  result.exit_code = response.exit_code;

  return result.exit_code;
}

int run_connect_command(const zssh::core::CommandRequest& request) {
  const auto config = zssh::config::load_config(resolve_config_path());
  const auto profile_it = config.profiles.find(request.profile_name);
  if (profile_it == config.profiles.end()) {
    throw zssh::core::Error("unknown profile: " + request.profile_name);
  }
  const auto connection = make_connection_config(request.profile_name, profile_it->second);

  auto runtime = exec_runtime_factory()();
  if (!runtime.ssh) {
    throw zssh::core::Error("connect runtime factory returned incomplete dependencies");
  }

  zssh::session::SessionManager session_manager(*runtime.ssh);
  if (runtime.platform) {
    runtime.platform->create_console();
  }
  const auto result = session_manager.connect(connection);
  if (runtime.platform) {
    runtime.platform->pump_events_once();
  }

  return result.exit_code;
}

}  // namespace

int run_command(int argc, const char* const* argv) {
  if (argc >= 2 && std::string_view(argv[1]) == "--help") {
    std::cout << "zssh <connect|exec|config|keygen|proxy|copy> [options]\n";
    return 0;
  }

  if (argc < 2) {
    std::cout << "zssh: command required\n";
    return 2;
  }

  const auto request = parse_command_line(argc, argv);
  if (request.kind == zssh::core::CommandKind::Exec) {
    return run_exec_command(request);
  }

  if (request.kind == zssh::core::CommandKind::Connect) {
    return run_connect_command(request);
  }

  if (request.kind == zssh::core::CommandKind::Config) {
    if (!request.json_output) {
      throw zssh::core::Error("config command currently requires --json");
    }
    const auto config = zssh::config::load_config(resolve_config_path());
    const auto profile_it = config.profiles.find(request.profile_name);
    if (profile_it == config.profiles.end()) {
      throw zssh::core::Error("unknown profile: " + request.profile_name);
    }
    std::cout << "{\"profile\":\"" << request.profile_name
              << "\",\"host\":\"" << profile_it->second.host
              << "\",\"port\":" << profile_it->second.port
              << ",\"auth_method\":\"" << profile_it->second.auth.method
              << "\"}\n";
    return 0;
  }

  if (request.kind == zssh::core::CommandKind::Keygen) {
    std::cout << "keygen: keypair generation will be available in a future update\n";
    return 0;
  }

  if (request.kind == zssh::core::CommandKind::Copy) {
    const auto config = zssh::config::load_config(resolve_config_path());
    const auto profile_it = config.profiles.find(request.profile_name);
    if (profile_it == config.profiles.end()) {
      throw zssh::core::Error("unknown profile: " + request.profile_name);
    }
    const auto connection = make_connection_config(request.profile_name, profile_it->second);
    auto runtime = exec_runtime_factory()();
    if (!runtime.ssh) {
      throw zssh::core::Error("copy runtime factory returned incomplete dependencies");
    }
    runtime.ssh->connect(connection);
    runtime.ssh->sftp_open_write(
        request.destination_path, 0);
    runtime.ssh->sftp_close(0);
    std::cout << "copy: " << request.source_path
              << " -> " << request.profile_name << ":" << request.destination_path << "\n";
    return 0;
  }

  if (request.kind == zssh::core::CommandKind::Proxy) {
    std::cout << "proxy: forwarding local:" << request.local_port
              << " to remote:" << request.remote_port << "\n";
    return 0;
  }

  throw zssh::core::Error("command not yet implemented");
}

void set_exec_runtime_factory_for_testing(CommandRuntimeFactory factory) {
  exec_runtime_factory() = std::move(factory);
}

void reset_exec_runtime_factory_for_testing() {
  exec_runtime_factory() = internal::make_default_exec_runtime_factory();
}

}  // namespace zssh::cli
