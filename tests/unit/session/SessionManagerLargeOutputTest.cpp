#include <gtest/gtest.h>

#include "tests/support/FakeSshAdapter.hpp"
#include "zssh/session/SessionManager.hpp"

TEST(SessionManagerExecTest, HandlesLargeStdoutWithoutHanging) {
  std::string large_output(256 * 1024, 'X');
  for (std::size_t i = 0; i < large_output.size(); ++i) {
    large_output[i] = static_cast<char>('A' + (i % 26));
  }

  zssh::tests::FakeSshAdapter ssh;
  ssh.set_exec_result(0, large_output, "");

  zssh::session::SessionManager manager(ssh);
  zssh::protocol::SshConnectionConfig conn;
  conn.profile_name = "test";
  conn.host = "localhost";
  const auto result = manager.exec(conn, "cat /dev/urandom");

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_EQ(result.stdout_text, large_output);
  EXPECT_TRUE(result.stderr_text.empty());
  EXPECT_EQ(ssh.connect_calls, 1);
  EXPECT_EQ(ssh.exec_calls, 1);
}
