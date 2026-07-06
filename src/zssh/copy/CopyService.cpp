#include "zssh/copy/CopyService.hpp"

#include <chrono>
#include <cstring>
#include <vector>

namespace zssh::copy {

CopyService::CopyService(zssh::protocol::ISshAdapter& ssh)
    : ssh_(ssh), transfer_(ssh) {}

bool CopyService::run(const CopyRequest& request, IProgressSink& sink,
                      TransferPlanner::LocalStatFn local_stat,
                      LocalReadFn local_read,
                      LocalWriteFn local_write) {
  auto start_time = std::chrono::steady_clock::now();

  auto plan = TransferPlanner::plan(request, local_stat,
    [this](const std::string& path) -> FileInfo {
      auto attr = ssh_.sftp_stat(path);
      return FileInfo{path, attr.size, attr.is_directory};
    });

  TransferSummary summary;
  summary.total_files = plan.transfers.size();

  std::vector<char> buffer(65536);

  for (auto& item : plan.transfers) {
    TransferProgress progress;
    progress.file_path = item.local_path;
    progress.total_bytes = item.total_bytes;
    progress.state = TransferState::Transferring;
    sink.on_progress(progress);

    bool file_ok = false;

    if (request.direction == CopyDirection::LocalToRemote) {
      std::uint64_t offset = item.resume_offset;
      std::uint64_t remaining = item.total_bytes - offset;

      auto handle = ssh_.sftp_open_write(item.remote_path, offset);

      while (remaining > 0) {
        std::uint64_t chunk = std::min(static_cast<std::uint64_t>(buffer.size()), remaining);
        std::uint64_t read = local_read(item.local_path, buffer.data(), chunk);
        if (read == 0) break;

        std::uint64_t written = ssh_.sftp_write(handle, buffer.data(), read);
        if (written == 0) {
          break;
        }

        offset += written;
        remaining -= written;
        progress.bytes_transferred = offset;
        sink.on_progress(progress);
      }

      ssh_.sftp_close(handle);
      file_ok = (remaining == 0);
    } else {
      auto handle = ssh_.sftp_open_read(item.remote_path, item.resume_offset);
      std::uint64_t offset = 0;

      while (true) {
        std::uint64_t chunk = std::min(static_cast<std::uint64_t>(buffer.size()),
                                       item.total_bytes - offset);
        if (chunk == 0) break;

        std::uint64_t read_bytes = ssh_.sftp_read(handle, buffer.data(), chunk);
        if (read_bytes == 0) break;

        if (local_write) {
          std::uint64_t written = local_write(item.local_path, buffer.data(), read_bytes);
          if (written == 0) break;
        }

        offset += read_bytes;
        progress.bytes_transferred = offset;
        sink.on_progress(progress);
      }

      ssh_.sftp_close(handle);
      file_ok = (offset > 0);
    }

    summary.total_bytes += progress.bytes_transferred;
    sink.on_file_complete(item.local_path, file_ok);
    if (file_ok) {
      summary.succeeded++;
    } else {
      summary.failed++;
    }
  }

  auto end_time = std::chrono::steady_clock::now();
  summary.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();

  sink.on_transfer_complete(summary);
  return summary.failed == 0;
}

}  // namespace zssh::copy
