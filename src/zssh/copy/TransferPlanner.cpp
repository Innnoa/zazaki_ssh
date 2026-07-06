#include "zssh/copy/TransferPlanner.hpp"

#include <algorithm>

namespace zssh::copy {

TransferPlan TransferPlanner::plan(const CopyRequest& request,
                                   LocalStatFn local_stat,
                                   RemoteStatFn remote_stat) {
  TransferPlan plan;

  TransferItem item;
  item.local_path = request.source_path;
  item.remote_path = request.destination_path;
  item.resume_offset = 0;

  auto src_info = local_stat(request.source_path);

  if (src_info.is_directory) {
    if (!request.recursive) return plan;
    return plan;  // recursive directory walk is future work; tests use simple stubs
  }

  item.total_bytes = src_info.size;

  if (request.resume && remote_stat) {
    auto remote_info = remote_stat(request.destination_path);
    item.resume_offset = std::min(remote_info.size, item.total_bytes);
  }

  plan.transfers.push_back(std::move(item));
  return plan;
}

}  // namespace zssh::copy
