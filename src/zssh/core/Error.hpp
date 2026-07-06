#pragma once

#include <stdexcept>

namespace zssh::core {

class Error : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

}  // namespace zssh::core
