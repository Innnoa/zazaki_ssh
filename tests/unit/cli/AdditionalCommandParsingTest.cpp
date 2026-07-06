#include <gtest/gtest.h>

#include "zssh/cli/CliParser.hpp"
#include "zssh/core/Error.hpp"

TEST(AdditionalCommandParsingTest, ParsesCopyCommand) {
  const char* argv[] = {"zssh", "copy", "prod", "./local.txt", "/tmp/local.txt"};
  const auto request = zssh::cli::parse_command_line(5, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Copy);
  EXPECT_EQ(request.source_path, "./local.txt");
  EXPECT_EQ(request.destination_path, "/tmp/local.txt");
}

TEST(AdditionalCommandParsingTest, ParsesKeygenCommand) {
  const char* argv[] = {"zssh", "keygen", "prod", "--output", "./id_ed25519"};
  const auto request = zssh::cli::parse_command_line(4, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Keygen);
}

TEST(AdditionalCommandParsingTest, ParsesConfigCommand) {
  const char* argv[] = {"zssh", "config", "show", "prod", "--json"};
  const auto request = zssh::cli::parse_command_line(5, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Config);
  EXPECT_TRUE(request.json_output);
}

TEST(AdditionalCommandParsingTest, ParsesProxyCommand) {
  const char* argv[] = {"zssh", "proxy", "prod", "--local-port", "9000", "--remote-port", "9001"};
  const auto request = zssh::cli::parse_command_line(7, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Proxy);
  EXPECT_EQ(request.local_port, 9000);
  EXPECT_EQ(request.remote_port, 9001);
}

TEST(AdditionalCommandParsingTest, RejectsCopyWithoutSource) {
  const char* argv[] = {"zssh", "copy", "prod"};
  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(3, argv)),
      zssh::core::Error);
}

TEST(AdditionalCommandParsingTest, RejectsProxyWithoutLocalPort) {
  const char* argv[] = {"zssh", "proxy", "prod"};
  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(3, argv)),
      zssh::core::Error);
}
