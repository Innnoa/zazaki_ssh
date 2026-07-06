#include "zssh/platform/linux/LinuxPlatformServices.hpp"

#include <sys/epoll.h>

namespace zssh::platform {

void LinuxPlatformServices::create_console() {
  // On Linux, the controlling terminal is already available.
  // No explicit console creation is needed for this implementation.
}

void LinuxPlatformServices::pump_events_once() {
  epoll_event events[1];
  epoll_wait(0, events, 1, 1);
}

}  // namespace zssh::platform
