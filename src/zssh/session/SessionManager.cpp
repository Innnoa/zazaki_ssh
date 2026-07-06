#include "zssh/session/SessionManager.hpp"

namespace zssh::session {

zssh::core::CommandResult SessionManager::exec(const zssh::protocol::SshConnectionConfig& connection,
                                               const std::string& remote_command,
                                               const zssh::protocol::ExecCallbacks& callbacks) {
  ssh_.connect(connection);
  zssh::core::CommandResult result;
  const bool has_callbacks = callbacks.on_stdout || callbacks.on_stderr;
  const auto response = ssh_.exec(
      remote_command,
      {
          [&result, &callbacks, has_callbacks](std::string_view chunk) {
            if (!has_callbacks) {
              result.stdout_text.append(chunk);
            }
            if (callbacks.on_stdout) {
              callbacks.on_stdout(chunk);
            }
          },
          [&result, &callbacks, has_callbacks](std::string_view chunk) {
            if (!has_callbacks) {
              result.stderr_text.append(chunk);
            }
            if (callbacks.on_stderr) {
              callbacks.on_stderr(chunk);
            }
          },
      });

  result.exit_code = response.exit_code;
  return result;
}

zssh::core::CommandResult SessionManager::connect(const zssh::protocol::SshConnectionConfig& connection) {
  ssh_.connect(connection);
  ssh_.start_interactive_shell();
  return {0, "", ""};
}

}  // namespace zssh::session
