#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

#include "tests/support/FakePlatformServices.hpp"
#include "tests/support/FakeSshAdapter.hpp"
#include "zssh/cli/CommandDispatcher.hpp"

int run_main(int argc, const char* const* argv);

namespace {

constexpr const char* kConfigContents =
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
    "      key_path: ~/.ssh/id_ed25519\n";

void write_config_file(const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << kConfigContents;
}

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, std::string value)
      : name_(name) {
    capture_original_value();
    set_environment_variable(name_, value.c_str());
  }

  explicit ScopedEnvironmentVariable(const char* name)
      : name_(name) {
    capture_original_value();
    unset_environment_variable(name_);
  }

  ~ScopedEnvironmentVariable() {
    if (had_original_value_) {
      set_environment_variable(name_, original_value_.c_str());
      return;
    }
    unset_environment_variable(name_);
  }

 private:
  static void set_environment_variable(const char* name, const char* value) {
#if defined(_WIN32)
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
  }

  static void unset_environment_variable(const char* name) {
#if defined(_WIN32)
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
  }

  void capture_original_value() {
    if (const char* current = std::getenv(name_)) {
      had_original_value_ = true;
      original_value_ = current;
    }
  }

  const char* name_;
  bool had_original_value_{false};
  std::string original_value_;
};

class ScopedWorkingDirectory {
 public:
  explicit ScopedWorkingDirectory(const std::filesystem::path& path)
      : original_path_(std::filesystem::current_path()) {
    std::filesystem::create_directories(path);
    std::filesystem::current_path(path);
  }

  ~ScopedWorkingDirectory() {
    std::filesystem::current_path(original_path_);
  }

 private:
  std::filesystem::path original_path_;
};

class ScopedConfigFile {
 public:
  ScopedConfigFile()
      : path_(std::filesystem::temp_directory_path() / make_filename()),
        env_override_("ZSSH_CONFIG_PATH", path_.string()) {
    write_config_file(path_);
  }

  ~ScopedConfigFile() {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

 private:
  static std::string make_filename() {
    static int next_id = 0;
    return "zssh-test-config-" + std::to_string(++next_id) + ".yaml";
  }

  std::filesystem::path path_;
  ScopedEnvironmentVariable env_override_;
};

struct InjectedExecRuntime {
  zssh::protocol::SshConnectionConfig connected_connection;
  std::shared_ptr<zssh::tests::PlatformContractProbe> probe;
  bool saw_connect{false};
};

class RecordingSshAdapter : public zssh::tests::FakeSshAdapter {
 public:
  explicit RecordingSshAdapter(InjectedExecRuntime* injected_runtime)
      : injected_runtime_(injected_runtime) {}

  void connect(const zssh::protocol::SshConnectionConfig& connection) override {
    FakeSshAdapter::connect(connection);
    injected_runtime_->connected_connection = connection;
    injected_runtime_->saw_connect = true;
  }

 private:
  InjectedExecRuntime* injected_runtime_;
};

zssh::cli::CommandRuntimeFactory make_injected_exec_runtime_factory(
    InjectedExecRuntime* injected_runtime,
    zssh::protocol::ExecResponse response,
    std::string stdout_text,
    std::string stderr_text,
    bool include_platform = true) {
  return [injected_runtime,
          response = std::move(response),
          stdout_text = std::move(stdout_text),
          stderr_text = std::move(stderr_text),
          include_platform]() mutable {
    auto ssh = std::make_unique<RecordingSshAdapter>(injected_runtime);
    ssh->set_exec_result(response.exit_code, std::move(stdout_text), std::move(stderr_text));
    std::shared_ptr<zssh::tests::PlatformContractProbe> probe;
    std::unique_ptr<zssh::platform::IPlatformServices> platform;
    if (include_platform) {
      probe = std::make_shared<zssh::tests::PlatformContractProbe>();
      platform = std::make_unique<zssh::tests::FakePlatformServices>(probe.get());
    }

    injected_runtime->probe = probe;

    return zssh::cli::CommandRuntime{
        std::move(ssh),
        std::move(platform),
    };
  };
}

class ScopedInjectedExecRuntimeFactory {
 public:
  explicit ScopedInjectedExecRuntimeFactory(zssh::cli::CommandRuntimeFactory factory) {
    zssh::cli::set_exec_runtime_factory_for_testing(std::move(factory));
  }

  ~ScopedInjectedExecRuntimeFactory() {
    zssh::cli::reset_exec_runtime_factory_for_testing();
  }
};

nlohmann::json parse_json_output(const std::string& output) {
  return nlohmann::json::parse(output);
}

}  // namespace

TEST(ExecJsonCliTest, ReturnsSuccessForExecJson) {
  ScopedConfigFile config_file;
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{0},
          "Linux test-host 6.9.0",
          ""));
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};

  testing::internal::CaptureStdout();
  const int exit_code = run_main(6, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const auto payload = parse_json_output(stdout_text);

  EXPECT_EQ(exit_code, 0);
  EXPECT_TRUE(payload.is_object());
  EXPECT_EQ(payload.size(), 5U);
  EXPECT_EQ(payload.at("profile"), "prod");
  EXPECT_EQ(payload.at("command"), "uname -a");
  EXPECT_EQ(payload.at("exit_code"), 0);
  EXPECT_EQ(payload.at("stdout"), "Linux test-host 6.9.0");
  EXPECT_EQ(payload.at("stderr"), "");
  EXPECT_TRUE(injected_runtime.saw_connect);
  EXPECT_EQ(injected_runtime.connected_connection.profile_name, "prod");
  EXPECT_EQ(injected_runtime.connected_connection.host, "prod.example.com");
  EXPECT_EQ(injected_runtime.connected_connection.port, 22);
  EXPECT_EQ(injected_runtime.connected_connection.auth.method, "publickey");
  EXPECT_EQ(injected_runtime.connected_connection.auth.username, "deploy");
  EXPECT_EQ(injected_runtime.connected_connection.auth.key_path, "~/.ssh/id_ed25519");
}

TEST(ExecJsonCliTest, FailsClosedWhenNoExecRuntimeIsInjected) {
  ScopedConfigFile config_file;
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};

  testing::internal::CaptureStdout();
  testing::internal::CaptureStderr();
  const int exit_code = run_main(6, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const std::string stderr_text = testing::internal::GetCapturedStderr();

  EXPECT_EQ(exit_code, 1);
  EXPECT_TRUE(stdout_text.empty());
  EXPECT_FALSE(stderr_text.empty());
}

TEST(ExecJsonCliTest, EscapesAllJsonControlCharactersInExecPayload) {
  ScopedConfigFile config_file;
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{0},
          "\b\f\n\r\t\x01",
          std::string("\x1f", 1)));

  const char* argv[] = {"zssh", "exec", "prod", "printf", "--json"};

  testing::internal::CaptureStdout();
  const int exit_code = run_main(5, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const auto payload = parse_json_output(stdout_text);

  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(payload.at("stdout"), "\b\f\n\r\t\x01");
  EXPECT_EQ(payload.at("stderr"), std::string("\x1f", 1));
}

TEST(ExecJsonCliTest, PreservesValidUtf8InExecPayload) {
  ScopedConfigFile config_file;
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{0},
          std::string("\xc3\xa9", 2),
          ""));

  const char* argv[] = {"zssh", "exec", "prod", "printf", "--json"};

  testing::internal::CaptureStdout();
  const int exit_code = run_main(5, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const auto payload = parse_json_output(stdout_text);

  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(payload.at("stdout"), std::string("\xc3\xa9", 2));
}

TEST(ExecJsonCliTest, EscapesInvalidUtf8BytesInExecPayload) {
  ScopedConfigFile config_file;
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{0},
          std::string("\x80\xff", 2),
          ""));

  const char* argv[] = {"zssh", "exec", "prod", "printf", "--json"};

  testing::internal::CaptureStdout();
  const int exit_code = run_main(5, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const auto payload = parse_json_output(stdout_text);

  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(payload.at("stdout"), std::string("\xc2\x80\xc3\xbf", 4));
}

TEST(ExecJsonCliTest, ResolvesConfigPathFromEnvironmentOutsideCurrentDirectory) {
  const auto temp_root = std::filesystem::temp_directory_path() / "zssh-exec-json-env-config";
  const auto config_path = temp_root / "custom" / "zssh.yaml";
  const auto working_directory = temp_root / "cwd";
  write_config_file(config_path);
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{0},
          "Linux test-host 6.9.0",
          ""));

  ScopedEnvironmentVariable config_path_env("ZSSH_CONFIG_PATH", config_path.string());
  ScopedWorkingDirectory cwd_guard(working_directory);
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};

  testing::internal::CaptureStdout();
  const int exit_code = run_main(6, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const auto payload = parse_json_output(stdout_text);

  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(payload.at("profile"), "prod");
}

TEST(ExecJsonCliTest, ResolvesConfigPathFromHomeConfigWhenCurrentDirectoryHasNoConfig) {
  const auto temp_root = std::filesystem::temp_directory_path() / "zssh-exec-json-home-config";
  const auto home_directory = temp_root / "home";
  const auto config_path = home_directory / ".config" / "zssh" / "zssh.yaml";
  const auto working_directory = temp_root / "cwd";
  write_config_file(config_path);
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{0},
          "Linux test-host 6.9.0",
          ""));

  ScopedEnvironmentVariable home_env("HOME", home_directory.string());
  ScopedEnvironmentVariable user_profile_env("USERPROFILE", home_directory.string());
  ScopedEnvironmentVariable config_path_env("ZSSH_CONFIG_PATH");
  ScopedWorkingDirectory cwd_guard(working_directory);
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};

  testing::internal::CaptureStdout();
  const int exit_code = run_main(6, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const auto payload = parse_json_output(stdout_text);

  EXPECT_EQ(exit_code, 0);
  EXPECT_EQ(payload.at("command"), "uname -a");
}

TEST(ExecJsonCliTest, NonJsonExecOwnsOutputAtDispatcherBoundary) {
  ScopedConfigFile config_file;
  InjectedExecRuntime injected_runtime;
  ScopedInjectedExecRuntimeFactory runtime_factory(
      make_injected_exec_runtime_factory(
          &injected_runtime,
          zssh::protocol::ExecResponse{17},
          "remote stdout",
          "remote stderr",
          true));

  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a"};

  testing::internal::CaptureStdout();
  testing::internal::CaptureStderr();
  const int exit_code = run_main(5, argv);
  const std::string stdout_text = testing::internal::GetCapturedStdout();
  const std::string stderr_text = testing::internal::GetCapturedStderr();

  EXPECT_EQ(exit_code, 17);
  EXPECT_EQ(stdout_text, "remote stdout");
  EXPECT_EQ(stderr_text, "remote stderr");
  ASSERT_NE(injected_runtime.probe, nullptr);
  EXPECT_TRUE(injected_runtime.probe->console_created);
  EXPECT_EQ(injected_runtime.probe->pump_count, 2U);
}
