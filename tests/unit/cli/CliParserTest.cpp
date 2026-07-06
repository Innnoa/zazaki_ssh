#include <gtest/gtest.h>

#include "zssh/cli/CliParser.hpp"
#include "zssh/core/Error.hpp"

TEST(CliParserTest, ParsesExecJsonCommand) {
  const char* argv[] = {"zssh", "exec", "prod", "uname", "-a", "--json"};
  const auto request = zssh::cli::parse_command_line(6, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Exec);
  EXPECT_EQ(request.profile_name, "prod");
  EXPECT_EQ(request.remote_command, "uname -a");
  ASSERT_EQ(request.positional_args.size(), 2U);
  EXPECT_EQ(request.positional_args[0], "uname");
  EXPECT_EQ(request.positional_args[1], "-a");
  EXPECT_TRUE(request.json_output);
}

TEST(CliParserTest, PreservesJsonTokenAfterDelimiterInPayload) {
  const char* argv[] = {"zssh", "exec", "prod", "--", "printf", "--json"};
  const auto request = zssh::cli::parse_command_line(6, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Exec);
  EXPECT_EQ(request.profile_name, "prod");
  EXPECT_EQ(request.remote_command, "printf --json");
  ASSERT_EQ(request.positional_args.size(), 2U);
  EXPECT_EQ(request.positional_args[0], "printf");
  EXPECT_EQ(request.positional_args[1], "--json");
  EXPECT_FALSE(request.json_output);
}

TEST(CliParserTest, JoinsPayloadTokenContainingSpacesForRawShellSemantics) {
  const char* argv[] = {"zssh", "exec", "prod", "--", "printf", "two words"};
  const auto request = zssh::cli::parse_command_line(6, argv);

  EXPECT_EQ(request.remote_command, "printf two words");
  ASSERT_EQ(request.positional_args.size(), 2U);
  EXPECT_EQ(request.positional_args[0], "printf");
  EXPECT_EQ(request.positional_args[1], "two words");
}

TEST(CliParserTest, PreservesShellSemanticsForShDashCPayload) {
  const char* argv[] = {
      "zssh",
      "exec",
      "prod",
      "--",
      "sh",
      "-c",
      "ls *.log | wc -l",
  };
  const auto request = zssh::cli::parse_command_line(7, argv);

  EXPECT_EQ(request.remote_command, "sh -c ls *.log | wc -l");
  ASSERT_EQ(request.positional_args.size(), 3U);
  EXPECT_EQ(request.positional_args[0], "sh");
  EXPECT_EQ(request.positional_args[1], "-c");
  EXPECT_EQ(request.positional_args[2], "ls *.log | wc -l");
}

TEST(CliParserTest, RejectsExecWithoutPayloadAfterFlags) {
  const char* argv[] = {"zssh", "exec", "prod", "--json"};

  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(4, argv)),
      zssh::core::Error);
}

TEST(CliParserTest, RejectsWhitespaceOnlyExecPayload) {
  const char* argv[] = {"zssh", "exec", "prod", "--", "   "};

  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(5, argv)),
      zssh::core::Error);
}

TEST(CliParserTest, RejectsConnectWithPayloadTokens) {
  const char* argv[] = {"zssh", "connect", "prod", "unexpected"};

  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(4, argv)),
      zssh::core::Error);
}

TEST(CliParserTest, RejectsConnectWithJsonFlag) {
  const char* argv[] = {"zssh", "connect", "prod", "--json"};

  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(4, argv)),
      zssh::core::Error);
}

TEST(CliParserTest, RejectsUnknownVerb) {
  const char* argv[] = {"zssh", "typo", "prod"};

  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(3, argv)),
      zssh::core::Error);
}

TEST(CliParserTest, RejectsBlankProfileName) {
  const char* argv[] = {"zssh", "exec", "   ", "uname"};

  EXPECT_THROW(
      static_cast<void>(zssh::cli::parse_command_line(4, argv)),
      zssh::core::Error);
}

TEST(CliParserTest, AcceptsMutableArgvShapeFromMainStyleCallSite) {
  char arg0[] = "zssh";
  char arg1[] = "exec";
  char arg2[] = "prod";
  char arg3[] = "uname";
  char* argv[] = {arg0, arg1, arg2, arg3};

  const auto request = zssh::cli::parse_command_line(4, argv);

  EXPECT_EQ(request.kind, zssh::core::CommandKind::Exec);
  EXPECT_EQ(request.profile_name, "prod");
  EXPECT_EQ(request.remote_command, "uname");
}
