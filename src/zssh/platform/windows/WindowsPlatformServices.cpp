#include "zssh/platform/windows/WindowsPlatformServices.hpp"

namespace zssh::platform {

void WindowsPlatformServices::create_console() {
  // Console allocation uses ConPTY on Windows 10 1909+.
  // Full ConPTY integration is deferred to a later implementation pass.
}

void WindowsPlatformServices::pump_events_once() {
  // Event loop relies on IOCP on Windows.
  // Full IOCP integration is deferred to a later implementation pass.
}

}  // namespace zssh::platform
