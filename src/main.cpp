#include <exception>
#include <iostream>

#include "zssh/cli/CommandDispatcher.hpp"

int run_main(int argc, const char* const* argv) {
  try {
    return zssh::cli::run_command(argc, argv);
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}

int run_main(int argc, const char** argv) {
  return run_main(argc, const_cast<const char* const*>(argv));
}

int run_main(int argc, char* const* argv) {
  return run_main(argc, const_cast<const char* const*>(argv));
}

#ifndef ZSSH_TEST_BUILD
int main(int argc, char* argv[]) {
  return run_main(argc, argv);
}
#endif
