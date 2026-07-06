#include <gtest/gtest.h>

int run_main(int argc, const char** argv);

TEST(StartupSmokeTest, HelpCommandReturnsSuccess) {
  const char* argv[] = {"zssh", "--help"};
  EXPECT_EQ(run_main(2, argv), 0);
}
