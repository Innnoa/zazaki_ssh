#pragma once

#include "zssh/protocol/ISshAdapter.hpp"

#include <cstdint>
#include <string>

namespace zssh::copy {

class SftpTransfer {
 public:
  explicit SftpTransfer(zssh::protocol::ISshAdapter& ssh);

  bool upload_file(const std::string& local_path, const std::string& remote_path,
                   const char* data, std::uint64_t size);

  std::uint64_t download_file(const std::string& remote_path,
                              char* buffer, std::uint64_t buffer_size);

 private:
  zssh::protocol::ISshAdapter& ssh_;
  static constexpr std::uint64_t kChunkSize = 65536;
};

}  // namespace zssh::copy
