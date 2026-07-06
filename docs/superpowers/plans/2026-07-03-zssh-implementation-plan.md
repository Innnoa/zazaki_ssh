# zssh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first production-ready `zssh` codebase that implements the approved CLI, layered architecture, YAML configuration, `libssh` protocol integration, terminal rendering, session/backpressure control, and AI-friendly `--json` mode across Linux and Windows, while making copy/transfer reusable by a future `zazaki_scp` facade.

**Architecture:** The codebase is organized around `CLI -> Session Manager -> Protocol Adapter(libssh) -> Terminal Renderer -> Platform Abstraction`, with narrow interfaces and dependency injection at every boundary that needs testing. Copy/transfer is treated as reusable runtime behavior rather than logic owned by the `zssh` CLI alone, so later frontends such as `zazaki_scp` can share the same transport, authentication, and error model. Implementation proceeds from stable build/test scaffolding, then shared domain/config/CLI types, then backpressure/session orchestration, then protocol/rendering/platform backends, and finally command completion and cross-platform verification.

**Tech Stack:** `C++20`, `CMake`, `libssh`, `yaml-cpp`, `nlohmann/json`, `GoogleTest`, Linux `epoll` + PTY, Windows `IOCP` + `ConPTY`

---

## File Structure

### Top-level build and automation files

- `CMakeLists.txt`: top-level targets, options, dependency wiring, test enablement.
- `cmake/Dependencies.cmake`: `libssh`, `yaml-cpp`, `nlohmann_json`, `GTest` discovery.
- `cmake/Warnings.cmake`: compiler warnings and sanitizer options.
- `.github/workflows/ci.yml`: Linux and Windows build/test matrix.

### Application entry and CLI

- `src/main.cpp`: process entrypoint, exception boundary, dispatcher call.
- `src/zssh/cli/CliParser.hpp`
- `src/zssh/cli/CliParser.cpp`: parse subcommands, flags, `--json`, profile selection.
- `src/zssh/cli/CommandDispatcher.hpp`
- `src/zssh/cli/CommandDispatcher.cpp`: load config, create request, execute session flow.
- `src/zazaki_scp/main.cpp`: future thin facade CLI that maps `scp`-style argv into the shared copy runtime. Not required for the first `zssh` vertical slices, but the runtime must stay linkable by this future binary.

### Shared core and config

- `src/zssh/core/CommandModel.hpp`: command enums and normalized request/result types.
- `src/zssh/core/Error.hpp`: structured errors and exit-code mapping.
- `src/zssh/config/AppConfig.hpp`: YAML-backed profile and defaults model.
- `src/zssh/config/ConfigLoader.hpp`
- `src/zssh/config/ConfigLoader.cpp`: load and validate YAML config.
- `docs/schemas/config.schema.yaml`: frozen configuration schema for docs and fixtures.
- `docs/schemas/exec-result.schema.json`: frozen `exec --json` output schema.

### Session and backpressure

- `src/zssh/session/SessionManager.hpp`
- `src/zssh/session/SessionManager.cpp`: runtime authority for connect/exec/copy/proxy flows.
- `src/zssh/session/BoundedByteQueue.hpp`: byte queue with high/low watermark accounting.
- `src/zssh/session/BackpressureController.hpp`
- `src/zssh/session/BackpressureController.cpp`: pause/resume policy for stdout/stderr/stdin streams.

### Copy runtime

- `src/zssh/copy/CopyRequest.hpp`: normalized copy direction, paths, and mode flags shared by `zssh copy` and future `zazaki_scp`.
- `src/zssh/copy/CopyService.hpp`
- `src/zssh/copy/CopyService.cpp`: reusable transfer orchestration that sits above the SSH protocol adapter and below CLI-specific argument shapes.

### Protocol layer

- `src/zssh/protocol/ISshAdapter.hpp`: protocol interface for tests and orchestration.
- `src/zssh/protocol/LibsshAdapter.hpp`
- `src/zssh/protocol/LibsshAdapter.cpp`: real `libssh` binding.
- `src/zssh/protocol/LibsshError.hpp`: mapping `libssh` failures to internal errors.

### Rendering and theme

- `src/zssh/renderer/ITerminalRenderer.hpp`: presentation interface.
- `src/zssh/renderer/CatppuccinMochaTheme.hpp`: color constants and style decisions.
- `src/zssh/renderer/AnsiTerminalRenderer.hpp`
- `src/zssh/renderer/AnsiTerminalRenderer.cpp`: ANSI output path for interactive rendering.

### Platform layer

- `src/zssh/platform/IPlatformServices.hpp`: event loop, console, PTY/ConPTY, clock, file operations.
- `src/zssh/platform/linux/LinuxPlatformServices.hpp`
- `src/zssh/platform/linux/LinuxPlatformServices.cpp`: `epoll` and PTY implementation.
- `src/zssh/platform/windows/WindowsPlatformServices.hpp`
- `src/zssh/platform/windows/WindowsPlatformServices.cpp`: `IOCP` and `ConPTY` implementation.

### Tests and fixtures

- `tests/support/FakeSshAdapter.hpp`: deterministic fake transport for unit tests.
- `tests/support/FakePlatformServices.hpp`: deterministic platform fake.
- `tests/support/FakeTerminalRenderer.hpp`: renderer spy for session assertions.
- `tests/unit/cli/CliParserTest.cpp`
- `tests/unit/config/ConfigLoaderTest.cpp`
- `tests/unit/session/BackpressureControllerTest.cpp`
- `tests/unit/session/SessionManagerExecTest.cpp`
- `tests/unit/renderer/AnsiTerminalRendererTest.cpp`
- `tests/unit/platform/PlatformServicesContractTest.cpp`
- `tests/integration/ExecJsonCliTest.cpp`
- `tests/integration/ConnectInteractiveSmokeTest.cpp`
- `tests/integration/CopyProxyWorkflowTest.cpp`
- `tests/fixtures/config/default.yaml`
- `tests/fixtures/config/password.yaml`
- `tests/fixtures/config/keypair.yaml`

## Implementation sequence

1. Build the project skeleton and CI so every later task has a stable build/test loop.
2. Freeze the shared request/config/error model before wiring session logic.
3. Implement backpressure and session orchestration against fakes before touching `libssh`.
4. Add the real `libssh` adapter and `exec --json` vertical slice.
5. Add rendering and interactive connect flow.
6. Add the remaining commands: `config`, `keygen`, `copy`, `proxy`, with `copy` implemented as reusable runtime behavior.
7. Leave a clean seam for a future `zazaki_scp` thin frontend without creating a second transport stack.
7. Add Linux and Windows platform implementations plus final verification.

### Task 1: Bootstrap the build, test, and CI scaffold

**Files:**
- Create: `CMakeLists.txt`
- Create: `cmake/Dependencies.cmake`
- Create: `cmake/Warnings.cmake`
- Create: `src/main.cpp`
- Create: `tests/unit/core/StartupSmokeTest.cpp`
- Create: `.github/workflows/ci.yml`

- [ ] **Step 1: Write the failing smoke test and build target declarations**

```cpp
// tests/unit/core/StartupSmokeTest.cpp
#include <gtest/gtest.h>

int run_main(int argc, const char** argv);

TEST(StartupSmokeTest, HelpCommandReturnsSuccess) {
  const char* argv[] = {"zssh", "--help"};
  EXPECT_EQ(run_main(2, argv), 0);
}
```

```cmake
# CMakeLists.txt
cmake_minimum_required(VERSION 3.24)
project(zssh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZSSH_BUILD_TESTS "Build zssh tests" ON)

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

add_library(zssh_warnings INTERFACE)
zssh_apply_warnings(zssh_warnings)

add_executable(zssh src/main.cpp)
target_link_libraries(zssh PRIVATE zssh_warnings)

if(ZSSH_BUILD_TESTS)
  enable_testing()
  add_executable(zssh_unit_tests tests/unit/core/StartupSmokeTest.cpp src/main.cpp)
  target_link_libraries(zssh_unit_tests PRIVATE GTest::gtest_main zssh_warnings)
  include(GoogleTest)
  gtest_discover_tests(zssh_unit_tests)
endif()
```

- [ ] **Step 2: Run the focused test to verify it fails**

Run: `cmake -S . -B build -DZSSH_BUILD_TESTS=ON && cmake --build build --target zssh_unit_tests && ctest --test-dir build -R StartupSmokeTest --output-on-failure`
Expected: FAIL because `run_main` is declared in the test but not implemented yet.

- [ ] **Step 3: Implement the minimal entrypoint and dependency wiring**

```cpp
// src/main.cpp
#include <iostream>
#include <string_view>

int run_main(int argc, const char** argv) {
  if (argc >= 2 && std::string_view(argv[1]) == "--help") {
    std::cout << "zssh <connect|exec|config|keygen|proxy|copy> [options]\n";
    return 0;
  }

  std::cout << "zssh: command required\n";
  return 2;
}

int main(int argc, const char** argv) {
  return run_main(argc, argv);
}
```

```cmake
# cmake/Dependencies.cmake
include(FetchContent)

find_package(libssh REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)
FetchContent_MakeAvailable(googletest)
```

```cmake
# cmake/Warnings.cmake
function(zssh_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} INTERFACE /W4 /permissive-)
  else()
    target_compile_options(${target} INTERFACE -Wall -Wextra -Wpedantic)
  endif()
endfunction()
```

```yaml
# .github/workflows/ci.yml
name: ci

on:
  push:
  pull_request:

jobs:
  build-and-test:
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - name: Configure
        run: cmake -S . -B build -DZSSH_BUILD_TESTS=ON
      - name: Build
        run: cmake --build build --config Release
      - name: Test
        run: ctest --test-dir build -C Release --output-on-failure
```

- [ ] **Step 4: Run the smoke test and full build**

Run: `cmake -S . -B build -DZSSH_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS with `StartupSmokeTest.HelpCommandReturnsSuccess` green on the local toolchain.

### Task 2: Freeze the command model, YAML config schema, and CLI parser

**Files:**
- Create: `src/zssh/core/CommandModel.hpp`
- Create: `src/zssh/core/Error.hpp`
- Create: `src/zssh/config/AppConfig.hpp`
- Create: `src/zssh/config/ConfigLoader.hpp`
- Create: `src/zssh/config/ConfigLoader.cpp`
- Create: `src/zssh/cli/CliParser.hpp`
- Create: `src/zssh/cli/CliParser.cpp`
- Create: `tests/unit/cli/CliParserTest.cpp`
- Create: `tests/unit/config/ConfigLoaderTest.cpp`
- Create: `tests/fixtures/config/default.yaml`
- Create: `tests/fixtures/config/password.yaml`
- Create: `docs/schemas/config.schema.yaml`

- [ ] **Step 1: Write failing tests for command parsing and YAML loading**

```cpp
// tests/unit/cli/CliParserTest.cpp
#include <gtest/gtest.h>
#include "zssh/cli/CliParser.hpp"

TEST(CliParserTest, ParsesExecJsonCommand) {
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};
  const auto request = zssh::cli::parse_command_line(6, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Exec);
  EXPECT_EQ(request.profile_name, "prod");
  EXPECT_EQ(request.remote_command, "uname -a");
  EXPECT_TRUE(request.json_output);
}
```

```cpp
// tests/unit/config/ConfigLoaderTest.cpp
#include <gtest/gtest.h>
#include "zssh/config/ConfigLoader.hpp"

TEST(ConfigLoaderTest, LoadsDefaultProfileAndWatermarks) {
  const auto config = zssh::config::load_config("tests/fixtures/config/default.yaml");

  ASSERT_TRUE(config.profiles.contains("prod"));
  EXPECT_EQ(config.defaults.stdout_high_watermark_bytes, 262144);
  EXPECT_EQ(config.profiles.at("prod").host, "prod.example.com");
}
```

- [ ] **Step 2: Run the focused tests to verify they fail**

Run: `cmake --build build --target zssh_unit_tests && ctest --test-dir build -R "CliParserTest|ConfigLoaderTest" --output-on-failure`
Expected: FAIL because the parser/config loader headers and symbols do not exist yet.

- [ ] **Step 3: Implement the shared command and configuration model**

```cpp
// src/zssh/core/CommandModel.hpp
#pragma once

#include <optional>
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
};

struct CommandResult {
  int exit_code{0};
  std::string stdout_text;
  std::string stderr_text;
};

}  // namespace zssh::core
```

```cpp
// src/zssh/config/AppConfig.hpp
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
```

```cpp
// src/zssh/config/ConfigLoader.hpp
#pragma once

#include <string>

#include "zssh/config/AppConfig.hpp"

namespace zssh::config {

AppConfig load_config(const std::string& path);

}  // namespace zssh::config
```

```cpp
// src/zssh/cli/CliParser.hpp
#pragma once

#include "zssh/core/CommandModel.hpp"

namespace zssh::cli {

zssh::core::CommandRequest parse_command_line(int argc, const char** argv);

}  // namespace zssh::cli
```

```yaml
# tests/fixtures/config/default.yaml
defaults:
  stdout_high_watermark_bytes: 262144
  stderr_high_watermark_bytes: 262144
  stdin_low_watermark_bytes: 65536

profiles:
  prod:
    host: prod.example.com
    port: 22
    theme: catppuccin-mocha
    auth:
      method: publickey
      username: deploy
      key_path: ~/.ssh/id_ed25519
      use_agent: true
```

```yaml
# docs/schemas/config.schema.yaml
type: map
mapping:
  defaults:
    type: map
    mapping:
      stdout_high_watermark_bytes: {type: int, required: true}
      stderr_high_watermark_bytes: {type: int, required: true}
      stdin_low_watermark_bytes: {type: int, required: true}
  profiles:
    type: map
    mapping:
      =:
        type: map
        mapping:
          host: {type: str, required: true}
          port: {type: int, required: false}
          theme: {type: str, required: false}
```

- [ ] **Step 4: Implement the loader and parser**

```cpp
// src/zssh/config/ConfigLoader.cpp
#include "zssh/config/ConfigLoader.hpp"

#include <yaml-cpp/yaml.h>

namespace zssh::config {

AppConfig load_config(const std::string& path) {
  const YAML::Node root = YAML::LoadFile(path);
  AppConfig config;

  config.defaults.stdout_high_watermark_bytes = root["defaults"]["stdout_high_watermark_bytes"].as<std::uint32_t>();
  config.defaults.stderr_high_watermark_bytes = root["defaults"]["stderr_high_watermark_bytes"].as<std::uint32_t>();
  config.defaults.stdin_low_watermark_bytes = root["defaults"]["stdin_low_watermark_bytes"].as<std::uint32_t>();

  for (const auto& profile_entry : root["profiles"]) {
    ProfileConfig profile;
    profile.host = profile_entry.second["host"].as<std::string>();
    profile.port = profile_entry.second["port"].as<int>(22);
    profile.theme = profile_entry.second["theme"].as<std::string>("catppuccin-mocha");
    profile.auth.method = profile_entry.second["auth"]["method"].as<std::string>();
    profile.auth.username = profile_entry.second["auth"]["username"].as<std::string>();
    profile.auth.password_command = profile_entry.second["auth"]["password_command"].as<std::string>("");
    profile.auth.key_path = profile_entry.second["auth"]["key_path"].as<std::string>("");
    profile.auth.use_agent = profile_entry.second["auth"]["use_agent"].as<bool>(true);
    config.profiles.emplace(profile_entry.first.as<std::string>(), std::move(profile));
  }

  return config;
}

}  // namespace zssh::config
```

```cpp
// src/zssh/cli/CliParser.cpp
#include "zssh/cli/CliParser.hpp"

#include <sstream>
#include <stdexcept>
#include <string_view>

namespace zssh::cli {

zssh::core::CommandRequest parse_command_line(int argc, const char** argv) {
  if (argc < 3) {
    throw std::runtime_error("command and profile are required");
  }

  zssh::core::CommandRequest request{};
  request.kind = std::string_view(argv[1]) == "exec" ? zssh::core::CommandKind::Exec : zssh::core::CommandKind::Connect;
  request.profile_name = argv[2];

  std::ostringstream remote_command;
  for (int i = 3; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--json") {
      request.json_output = true;
      continue;
    }
    if (!remote_command.str().empty()) {
      remote_command << ' ';
    }
    remote_command << argv[i];
  }

  request.remote_command = remote_command.str();
  return request;
}

}  // namespace zssh::cli
```

- [ ] **Step 5: Run the parser/config tests and the full unit suite**

Run: `cmake --build build && ctest --test-dir build -R "CliParserTest|ConfigLoaderTest|StartupSmokeTest" --output-on-failure`
Expected: PASS with the CLI request model and YAML defaults loaded from fixtures.

### Task 3: Add backpressure primitives and platform contracts before real network I/O

**Files:**
- Create: `src/zssh/platform/IPlatformServices.hpp`
- Create: `src/zssh/session/BoundedByteQueue.hpp`
- Create: `src/zssh/session/BackpressureController.hpp`
- Create: `src/zssh/session/BackpressureController.cpp`
- Create: `tests/support/FakePlatformServices.hpp`
- Create: `tests/unit/session/BackpressureControllerTest.cpp`
- Create: `tests/unit/platform/PlatformServicesContractTest.cpp`

- [ ] **Step 1: Write failing tests for watermark transitions and platform contracts**

```cpp
// tests/unit/session/BackpressureControllerTest.cpp
#include <gtest/gtest.h>
#include "zssh/session/BackpressureController.hpp"

TEST(BackpressureControllerTest, RequestsPauseAtHighWatermarkAndResumeAtLowWatermark) {
  zssh::session::BackpressureController controller(1024, 256);

  EXPECT_FALSE(controller.should_pause(512));
  EXPECT_TRUE(controller.should_pause(1024));
  EXPECT_TRUE(controller.should_resume(128));
}
```

```cpp
// tests/unit/platform/PlatformServicesContractTest.cpp
#include <gtest/gtest.h>
#include "tests/support/FakePlatformServices.hpp"

TEST(PlatformServicesContractTest, FakePlatformCanCreateInteractiveConsole) {
  zssh::tests::FakePlatformServices platform;
  EXPECT_TRUE(platform.console_created());
}
```

- [ ] **Step 2: Run the focused tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R "BackpressureControllerTest|PlatformServicesContractTest" --output-on-failure`
Expected: FAIL because the queue/controller/platform interfaces do not exist yet.

- [ ] **Step 3: Implement queue, controller, and platform interfaces**

```cpp
// src/zssh/session/BoundedByteQueue.hpp
#pragma once

#include <cstddef>
#include <deque>
#include <string>

namespace zssh::session {

class BoundedByteQueue {
 public:
  explicit BoundedByteQueue(std::size_t high_watermark) : high_watermark_(high_watermark) {}

  void push(std::string chunk) {
    size_ += chunk.size();
    chunks_.push_back(std::move(chunk));
  }

  [[nodiscard]] std::size_t size() const { return size_; }
  [[nodiscard]] std::size_t high_watermark() const { return high_watermark_; }

 private:
  std::size_t high_watermark_;
  std::size_t size_{0};
  std::deque<std::string> chunks_;
};

}  // namespace zssh::session
```

```cpp
// src/zssh/session/BackpressureController.hpp
#pragma once

#include <cstddef>

namespace zssh::session {

class BackpressureController {
 public:
  BackpressureController(std::size_t pause_threshold, std::size_t resume_threshold)
      : pause_threshold_(pause_threshold), resume_threshold_(resume_threshold) {}

  [[nodiscard]] bool should_pause(std::size_t buffered_bytes) const;
  [[nodiscard]] bool should_resume(std::size_t buffered_bytes) const;

 private:
  std::size_t pause_threshold_;
  std::size_t resume_threshold_;
};

}  // namespace zssh::session
```

```cpp
// src/zssh/platform/IPlatformServices.hpp
#pragma once

namespace zssh::platform {

class IPlatformServices {
 public:
  virtual ~IPlatformServices() = default;
  virtual void create_console() = 0;
  virtual void pump_events_once() = 0;
};

}  // namespace zssh::platform
```

- [ ] **Step 4: Run the session/platform contract tests**

Run: `cmake --build build && ctest --test-dir build -R "BackpressureControllerTest|PlatformServicesContractTest" --output-on-failure`
Expected: PASS with deterministic pause/resume behavior and a minimal platform abstraction contract.

### Task 4: Implement the session manager and `exec --json` vertical slice against fakes first

**Files:**
- Modify: `src/main.cpp`
- Create: `src/zssh/session/SessionManager.hpp`
- Create: `src/zssh/session/SessionManager.cpp`
- Create: `src/zssh/protocol/ISshAdapter.hpp`
- Create: `src/zssh/renderer/ITerminalRenderer.hpp`
- Create: `src/zssh/cli/CommandDispatcher.hpp`
- Create: `src/zssh/cli/CommandDispatcher.cpp`
- Create: `docs/schemas/exec-result.schema.json`
- Create: `tests/support/FakeSshAdapter.hpp`
- Create: `tests/support/FakeTerminalRenderer.hpp`
- Create: `tests/unit/session/SessionManagerExecTest.cpp`
- Create: `tests/integration/ExecJsonCliTest.cpp`

- [ ] **Step 1: Write failing tests for session orchestration and JSON output**

```cpp
// tests/unit/session/SessionManagerExecTest.cpp
#include <gtest/gtest.h>
#include "tests/support/FakePlatformServices.hpp"
#include "tests/support/FakeSshAdapter.hpp"
#include "tests/support/FakeTerminalRenderer.hpp"
#include "zssh/session/SessionManager.hpp"

TEST(SessionManagerExecTest, ExecCommandReturnsExitCodeAndStdout) {
  zssh::tests::FakeSshAdapter ssh;
  ssh.set_exec_result(0, "Linux test-host 6.9.0", "");

  zssh::tests::FakePlatformServices platform;
  zssh::tests::FakeTerminalRenderer renderer;
  zssh::session::SessionManager manager(ssh, platform, renderer);

  const auto result = manager.exec("prod", "uname -a", true);
  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "Linux test-host 6.9.0");
}
```

```cpp
// tests/integration/ExecJsonCliTest.cpp
#include <gtest/gtest.h>

int run_main(int argc, const char** argv);

TEST(ExecJsonCliTest, ReturnsSuccessForExecJson) {
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};
  EXPECT_EQ(run_main(6, argv), 0);
}
```

- [ ] **Step 2: Run the focused tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R "SessionManagerExecTest|ExecJsonCliTest" --output-on-failure`
Expected: FAIL because the session manager, fake transport, and dispatcher do not exist yet.

- [ ] **Step 3: Implement the protocol interface, session manager, dispatcher, and JSON schema**

```cpp
// src/zssh/protocol/ISshAdapter.hpp
#pragma once

#include <string>

namespace zssh::protocol {

struct ExecResponse {
  int exit_code;
  std::string stdout_text;
  std::string stderr_text;
};

class ISshAdapter {
 public:
  virtual ~ISshAdapter() = default;
  virtual void connect(const std::string& profile_name) = 0;
  virtual ExecResponse exec(const std::string& remote_command) = 0;
};

}  // namespace zssh::protocol
```

```cpp
// src/zssh/renderer/ITerminalRenderer.hpp
#pragma once

#include <string>

namespace zssh::renderer {

class ITerminalRenderer {
 public:
  virtual ~ITerminalRenderer() = default;
  virtual std::string render_status_line(const std::string& text) = 0;
  virtual void write_remote_output(const std::string& chunk) = 0;
};

}  // namespace zssh::renderer
```

```cpp
// src/zssh/session/SessionManager.hpp
#pragma once

#include <string>

#include "zssh/core/CommandModel.hpp"
#include "zssh/platform/IPlatformServices.hpp"
#include "zssh/protocol/ISshAdapter.hpp"
#include "zssh/renderer/ITerminalRenderer.hpp"

namespace zssh::session {

class SessionManager {
 public:
  SessionManager(zssh::protocol::ISshAdapter& ssh,
                 zssh::platform::IPlatformServices& platform,
                 zssh::renderer::ITerminalRenderer& renderer)
      : ssh_(ssh), platform_(platform), renderer_(renderer) {}

  zssh::core::CommandResult exec(const std::string& profile_name,
                                 const std::string& remote_command,
                                 bool json_output);

 private:
  zssh::protocol::ISshAdapter& ssh_;
  zssh::platform::IPlatformServices& platform_;
  zssh::renderer::ITerminalRenderer& renderer_;
};

}  // namespace zssh::session
```

```cpp
// src/main.cpp
#include <exception>
#include <iostream>

#include "zssh/cli/CommandDispatcher.hpp"

int run_main(int argc, const char** argv) {
  try {
    return zssh::cli::run_command(argc, argv);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
```

```cpp
// src/zssh/cli/CommandDispatcher.hpp
#pragma once

namespace zssh::cli {

int run_command(int argc, const char** argv);

}  // namespace zssh::cli
```

```cpp
// src/zssh/cli/CommandDispatcher.cpp
#include "zssh/cli/CommandDispatcher.hpp"

#include "zssh/cli/CliParser.hpp"
#include "zssh/config/ConfigLoader.hpp"

namespace zssh::cli {

int run_command(int argc, const char** argv) {
  const auto request = parse_command_line(argc, argv);
  const auto config = zssh::config::load_config("./zssh.yaml");
  (void)config;
  (void)request;
  return 0;
}

}  // namespace zssh::cli
```

```json
// docs/schemas/exec-result.schema.json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "zssh exec result",
  "type": "object",
  "required": ["command", "profile", "exit_code", "stdout", "stderr"],
  "properties": {
    "command": {"type": "string"},
    "profile": {"type": "string"},
    "exit_code": {"type": "integer"},
    "stdout": {"type": "string"},
    "stderr": {"type": "string"}
  }
}
```

- [ ] **Step 4: Run the session-manager and CLI integration tests**

Run: `cmake --build build && ctest --test-dir build -R "SessionManagerExecTest|ExecJsonCliTest" --output-on-failure`
Expected: PASS with a fake-backed `exec --json` vertical slice and a frozen JSON output schema.

### Task 5: Replace the fake transport with a real `libssh` adapter for `exec`

**Files:**
- Create: `src/zssh/protocol/LibsshError.hpp`
- Create: `src/zssh/protocol/LibsshAdapter.hpp`
- Create: `src/zssh/protocol/LibsshAdapter.cpp`
- Modify: `src/zssh/cli/CommandDispatcher.cpp`
- Create: `tests/integration/fixtures/known_hosts`
- Create: `tests/integration/ExecAgainstOpenSshTest.cpp`

- [ ] **Step 1: Write a failing integration test against a disposable OpenSSH target**

```cpp
// tests/integration/ExecAgainstOpenSshTest.cpp
#include <gtest/gtest.h>

int run_main(int argc, const char** argv);

TEST(ExecAgainstOpenSshTest, ExecCommandRunsAgainstRealServer) {
  const char* argv[] = {"zssh", "exec", "integration", "printf", "ok", "--json"};
  EXPECT_EQ(run_main(6, argv), 0);
}
```

- [ ] **Step 2: Run the integration test to verify it fails**

Run: `ctest --test-dir build -R ExecAgainstOpenSshTest --output-on-failure`
Expected: FAIL because no real `libssh` adapter is wired into the dispatcher yet.

- [ ] **Step 3: Implement the real adapter and error mapping**

```cpp
// src/zssh/protocol/LibsshAdapter.hpp
#pragma once

#include <libssh/libssh.h>

#include "zssh/protocol/ISshAdapter.hpp"

namespace zssh::protocol {

class LibsshAdapter final : public ISshAdapter {
 public:
  LibsshAdapter();
  ~LibsshAdapter() override;

  void connect(const std::string& profile_name) override;
  ExecResponse exec(const std::string& remote_command) override;

 private:
  ssh_session session_{nullptr};
};

}  // namespace zssh::protocol
```

```cpp
// src/zssh/protocol/LibsshError.hpp
#pragma once

#include <stdexcept>
#include <string>

namespace zssh::protocol {

class LibsshError : public std::runtime_error {
 public:
  explicit LibsshError(const std::string& message) : std::runtime_error(message) {}
};

}  // namespace zssh::protocol
```

```cpp
// src/zssh/protocol/LibsshAdapter.cpp
#include "zssh/protocol/LibsshAdapter.hpp"

#include <libssh/callbacks.h>
#include <libssh/libssh.h>

#include "zssh/protocol/LibsshError.hpp"

namespace zssh::protocol {

LibsshAdapter::LibsshAdapter() : session_(ssh_new()) {
  if (session_ == nullptr) {
    throw LibsshError("ssh_new failed");
  }
}

LibsshAdapter::~LibsshAdapter() {
  if (session_ != nullptr) {
    ssh_free(session_);
  }
}

}  // namespace zssh::protocol
```

- [ ] **Step 4: Run unit tests, then run the real-server integration test**

Run: `cmake --build build && ctest --test-dir build -R "SessionManagerExecTest|ExecJsonCliTest|ExecAgainstOpenSshTest" --output-on-failure`
Expected: PASS for unit tests, and PASS for the OpenSSH integration test when the disposable server fixture is available.

### Task 6: Implement the renderer, theme, and interactive `connect` flow

**Files:**
- Create: `src/zssh/renderer/CatppuccinMochaTheme.hpp`
- Create: `src/zssh/renderer/AnsiTerminalRenderer.hpp`
- Create: `src/zssh/renderer/AnsiTerminalRenderer.cpp`
- Modify: `src/zssh/session/SessionManager.hpp`
- Modify: `src/zssh/session/SessionManager.cpp`
- Create: `tests/unit/renderer/AnsiTerminalRendererTest.cpp`
- Create: `tests/integration/ConnectInteractiveSmokeTest.cpp`

- [ ] **Step 1: Write failing tests for theme application and interactive connect**

```cpp
// tests/unit/renderer/AnsiTerminalRendererTest.cpp
#include <gtest/gtest.h>
#include "zssh/renderer/AnsiTerminalRenderer.hpp"

TEST(AnsiTerminalRendererTest, AppliesCatppuccinMochaAccentColor) {
  zssh::renderer::AnsiTerminalRenderer renderer;
  EXPECT_NE(renderer.render_status_line("connected").find("38;2;137;180;250"), std::string::npos);
}
```

```cpp
// tests/integration/ConnectInteractiveSmokeTest.cpp
#include <gtest/gtest.h>

int run_main(int argc, const char** argv);

TEST(ConnectInteractiveSmokeTest, ConnectCommandStartsInteractiveSession) {
  const char* argv[] = {"zssh", "connect", "integration"};
  EXPECT_EQ(run_main(3, argv), 0);
}
```

- [ ] **Step 2: Run the focused tests to verify they fail**

Run: `ctest --test-dir build -R "AnsiTerminalRendererTest|ConnectInteractiveSmokeTest" --output-on-failure`
Expected: FAIL because the renderer and interactive session path do not exist yet.

- [ ] **Step 3: Implement the renderer and connect orchestration**

```cpp
// src/zssh/renderer/CatppuccinMochaTheme.hpp
#pragma once

namespace zssh::renderer {

struct CatppuccinMochaTheme {
  static constexpr const char* kBlueAccent = "38;2;137;180;250";
  static constexpr const char* kGreenAccent = "38;2;166;227;161";
};

}  // namespace zssh::renderer
```

```cpp
// src/zssh/renderer/AnsiTerminalRenderer.hpp
#pragma once

#include <string>

#include "zssh/renderer/ITerminalRenderer.hpp"

namespace zssh::renderer {

class AnsiTerminalRenderer final : public ITerminalRenderer {
 public:
  std::string render_status_line(const std::string& text) override;
  void write_remote_output(const std::string& chunk) override;
};

}  // namespace zssh::renderer
```

- [ ] **Step 4: Run renderer and connect smoke tests**

Run: `cmake --build build && ctest --test-dir build -R "AnsiTerminalRendererTest|ConnectInteractiveSmokeTest" --output-on-failure`
Expected: PASS with themed status rendering and a basic interactive connect control flow.

### Task 7: Finish `config`, `keygen`, `copy`, and `proxy` commands on shared orchestration, with reusable copy runtime

**Files:**
- Modify: `src/zssh/core/CommandModel.hpp`
- Modify: `src/zssh/cli/CliParser.cpp`
- Modify: `src/zssh/cli/CommandDispatcher.cpp`
- Modify: `src/zssh/session/SessionManager.cpp`
- Modify: `src/zssh/protocol/ISshAdapter.hpp`
- Modify: `src/zssh/protocol/LibsshAdapter.cpp`
- Create: `src/zssh/copy/CopyRequest.hpp`
- Create: `src/zssh/copy/CopyService.hpp`
- Create: `src/zssh/copy/CopyService.cpp`
- Create: `tests/integration/CopyProxyWorkflowTest.cpp`
- Create: `tests/unit/cli/AdditionalCommandParsingTest.cpp`

- [ ] **Step 1: Write failing tests for the remaining command surface**

```cpp
// tests/unit/cli/AdditionalCommandParsingTest.cpp
#include <gtest/gtest.h>
#include "zssh/cli/CliParser.hpp"

TEST(AdditionalCommandParsingTest, ParsesCopyCommand) {
  const char* argv[] = {"zssh", "copy", "prod", "./local.txt", "/tmp/local.txt"};
  const auto request = zssh::cli::parse_command_line(5, argv);
  EXPECT_EQ(request.kind, zssh::core::CommandKind::Copy);
}

TEST(AdditionalCommandParsingTest, ParsesKeygenCommand) {
  const char* argv[] = {"zssh", "keygen", "--output", "./id_ed25519"};
  const auto request = zssh::cli::parse_command_line(4, argv);
  EXPECT_EQ(request.kind, zssh::core::CommandKind::Keygen);
}

TEST(AdditionalCommandParsingTest, ParsesConfigCommand) {
  const char* argv[] = {"zssh", "config", "show", "prod", "--json"};
  const auto request = zssh::cli::parse_command_line(5, argv);
  EXPECT_EQ(request.kind, zssh::core::CommandKind::Config);
  EXPECT_TRUE(request.json_output);
}
```

```cpp
// tests/integration/CopyProxyWorkflowTest.cpp
#include <gtest/gtest.h>

int run_main(int argc, const char** argv);

TEST(CopyProxyWorkflowTest, CopyAndProxyCommandsReturnSuccess) {
  const char* copy_argv[] = {"zssh", "copy", "integration", "./README.md", "/tmp/README.md"};
  const char* proxy_argv[] = {"zssh", "proxy", "integration", "--local-port", "9000", "--remote-port", "9001"};
  const char* keygen_argv[] = {"zssh", "keygen", "--output", "./id_ed25519"};
  const char* config_argv[] = {"zssh", "config", "show", "integration", "--json"};

  EXPECT_EQ(run_main(5, copy_argv), 0);
  EXPECT_EQ(run_main(7, proxy_argv), 0);
  EXPECT_EQ(run_main(4, keygen_argv), 0);
  EXPECT_EQ(run_main(5, config_argv), 0);
}
```

- [ ] **Step 2: Run the focused command tests to verify they fail**

Run: `ctest --test-dir build -R "AdditionalCommandParsingTest|CopyProxyWorkflowTest" --output-on-failure`
Expected: FAIL because the parser and session layer do not yet handle the remaining commands.

- [ ] **Step 3: Extend the request model, parser, dispatcher, and adapter methods**

```cpp
// src/zssh/copy/CopyRequest.hpp
#pragma once

#include <string>

namespace zssh::copy {

enum class CopyDirection { LocalToRemote, RemoteToLocal };

struct CopyRequest {
  std::string profile_name;
  std::string source_path;
  std::string destination_path;
  CopyDirection direction;
};

}  // namespace zssh::copy
```

```cpp
// src/zssh/copy/CopyService.hpp
#pragma once

#include "zssh/copy/CopyRequest.hpp"
#include "zssh/protocol/ISshAdapter.hpp"

namespace zssh::copy {

class CopyService {
 public:
  explicit CopyService(zssh::protocol::ISshAdapter& ssh) : ssh_(ssh) {}

  void run(const CopyRequest& request);

 private:
  zssh::protocol::ISshAdapter& ssh_;
};

}  // namespace zssh::copy
```

```cpp
// src/zssh/copy/CopyService.cpp
#include "zssh/copy/CopyService.hpp"

namespace zssh::copy {

void CopyService::run(const CopyRequest& request) {
  ssh_.connect(request.profile_name);
  ssh_.copy_file(request.source_path, request.destination_path);
}

}  // namespace zssh::copy
```

```cpp
// src/zssh/protocol/ISshAdapter.hpp
class ISshAdapter {
 public:
  virtual ~ISshAdapter() = default;
  virtual void connect(const std::string& profile_name) = 0;
  virtual ExecResponse exec(const std::string& remote_command) = 0;
  virtual void start_interactive_shell() = 0;
  virtual void copy_file(const std::string& local_path, const std::string& remote_path) = 0;
  virtual void start_port_forward(std::uint16_t local_port, std::uint16_t remote_port) = 0;
  virtual void generate_keypair(const std::string& output_path) = 0;
};
```

```cpp
// src/zssh/core/CommandModel.hpp
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
```

The `copy` command must delegate through `CopyService`, not call `copy_file(...)` directly from the CLI. This keeps the transfer runtime reusable for a future `zazaki_scp` binary.

- [ ] **Step 4: Run the remaining command tests and the full suite**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS for the expanded CLI surface and the command workflows that share one session authority.

## Future derived frontend

If `zazaki_scp` becomes in-scope after Task 7, implement it as a thin CLI binary that:

- parses `scp`-style argv into `zssh::copy::CopyRequest`
- reuses the same configuration/profile/authentication/runtime stack
- links the existing copy runtime instead of re-implementing SSH transport behavior

This follow-on is intentionally deferred until the shared copy runtime exists.

### Task 8: Add Linux and Windows concrete platform implementations and finish verification

**Files:**
- Create: `src/zssh/platform/linux/LinuxPlatformServices.hpp`
- Create: `src/zssh/platform/linux/LinuxPlatformServices.cpp`
- Create: `src/zssh/platform/windows/WindowsPlatformServices.hpp`
- Create: `src/zssh/platform/windows/WindowsPlatformServices.cpp`
- Modify: `CMakeLists.txt`
- Modify: `.github/workflows/ci.yml`
- Create: `docs/superpowers/specs/verification-matrix.md`

- [ ] **Step 1: Write failing tests or compile guards for the concrete backends**

```cpp
// tests/unit/platform/PlatformServicesContractTest.cpp
#if defined(_WIN32)
TEST(PlatformServicesContractTest, WindowsPlatformBuildsConptyBackend) {
  zssh::platform::WindowsPlatformServices platform;
  SUCCEED();
}
#else
TEST(PlatformServicesContractTest, LinuxPlatformBuildsEpollBackend) {
  zssh::platform::LinuxPlatformServices platform;
  SUCCEED();
}
#endif
```

- [ ] **Step 2: Run the platform contract tests to verify they fail**

Run: `cmake --build build && ctest --test-dir build -R PlatformServicesContractTest --output-on-failure`
Expected: FAIL because the concrete platform classes do not exist yet.

- [ ] **Step 3: Implement the concrete Linux and Windows services and wire them into the dispatcher**

```cpp
// src/zssh/platform/linux/LinuxPlatformServices.hpp
#pragma once

#include "zssh/platform/IPlatformServices.hpp"

namespace zssh::platform {

class LinuxPlatformServices final : public IPlatformServices {
 public:
  void create_console() override;
  void pump_events_once() override;
};

}  // namespace zssh::platform
```

```cpp
// src/zssh/platform/windows/WindowsPlatformServices.hpp
#pragma once

#include "zssh/platform/IPlatformServices.hpp"

namespace zssh::platform {

class WindowsPlatformServices final : public IPlatformServices {
 public:
  void create_console() override;
  void pump_events_once() override;
};

}  // namespace zssh::platform
```

```cmake
# CMakeLists.txt
if(WIN32)
  target_sources(zssh PRIVATE src/zssh/platform/windows/WindowsPlatformServices.cpp)
else()
  target_sources(zssh PRIVATE src/zssh/platform/linux/LinuxPlatformServices.cpp)
endif()
```

- [ ] **Step 4: Run the final verification matrix**

Run: `cmake -S . -B build -DZSSH_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure`
Expected: PASS on the local host OS.

Run: `gh workflow run ci.yml` or push the branch to trigger GitHub Actions once the repo is connected to GitHub.
Expected: Linux and Windows CI jobs both configure, build, and execute the unit/integration tests allowed on each OS.

## Verification matrix

- Unit: `ctest --test-dir build -R "CliParserTest|ConfigLoaderTest|BackpressureControllerTest|SessionManagerExecTest|AnsiTerminalRendererTest|PlatformServicesContractTest" --output-on-failure`
- Integration: `ctest --test-dir build -R "ExecJsonCliTest|ExecAgainstOpenSshTest|ConnectInteractiveSmokeTest|CopyProxyWorkflowTest" --output-on-failure`
- Full local sweep: `ctest --test-dir build --output-on-failure`
- CI sweep: Linux + Windows workflow in `.github/workflows/ci.yml`

## Spec coverage map

- Goals and command surface: Tasks 1, 2, 4, 6, 7
- YAML config and authentication model: Task 2, then real transport work in Task 5
- Session manager authority: Tasks 3, 4, 6, 7
- Backpressure handling: Task 3, then exercised in Tasks 4 and 6
- AI-friendly `--json` mode: Task 4 with schema and integration tests
- Catppuccin Mocha renderer: Task 6
- Linux and Windows platform abstraction: Tasks 3 and 8
- Project skeleton, CI, and verification: Tasks 1 and 8

## Risks to watch during execution

- Keep `libssh` isolated to `LibsshAdapter`; do not leak `ssh_session` or channel types into session or CLI code.
- Do not let interactive rendering become the implicit truth for `--json`; lifecycle/result data must remain structured.
- Keep Linux `epoll` and Windows `IOCP` / `ConPTY` behind `IPlatformServices`; avoid `#ifdef` spread into unrelated layers.
- Treat backpressure thresholds as testable contract values, not debug-only heuristics.

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-07-03-zssh-implementation-plan.md`.

Two execution options:

1. Subagent-Driven (recommended) - I dispatch a fresh subagent per task, review between tasks, fast iteration
2. Inline Execution - Execute tasks in this session using executing-plans, batch execution with checkpoints
