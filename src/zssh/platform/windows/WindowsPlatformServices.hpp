#pragma once

#include "zssh/platform/IPlatformServices.hpp"

namespace zssh::platform {

class WindowsPlatformServices final : public IPlatformServices {
 public:
  void create_console() override;
  void pump_events_once() override;
};

}  // namespace zssh::platform
