#include <gtest/gtest.h>

#include "zssh/renderer/AnsiTerminalRenderer.hpp"
#include "zssh/renderer/CatppuccinMochaTheme.hpp"

TEST(AnsiTerminalRendererTest, AppliesCatppuccinMochaBlueAccentToStatusLine) {
  zssh::renderer::AnsiTerminalRenderer renderer;
  const auto result = renderer.render_status_line("connected");

  EXPECT_NE(result.find("38;2;137;180;250"), std::string::npos);
  EXPECT_NE(result.find("connected"), std::string::npos);
}

TEST(AnsiTerminalRendererTest, OutputContainsAnsiResetAfterText) {
  zssh::renderer::AnsiTerminalRenderer renderer;
  const auto result = renderer.render_status_line("ok");

  EXPECT_NE(result.find("ok"), std::string::npos);
  EXPECT_NE(result.find(std::string("\x1b[0m", 4)), std::string::npos);
}

TEST(AnsiTerminalRendererTest, ThemeConstantsProvideExpectedColors) {
  EXPECT_STREQ(zssh::renderer::CatppuccinMochaTheme::kBlueAccent, "38;2;137;180;250");
  EXPECT_STREQ(zssh::renderer::CatppuccinMochaTheme::kGreenAccent, "38;2;166;227;161");
  EXPECT_STREQ(zssh::renderer::CatppuccinMochaTheme::kRedAccent, "38;2;243;139;168");
}
