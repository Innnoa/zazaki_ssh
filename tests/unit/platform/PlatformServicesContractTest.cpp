#include <gtest/gtest.h>

#include "tests/support/FakePlatformServices.hpp"

#ifdef __linux__
#include "zssh/platform/linux/LinuxPlatformServices.hpp"
#endif
#ifdef _WIN32
#include "zssh/platform/windows/WindowsPlatformServices.hpp"
#endif

namespace {

template <typename IsConsoleCreated, typename PumpCount>
void assert_platform_services_contract(
    zssh::platform::IPlatformServices& platform,
    IsConsoleCreated is_console_created,
    PumpCount pump_count) {
  EXPECT_FALSE(is_console_created());
  EXPECT_EQ(pump_count(), 0U);

  platform.create_console();
  EXPECT_TRUE(is_console_created());

  platform.pump_events_once();
  EXPECT_EQ(pump_count(), 1U);
}

}  // namespace

TEST(PlatformServicesContractTest, FakePlatformSatisfiesPlatformServicesContract) {
  zssh::tests::PlatformContractProbe probe;
  zssh::tests::FakePlatformServices platform(&probe);

  assert_platform_services_contract(
      platform,
      [&probe]() { return probe.console_created; },
      [&probe]() { return probe.pump_count; });
}

#ifdef __linux__
TEST(PlatformServicesContractTest, LinuxPlatformBuildsAndCreatesConsole) {
  zssh::platform::LinuxPlatformServices platform;
  platform.create_console();
  platform.pump_events_once();
  SUCCEED();
}
#endif

#ifdef _WIN32
TEST(PlatformServicesContractTest, WindowsPlatformBuildsAndCreatesConsole) {
  zssh::platform::WindowsPlatformServices platform;
  platform.create_console();
  platform.pump_events_once();
  SUCCEED();
}
#endif
