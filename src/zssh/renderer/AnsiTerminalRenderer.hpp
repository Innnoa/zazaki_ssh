#pragma once

#include <string>

#include "zssh/renderer/CatppuccinMochaTheme.hpp"
#include "zssh/renderer/ITerminalRenderer.hpp"

namespace zssh::renderer {

class AnsiTerminalRenderer final : public ITerminalRenderer {
 public:
  std::string render_status_line(const std::string& text) override;
  void write_stdout(const std::string& chunk) override;
  void write_stderr(const std::string& chunk) override;
};

}  // namespace zssh::renderer
