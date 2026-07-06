#pragma once

namespace zssh::platform {

class IPlatformServices {
 public:
  virtual ~IPlatformServices() = default;

  virtual void create_console() = 0;
  virtual void pump_events_once() = 0;
};

}  // namespace zssh::platform
