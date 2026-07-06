#pragma once

namespace zssh::renderer {

struct CatppuccinMochaTheme {
  static constexpr const char* kBlueAccent = "38;2;137;180;250";
  static constexpr const char* kGreenAccent = "38;2;166;227;161";
  static constexpr const char* kRedAccent = "38;2;243;139;168";
  static constexpr const char* kReset = "0";

  static constexpr const char* status_ok() { return kGreenAccent; }
  static constexpr const char* status_info() { return kBlueAccent; }
  static constexpr const char* status_error() { return kRedAccent; }
};

}  // namespace zssh::renderer
