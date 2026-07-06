#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "zssh/core/Error.hpp"
#include "zssh/config/ConfigLoader.hpp"

namespace {

std::filesystem::path fixture_config_path(std::string_view file_name) {
  const auto test_directory = std::filesystem::path(__FILE__).parent_path();
  return (test_directory.parent_path().parent_path() / "fixtures" / "config" / file_name)
      .lexically_normal();
}

std::string write_temp_config(const std::string& file_stem, const std::string& contents) {
  static std::atomic<unsigned long long> file_counter{0};

  const auto suffix = std::to_string(
      std::chrono::steady_clock::now().time_since_epoch().count()) +
      "-" + std::to_string(file_counter.fetch_add(1, std::memory_order_relaxed));
  const auto path = std::filesystem::temp_directory_path() /
      (file_stem + "-" + suffix + ".yaml");
  std::ofstream output(path);
  output << contents;
  output.close();
  return path.string();
}

}  // namespace

TEST(ConfigLoaderTest, LoadsDefaultProfileAndWatermarks) {
  const auto config = zssh::config::load_config(fixture_config_path("default.yaml").string());

  ASSERT_TRUE(config.profiles.contains("prod"));
  EXPECT_EQ(config.defaults.stdout_high_watermark_bytes, 262144);
  EXPECT_EQ(config.profiles.at("prod").host, "prod.example.com");
}

TEST(ConfigLoaderTest, LoadsPasswordAuthProfileFixture) {
  const auto config = zssh::config::load_config(fixture_config_path("password.yaml").string());

  ASSERT_TRUE(config.profiles.contains("staging"));
  const auto& profile = config.profiles.at("staging");
  EXPECT_EQ(profile.auth.method, "password");
  EXPECT_EQ(profile.auth.password_command, "pass show zssh/staging");
}

TEST(ConfigLoaderTest, TempConfigHelperUsesUniquePathsPerInvocation) {
  const auto first_path = write_temp_config("zssh-config-unique", "profiles: {}\n");
  const auto second_path = write_temp_config("zssh-config-unique", "profiles: {}\n");

  EXPECT_NE(first_path, second_path);

  std::filesystem::remove(first_path);
  std::filesystem::remove(second_path);
}

TEST(ConfigLoaderTest, RejectsMissingProfilesMapping) {
  const auto path = write_temp_config(
      "zssh-config-missing-profiles",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsMissingDefaultsMapping) {
  const auto path = write_temp_config(
      "zssh-config-missing-defaults",
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsUnsupportedAuthMethod) {
  const auto path = write_temp_config(
      "zssh-config-invalid-auth-method",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: token\n"
      "      username: deploy\n"
      "      password_command: pass show zssh/prod\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsPasswordAuthWithoutPasswordCommand) {
  const auto path = write_temp_config(
      "zssh-config-missing-password-command",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: password\n"
      "      username: deploy\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsPasswordAuthWithEmptyPasswordCommand) {
  const auto path = write_temp_config(
      "zssh-config-empty-password-command",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: password\n"
      "      username: deploy\n"
      "      password_command: \"\"\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, AcceptsPublicKeyAuthWithAgentOnly) {
  const auto path = write_temp_config(
      "zssh-config-agent-only-publickey",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      use_agent: true\n");

  const auto config = zssh::config::load_config(path);

  ASSERT_TRUE(config.profiles.contains("prod"));
  EXPECT_TRUE(config.profiles.at("prod").auth.use_agent);
  EXPECT_TRUE(config.profiles.at("prod").auth.key_path.empty());
}

TEST(ConfigLoaderTest, DefaultsPublicKeyAuthToAgentWhenUseAgentIsOmitted) {
  const auto path = write_temp_config(
      "zssh-config-implicit-agent-publickey",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n");

  const auto config = zssh::config::load_config(path);

  ASSERT_TRUE(config.profiles.contains("prod"));
  EXPECT_TRUE(config.profiles.at("prod").auth.use_agent);
  EXPECT_TRUE(config.profiles.at("prod").auth.key_path.empty());
}

TEST(ConfigLoaderTest, RejectsPublicKeyAuthWithoutCredentialSource) {
  const auto path = write_temp_config(
      "zssh-config-missing-publickey-credential-source",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      use_agent: false\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsPublicKeyAuthWithEmptyKeyPathWhenAgentDisabled) {
  const auto path = write_temp_config(
      "zssh-config-empty-publickey-path",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: \"\"\n"
      "      use_agent: false\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsDuplicateProfileDefinitions) {
  const auto path = write_temp_config(
      "zssh-config-duplicate-profile-definitions",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n"
      "  prod:\n"
      "    host: backup.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: backup\n"
      "      key_path: ~/.ssh/id_backup_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsBlankProfileName) {
  const auto path = write_temp_config(
      "zssh-config-blank-profile-name",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  \"   \":\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsUnknownDefaultsKey) {
  const auto path = write_temp_config(
      "zssh-config-unknown-defaults-key",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "  stdout_hgh_watermark_bytes: 123\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsUnknownTopLevelKey) {
  const auto path = write_temp_config(
      "zssh-config-unknown-top-level-key",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n"
      "telemetry:\n"
      "  enabled: true\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsUnknownProfileKey) {
  const auto path = write_temp_config(
      "zssh-config-unknown-profile-key",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    prot: 22\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsUnknownAuthKey) {
  const auto path = write_temp_config(
      "zssh-config-unknown-auth-key",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n"
      "      agent_socket: /tmp/ssh-agent.sock\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsPortOutsideValidRange) {
  const auto path = write_temp_config(
      "zssh-config-invalid-port",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    port: 65536\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsEmptyHost) {
  const auto path = write_temp_config(
      "zssh-config-empty-host",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: \"\"\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsWhitespaceOnlyHost) {
  const auto path = write_temp_config(
      "zssh-config-whitespace-host",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: \"   \"\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsEmptyAuthUsername) {
  const auto path = write_temp_config(
      "zssh-config-empty-username",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: \"\"\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsWhitespaceOnlyAuthUsername) {
  const auto path = write_temp_config(
      "zssh-config-whitespace-username",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: \"   \"\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsWhitespaceOnlyPasswordCommand) {
  const auto path = write_temp_config(
      "zssh-config-whitespace-password-command",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: password\n"
      "      username: deploy\n"
      "      password_command: \"   \"\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsWhitespaceOnlyKeyPathWhenAgentDisabled) {
  const auto path = write_temp_config(
      "zssh-config-whitespace-key-path",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: \"   \"\n"
      "      use_agent: false\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsWhitespaceOnlyTheme) {
  const auto path = write_temp_config(
      "zssh-config-whitespace-theme",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    theme: \"   \"\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsEmptyThemeWhenProvided) {
  const auto path = write_temp_config(
      "zssh-config-empty-theme",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    theme: \"\"\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsEmptyOptionalKeyPathWhenProvided) {
  const auto path = write_temp_config(
      "zssh-config-empty-optional-key-path",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 262144\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: \"\"\n"
      "      use_agent: true\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}

TEST(ConfigLoaderTest, RejectsZeroWatermarkValue) {
  const auto path = write_temp_config(
      "zssh-config-zero-watermark",
      "defaults:\n"
      "  stdout_high_watermark_bytes: 0\n"
      "  stderr_high_watermark_bytes: 262144\n"
      "  stdin_low_watermark_bytes: 65536\n"
      "profiles:\n"
      "  prod:\n"
      "    host: prod.example.com\n"
      "    auth:\n"
      "      method: publickey\n"
      "      username: deploy\n"
      "      key_path: ~/.ssh/id_ed25519\n");

  EXPECT_THROW(static_cast<void>(zssh::config::load_config(path)), zssh::core::Error);
}
