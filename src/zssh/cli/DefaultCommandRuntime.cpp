#include "zssh/cli/CommandDispatcher.hpp"

#include <memory>

#include "zssh/platform/linux/LinuxPlatformServices.hpp"
#include "zssh/protocol/LibsshAdapter.hpp"
#include "zssh/renderer/AnsiTerminalRenderer.hpp"

namespace zssh::cli::internal {
namespace {

}  // namespace

CommandRuntime make_default_exec_runtime() {
  return {
      std::make_unique<zssh::protocol::LibsshAdapter>(),
      std::make_unique<zssh::platform::LinuxPlatformServices>(),
      std::make_unique<zssh::renderer::AnsiTerminalRenderer>(),
  };
}

CommandRuntimeFactory make_default_exec_runtime_factory() {
  return [] {
    return make_default_exec_runtime();
  };
}

}  // namespace zssh::cli::internal
