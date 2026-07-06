#pragma once

#include "zssh/core/CommandModel.hpp"

namespace zssh::cli {

zssh::core::CommandRequest parse_command_line(int argc, const char* const* argv);
zssh::core::CommandRequest parse_command_line(int argc, char* const* argv);

}  // namespace zssh::cli
