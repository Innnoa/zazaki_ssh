#pragma once

#include <string>

namespace zssh::renderer {

class ITerminalRenderer {
 public:
  virtual ~ITerminalRenderer() = default;

  virtual std::string render_status_line(const std::string& text) = 0;
  virtual void write_stdout(const std::string& chunk) = 0;
  virtual void write_stderr(const std::string& chunk) = 0;
};

}  // namespace zssh::renderer
