#pragma once

#include <filesystem>

#include "zssh/config/AppConfig.hpp"

namespace zssh::config {

AppConfig load_config(const std::filesystem::path& path);

}  // namespace zssh::config
