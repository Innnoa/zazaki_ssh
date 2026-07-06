#pragma once

#include <string>
#include <vector>

#include "zssh/renderer/ITerminalRenderer.hpp"

namespace zssh::tests {

class FakeTerminalRenderer : public zssh::renderer::ITerminalRenderer {
 public:
  std::string render_status_line(const std::string& text) override {
    status_lines.push_back(text);
    return text;
  }

  void write_stdout(const std::string& chunk) override {
    stdout_chunks.push_back(chunk);
  }

  void write_stderr(const std::string& chunk) override {
    stderr_chunks.push_back(chunk);
  }

  std::vector<std::string> status_lines;
  std::vector<std::string> stdout_chunks;
  std::vector<std::string> stderr_chunks;
};

}  // namespace zssh::tests
