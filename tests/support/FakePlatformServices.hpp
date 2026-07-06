#pragma once

#include <cstddef>

#include "zssh/platform/IPlatformServices.hpp"

namespace zssh::tests {

struct PlatformContractProbe {
  bool console_created{false};
  std::size_t pump_count{0};
};

class FakePlatformServices : public zssh::platform::IPlatformServices {
 public:
  explicit FakePlatformServices(PlatformContractProbe* probe = nullptr) : probe_(probe) {}

  void create_console() override {
    if (probe_ != nullptr) {
      probe_->console_created = true;
    }
  }

  void pump_events_once() override {
    if (probe_ != nullptr) {
      ++probe_->pump_count;
    }
  }

 private:
  PlatformContractProbe* probe_{nullptr};
};

}  // namespace zssh::tests
