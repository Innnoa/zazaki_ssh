#include "zssh/renderer/AnsiTerminalRenderer.hpp"

#include <iostream>
#include <string>

namespace zssh::renderer {

std::string AnsiTerminalRenderer::render_status_line(const std::string& text) {
  return "\x1b[" + std::string(CatppuccinMochaTheme::status_info()) + "m" +
         text +
         "\x1b[" + std::string(CatppuccinMochaTheme::kReset) + "m";
}

void AnsiTerminalRenderer::write_stdout(const std::string& chunk) {
  std::cout << chunk << std::flush;
}

void AnsiTerminalRenderer::write_stderr(const std::string& chunk) {
  std::cerr << chunk << std::flush;
}

}  // namespace zssh::renderer
