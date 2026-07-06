#pragma once

#include "zssh/copy/CopyRequest.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace zssh::copy {

struct FileInfo {
  std::string path;
  std::uint64_t size{0};
  bool is_directory{false};
};

struct TransferItem {
  std::string local_path;
  std::string remote_path;
  std::uint64_t total_bytes{0};
  std::uint64_t resume_offset{0};
};

struct TransferPlan {
  std::vector<TransferItem> transfers;
};

class TransferPlanner {
 public:
  using LocalStatFn = std::function<FileInfo(const std::string&)>;
  using RemoteStatFn = std::function<FileInfo(const std::string&)>;

  static TransferPlan plan(const CopyRequest& request,
                           LocalStatFn local_stat,
                           RemoteStatFn remote_stat = nullptr);
};

}  // namespace zssh::copy
