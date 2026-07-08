cmake_minimum_required(VERSION 3.24)
project(zssh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZSSH_BUILD_TESTS "Build zssh tests" ON)

include(cmake/Warnings.cmake)
include(cmake/Dependencies.cmake)

add_library(zssh_warnings INTERFACE)
zssh_apply_warnings(zssh_warnings)

zssh_require_yaml_cpp()
zssh_require_libssh()

add_library(
  zssh_protocol_libssh
  src/zssh/protocol/LibsshAdapter.cpp
)
target_include_directories(zssh_protocol_libssh PUBLIC src)
target_link_libraries(zssh_protocol_libssh PUBLIC zssh_warnings libssh::libssh)

add_library(
  zssh_core
  src/zssh/config/ConfigLoader.cpp
  src/zssh/cli/CliParser.cpp
  src/zssh/cli/CommandDispatcher.cpp
  src/zssh/session/BackpressureController.cpp
  src/zssh/session/SessionManager.cpp
  src/zssh/renderer/AnsiTerminalRenderer.cpp
)
target_include_directories(zssh_core PUBLIC src)
target_link_libraries(zssh_core PUBLIC zssh_warnings PRIVATE yaml-cpp::yaml-cpp)

add_library(
  zssh_cli_runtime_defaults
  src/zssh/cli/DefaultCommandRuntime.cpp
)
target_include_directories(zssh_cli_runtime_defaults PUBLIC src)
target_link_libraries(zssh_cli_runtime_defaults PUBLIC zssh_core zssh_protocol_libssh zssh_platform)

add_executable(zssh src/main.cpp)
target_link_libraries(zssh PRIVATE zssh_core zssh_cli_runtime_defaults)

add_library(
  zssh_platform
)
target_include_directories(zssh_platform PUBLIC src)
target_link_libraries(zssh_platform PUBLIC zssh_warnings)
if(WIN32)
  target_sources(zssh_platform PRIVATE src/zssh/platform/windows/WindowsPlatformServices.cpp)
else()
  target_sources(zssh_platform PRIVATE src/zssh/platform/linux/LinuxPlatformServices.cpp)
endif()

add_library(
  zssh_copy
  src/zssh/copy/CopyRequest.hpp
  src/zssh/copy/IProgressSink.hpp
  src/zssh/copy/RateLimiter.hpp
  src/zssh/copy/RateLimiter.cpp
  src/zssh/copy/TransferPlanner.hpp
  src/zssh/copy/TransferPlanner.cpp
  src/zssh/copy/SftpTransfer.hpp
  src/zssh/copy/SftpTransfer.cpp
  src/zssh/copy/CopyService.hpp
  src/zssh/copy/CopyService.cpp
)
target_include_directories(zssh_copy PUBLIC src)
target_link_libraries(zssh_copy PUBLIC zssh_warnings)

if(ZSSH_BUILD_TESTS)
  zssh_require_nlohmann_json()
  enable_testing()
  zssh_require_googletest()
  add_executable(
    zssh_unit_tests
    tests/unit/core/StartupSmokeTest.cpp
    tests/unit/cli/CliParserTest.cpp
    tests/unit/cli/AdditionalCommandParsingTest.cpp
    tests/unit/config/ConfigLoaderTest.cpp
    tests/unit/session/BackpressureControllerTest.cpp
    tests/unit/session/SessionManagerExecTest.cpp
    tests/unit/session/SessionManagerLargeOutputTest.cpp
    tests/unit/platform/PlatformServicesContractTest.cpp
    tests/unit/renderer/AnsiTerminalRendererTest.cpp
    tests/integration/ExecJsonCliTest.cpp
    tests/integration/ConnectInteractiveSmokeTest.cpp
    src/main.cpp
  )
  target_compile_definitions(zssh_unit_tests PRIVATE ZSSH_TEST_BUILD)
  target_include_directories(zssh_unit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(zssh_unit_tests PRIVATE GTest::gtest_main zssh_core zssh_cli_runtime_defaults nlohmann_json::nlohmann_json)
  include(GoogleTest)
  gtest_discover_tests(zssh_unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  add_executable(
    zssh_copy_tests
    tests/unit/copy/RateLimiterTest.cpp
    tests/unit/copy/TransferPlannerTest.cpp
    tests/unit/copy/SftpTransferTest.cpp
    tests/unit/copy/CopyServiceTest.cpp
  )
  target_include_directories(zssh_copy_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(zssh_copy_tests PRIVATE GTest::gtest_main zssh_copy)
  gtest_discover_tests(zssh_copy_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()
