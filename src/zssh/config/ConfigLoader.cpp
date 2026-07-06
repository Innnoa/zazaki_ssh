#include "zssh/config/ConfigLoader.hpp"

#include "zssh/core/Error.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <utility>

#include <yaml-cpp/yaml.h>

namespace zssh::config {

namespace {

const YAML::Node require_map(const YAML::Node& parent, const char* key) {
  const YAML::Node node = parent[key];
  if (!node || !node.IsMap()) {
    throw zssh::core::Error(std::string("config field '") + key + "' must be a mapping");
  }
  return node;
}

std::uint32_t require_positive_u32(const YAML::Node& parent, const char* key) {
  const auto value = parent[key].as<std::uint32_t>();
  if (value == 0) {
    throw zssh::core::Error(std::string("config field '") + key + "' must be greater than zero");
  }
  return value;
}

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

bool is_yaml_non_string_scalar(const YAML::Node& node) {
  const auto tag = node.Tag();
  return tag == "tag:yaml.org,2002:bool" ||
      tag == "tag:yaml.org,2002:int" ||
      tag == "tag:yaml.org,2002:float" ||
      tag == "tag:yaml.org,2002:null";
}

std::string require_string_scalar(
    const YAML::Node& node,
    std::string_view field_name,
    std::string_view profile_name) {
  if (!node || !node.IsScalar() || is_yaml_non_string_scalar(node)) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) + "' requires string " + std::string(field_name));
  }

  return node.as<std::string>();
}

void validate_non_empty_string(
    std::string_view value,
    std::string_view field_name,
    std::string_view profile_name) {
  if (is_blank_string(value)) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) + "' has blank " + std::string(field_name));
  }
}

void validate_non_empty_identifier(
    std::string_view value,
    std::string_view field_name) {
  if (is_blank_string(value)) {
    throw zssh::core::Error(
        std::string(field_name) + " must not be blank");
  }
}

void validate_optional_string(
    const YAML::Node& node,
    std::string_view value,
    std::string_view field_name,
    std::string_view profile_name) {
  if (node && is_blank_string(value)) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) + "' has blank " + std::string(field_name));
  }
}

void validate_port(int port, std::string_view profile_name) {
  if (port < 1 || port > 65535) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) + "' has invalid port: " + std::to_string(port));
  }
}

bool is_supported_auth_method(const std::string& method) {
  constexpr std::array<std::string_view, 2> kSupportedAuthMethods = {
      "publickey",
      "password",
  };

  for (const auto supported_method : kSupportedAuthMethods) {
    if (method == supported_method) {
      return true;
    }
  }

  return false;
}

template <std::size_t N>
void reject_unknown_keys(
    const YAML::Node& node,
    const std::array<std::string_view, N>& allowed_keys,
    std::string_view context) {
  for (const auto& entry : node) {
    const auto key = entry.first.as<std::string>();
    bool is_allowed = false;
    for (const auto allowed_key : allowed_keys) {
      if (key == allowed_key) {
        is_allowed = true;
        break;
      }
    }

    if (!is_allowed) {
      throw zssh::core::Error(
          std::string(context) + " contains unknown key: " + key);
    }
  }
}

void validate_auth_config(const AuthConfig& auth, const YAML::Node& auth_node, std::string_view profile_name) {
  if (!is_supported_auth_method(auth.method)) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) + "' has unsupported auth method: " + auth.method);
  }

  validate_optional_string(auth_node["password_command"], auth.password_command, "auth.password_command", profile_name);
  validate_optional_string(auth_node["key_path"], auth.key_path, "auth.key_path", profile_name);

  if (auth.method == "password" && auth.password_command.empty()) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) + "' requires auth.password_command for password auth");
  }

  if (auth.method == "publickey" && auth.key_path.empty() && !auth.use_agent) {
    throw zssh::core::Error(
        "profile '" + std::string(profile_name) +
        "' requires auth.key_path or auth.use_agent=true for publickey auth");
  }
}

}  // namespace

AppConfig load_config(const std::filesystem::path& path) {
  try {
    std::ifstream input(path);
    if (!input.is_open()) {
      throw zssh::core::Error("failed to open config '" + path.string() + "'");
    }

    const YAML::Node root = YAML::Load(input);
    AppConfig config;

    reject_unknown_keys(
        root,
        std::array<std::string_view, 2>{
            "defaults",
            "profiles",
        },
        "config root");

    const YAML::Node defaults = require_map(root, "defaults");
    reject_unknown_keys(
        defaults,
        std::array<std::string_view, 3>{
            "stdout_high_watermark_bytes",
            "stderr_high_watermark_bytes",
            "stdin_low_watermark_bytes",
        },
        "defaults");
    config.defaults.stdout_high_watermark_bytes = require_positive_u32(defaults, "stdout_high_watermark_bytes");
    config.defaults.stderr_high_watermark_bytes = require_positive_u32(defaults, "stderr_high_watermark_bytes");
    config.defaults.stdin_low_watermark_bytes = require_positive_u32(defaults, "stdin_low_watermark_bytes");

    const YAML::Node profiles = require_map(root, "profiles");
    for (const auto& profile_entry : profiles) {
      const auto profile_name = profile_entry.first.as<std::string>();
      validate_non_empty_identifier(profile_name, "profile name");
      const YAML::Node profile_node = profile_entry.second;
      reject_unknown_keys(
          profile_node,
          std::array<std::string_view, 4>{
              "host",
              "port",
              "auth",
              "theme",
          },
          "profile '" + profile_name + "'");
      const YAML::Node auth_node = require_map(profile_node, "auth");
      reject_unknown_keys(
          auth_node,
          std::array<std::string_view, 5>{
              "method",
              "username",
              "password_command",
              "key_path",
              "use_agent",
          },
          "profile '" + profile_name + "' auth");

      ProfileConfig profile;
      profile.host = require_string_scalar(profile_node["host"], "host", profile_name);
      profile.port = profile_node["port"].as<int>(22);
      validate_port(profile.port, profile_name);
      profile.theme = profile_node["theme"] ? require_string_scalar(profile_node["theme"], "theme", profile_name) : "catppuccin-mocha";
      profile.auth.method = require_string_scalar(auth_node["method"], "auth.method", profile_name);
      profile.auth.username = require_string_scalar(auth_node["username"], "auth.username", profile_name);
      profile.auth.password_command = auth_node["password_command"] ? require_string_scalar(auth_node["password_command"], "auth.password_command", profile_name) : "";
      profile.auth.key_path = auth_node["key_path"] ? require_string_scalar(auth_node["key_path"], "auth.key_path", profile_name) : "";
      profile.auth.use_agent = auth_node["use_agent"].as<bool>(true);
      validate_non_empty_string(profile.host, "host", profile_name);
      validate_optional_string(profile_node["theme"], profile.theme, "theme", profile_name);
      validate_non_empty_string(profile.auth.username, "auth.username", profile_name);
      validate_auth_config(profile.auth, auth_node, profile_name);

      const auto [_, inserted] = config.profiles.emplace(profile_name, std::move(profile));
      if (!inserted) {
        throw zssh::core::Error("duplicate profile definition: " + profile_name);
      }
    }

    return config;
  } catch (const zssh::core::Error&) {
    throw;
  } catch (const YAML::Exception& error) {
    throw zssh::core::Error(std::string("failed to load config '") + path.string() + "': " + error.what());
  }
}

}  // namespace zssh::config
