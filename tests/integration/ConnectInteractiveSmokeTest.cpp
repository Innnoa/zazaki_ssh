#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "tests/support/FakePlatformServices.hpp"
#include "tests/support/FakeSshAdapter.hpp"
#include "zssh/cli/CommandDispatcher.hpp"
#include "zssh/session/SessionManager.hpp"

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
  std::ofstream file(path);
  file << kConfigContents;
}

zssh::cli::CommandRuntimeFactory make_connect_test_runtime_factory() {
  return [] {
    auto ssh = std::make_unique<zssh::tests::FakeSshAdapter>();
    auto platform = std::make_unique<zssh::tests::FakePlatformServices>(nullptr);
    return zssh::cli::CommandRuntime{
        std::move(ssh),
        std::move(platform),
    };
  };
}

class ScopedConnectTestEnv {
 public:
  ScopedConnectTestEnv()
      : path_(std::filesystem::temp_directory_path() / "zssh-connect-test.yaml") {
    write_config_file(path_);
    setenv("ZSSH_CONFIG_PATH", path_.string().c_str(), 1);
    zssh::cli::set_exec_runtime_factory_for_testing(
        make_connect_test_runtime_factory());
  }

  ~ScopedConnectTestEnv() {
    unsetenv("ZSSH_CONFIG_PATH");
    zssh::cli::reset_exec_runtime_factory_for_testing();
    std::error_code ec;
    std::filesystem::remove(path_, ec);
  }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST(ConnectInteractiveSmokeTest, ConnectCommandIsDispatched) {
  ScopedConnectTestEnv env;
  const char* argv[] = {"zssh", "connect", "prod"};

  const int exit_code = run_main(3, argv);

  EXPECT_EQ(exit_code, 0);
}
