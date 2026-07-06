#pragma once

#include <functional>
#include <memory>

#include "zssh/platform/IPlatformServices.hpp"
#include "zssh/protocol/ISshAdapter.hpp"
#include "zssh/renderer/ITerminalRenderer.hpp"

namespace zssh::cli {

struct CommandRuntime {
  std::unique_ptr<zssh::protocol::ISshAdapter> ssh;
  std::unique_ptr<zssh::platform::IPlatformServices> platform;
  std::unique_ptr<zssh::renderer::ITerminalRenderer> renderer;
};

using CommandRuntimeFactory = std::function<CommandRuntime()>;

namespace internal {

CommandRuntime make_default_exec_runtime();
CommandRuntimeFactory make_default_exec_runtime_factory();

}  // namespace internal

int run_command(int argc, const char* const* argv);

#ifdef ZSSH_TEST_BUILD
void set_exec_runtime_factory_for_testing(CommandRuntimeFactory factory);
void reset_exec_runtime_factory_for_testing();
#endif

}  // namespace zssh::cli
