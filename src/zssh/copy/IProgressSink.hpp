#pragma once

#include <cstdint>
#include <string>

namespace zssh::copy {

enum class TransferState { Queued, Transferring, Completed, Failed };

struct TransferProgress {
  std::string file_path;
  std::uint64_t bytes_transferred{0};
  std::uint64_t total_bytes{0};
  std::uint64_t bytes_per_second{0};
  TransferState state{TransferState::Queued};
};

struct TransferSummary {
  std::uint64_t total_files{0};
  std::uint64_t succeeded{0};
  std::uint64_t failed{0};
  std::uint64_t total_bytes{0};
  double elapsed_seconds{0.0};
};

class IProgressSink {
 public:
  virtual ~IProgressSink() = default;
  virtual void on_progress(const TransferProgress& progress) = 0;
  virtual void on_file_complete(const std::string& path, bool success) = 0;
  virtual void on_transfer_complete(const TransferSummary& summary) = 0;
};

}  // namespace zssh::copy
