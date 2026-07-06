#include <gtest/gtest.h>

#include "tests/support/FakeSshAdapter.hpp"
#include "zssh/session/SessionManager.hpp"

TEST(SessionManagerExecTest, ExecCommandReturnsExitCodeAndStdout) {
  zssh::tests::FakeSshAdapter ssh;
  ssh.set_exec_result(0, "Linux test-host 6.9.0", "");

  zssh::session::SessionManager manager(ssh);
  const zssh::protocol::SshConnectionConfig connection{
      .profile_name = "prod",
      .host = "prod.example.com",
      .port = 2222,
      .auth = {
          .method = "publickey",
          .username = "deploy",
          .password_command = "",
          .key_path = "~/.ssh/id_ed25519",
          .use_agent = true,
      },
  };

  const auto result = manager.exec(connection, "uname -a");

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, "Linux test-host 6.9.0");
  EXPECT_EQ(result.stderr_text, "");
  EXPECT_EQ(ssh.connect_calls, 1);
  EXPECT_EQ(ssh.exec_calls, 1);
  EXPECT_EQ(ssh.connected_connection.profile_name, "prod");
  EXPECT_EQ(ssh.connected_connection.host, "prod.example.com");
  EXPECT_EQ(ssh.connected_connection.port, 2222);
  EXPECT_EQ(ssh.connected_connection.auth.method, "publickey");
  EXPECT_EQ(ssh.connected_connection.auth.username, "deploy");
  EXPECT_EQ(ssh.connected_connection.auth.key_path, "~/.ssh/id_ed25519");
  EXPECT_EQ(ssh.last_remote_command, "uname -a");
}
