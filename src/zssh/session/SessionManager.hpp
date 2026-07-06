#pragma once

#include <string>

#include "zssh/core/CommandModel.hpp"
#include "zssh/protocol/ISshAdapter.hpp"

namespace zssh::session {

class SessionManager {
 public:
  explicit SessionManager(zssh::protocol::ISshAdapter& ssh)
      : ssh_(ssh) {}

  zssh::core::CommandResult exec(const zssh::protocol::SshConnectionConfig& connection,
                                 const std::string& remote_command,
                                 const zssh::protocol::ExecCallbacks& callbacks = {});

  zssh::core::CommandResult connect(const zssh::protocol::SshConnectionConfig& connection);

 private:
  zssh::protocol::ISshAdapter& ssh_;
};

}  // namespace zssh::session
