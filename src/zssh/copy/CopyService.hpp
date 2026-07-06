#pragma once

#include "zssh/copy/CopyRequest.hpp"
#include "zssh/copy/IProgressSink.hpp"
#include "zssh/copy/TransferPlanner.hpp"
#include "zssh/copy/SftpTransfer.hpp"
#include "zssh/protocol/ISshAdapter.hpp"

#include <functional>
#include <string>

namespace zssh::copy {

using LocalReadFn = std::function<std::uint64_t(const std::string&, char*, std::uint64_t)>;
using LocalWriteFn = std::function<std::uint64_t(const std::string&, const char*, std::uint64_t)>;

class CopyService {
 public:
  explicit CopyService(zssh::protocol::ISshAdapter& ssh);

  bool run(const CopyRequest& request, IProgressSink& sink,
           TransferPlanner::LocalStatFn local_stat,
           LocalReadFn local_read,
           LocalWriteFn local_write = nullptr);

 private:
  zssh::protocol::ISshAdapter& ssh_;
  SftpTransfer transfer_;
};

}  // namespace zssh::copy
