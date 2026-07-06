#include "zssh/copy/SftpTransfer.hpp"

#include <algorithm>

namespace zssh::copy {

SftpTransfer::SftpTransfer(zssh::protocol::ISshAdapter& ssh) : ssh_(ssh) {}

bool SftpTransfer::upload_file(const std::string& local_path,
                                const std::string& remote_path,
                                const char* data, std::uint64_t size) {
  auto handle = ssh_.sftp_open_write(remote_path, 0);

  std::uint64_t offset = 0;
  while (offset < size) {
    std::uint64_t chunk = std::min(kChunkSize, size - offset);
    std::uint64_t written = ssh_.sftp_write(handle, data + offset, chunk);
    if (written == 0) {
      ssh_.sftp_close(handle);
      return false;
    }
    offset += written;
  }

  ssh_.sftp_close(handle);
  return true;
}

std::uint64_t SftpTransfer::download_file(const std::string& remote_path,
                                           char* buffer, std::uint64_t buffer_size) {
  auto handle = ssh_.sftp_open_read(remote_path, 0);

  std::uint64_t total = 0;
  while (total < buffer_size) {
    std::uint64_t chunk = std::min(kChunkSize, buffer_size - total);
    std::uint64_t read = ssh_.sftp_read(handle, buffer + total, chunk);
    if (read == 0) break;
    total += read;
  }

  ssh_.sftp_close(handle);
  return total;
}

}  // namespace zssh::copy
