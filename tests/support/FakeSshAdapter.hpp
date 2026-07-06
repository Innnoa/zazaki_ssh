#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "zssh/protocol/ISshAdapter.hpp"

namespace zssh::tests {

class FakeSshAdapter : public zssh::protocol::ISshAdapter {
 public:
  using SftpHandle = zssh::protocol::SftpHandle;
  void set_exec_result(int exit_code, std::string stdout_text, std::string stderr_text) {
    response_.exit_code = exit_code;
    stdout_text_ = std::move(stdout_text);
    stderr_text_ = std::move(stderr_text);
  }

  void connect(const zssh::protocol::SshConnectionConfig& connection) override {
    connected_connection = connection;
    connect_calls += 1;
  }

  zssh::protocol::ExecResponse exec(
      const std::string& remote_command,
      const zssh::protocol::ExecCallbacks& callbacks) override {
    last_remote_command = remote_command;
    exec_calls += 1;
    if (callbacks.on_stdout && !stdout_text_.empty()) {
      callbacks.on_stdout(stdout_text_);
    }
    if (callbacks.on_stderr && !stderr_text_.empty()) {
      callbacks.on_stderr(stderr_text_);
    }
    return response_;
  }

  void start_interactive_shell() override {
    interactive_shell_calls += 1;
  }

  void set_sftp_stat_result(const std::string& path, zssh::protocol::SftpFileAttributes attrs) {
    sftp_stats_[path] = attrs;
  }

  void set_sftp_list_result(const std::string& path, std::vector<std::string> entries) {
    sftp_lists_[path] = std::move(entries);
  }

  zssh::protocol::SftpFileAttributes sftp_stat(const std::string& remote_path) override {
    auto it = sftp_stats_.find(remote_path);
    if (it != sftp_stats_.end()) return it->second;
    return zssh::protocol::SftpFileAttributes{};
  }

  std::vector<std::string> sftp_list_directory(const std::string& remote_path) override {
    auto it = sftp_lists_.find(remote_path);
    if (it != sftp_lists_.end()) return it->second;
    return {};
  }

  void sftp_mkdir(const std::string&) override {}

  SftpHandle sftp_open_read(const std::string& remote_path, std::uint64_t offset = 0) override {
    if (sftp_open_read_handler_) return sftp_open_read_handler_(remote_path, offset);
    return next_sftp_handle_++;
  }

  SftpHandle sftp_open_write(const std::string& remote_path, std::uint64_t offset = 0) override {
    if (sftp_open_write_handler_) return sftp_open_write_handler_(remote_path, offset);
    return next_sftp_handle_++;
  }

  std::uint64_t sftp_read(SftpHandle handle, void* buffer, std::uint64_t count) override {
    if (sftp_read_handler_) return sftp_read_handler_(handle, buffer, count);
    auto it = sftp_read_buffers_.find(handle);
    if (it == sftp_read_buffers_.end()) return 0;
    std::uint64_t available = it->second.size();
    std::uint64_t to_copy = std::min(count, available);
    std::memcpy(buffer, it->second.data(), to_copy);
    it->second = it->second.substr(to_copy);
    return to_copy;
  }

  std::uint64_t sftp_write(SftpHandle handle, const void* buffer, std::uint64_t count) override {
    if (sftp_write_handler_) return sftp_write_handler_(handle, buffer, count);
    last_written_data_.append(static_cast<const char*>(buffer), count);
    last_written_handle_ = handle;
    return count;
  }

  void sftp_close(SftpHandle) override {}
  void sftp_remove(const std::string&) override {}
  void sftp_set_attributes(const std::string&, const zssh::protocol::SftpFileAttributes&) override {}

  using SftpOpenHandler = std::function<SftpHandle(const std::string&, std::uint64_t)>;
  using SftpWriteHandler = std::function<std::uint64_t(SftpHandle, const void*, std::uint64_t)>;
  using SftpReadHandler = std::function<std::uint64_t(SftpHandle, void*, std::uint64_t)>;

  void set_sftp_open_write_handler(SftpOpenHandler h) { sftp_open_write_handler_ = std::move(h); }
  void set_sftp_write_handler(SftpWriteHandler h) { sftp_write_handler_ = std::move(h); }
  void set_sftp_open_read_handler(SftpOpenHandler h) { sftp_open_read_handler_ = std::move(h); }
  void set_sftp_read_handler(SftpReadHandler h) { sftp_read_handler_ = std::move(h); }

  void set_sftp_read_buffer(SftpHandle handle, std::string data) {
    sftp_read_buffers_[handle] = std::move(data);
  }

  zssh::protocol::SshConnectionConfig connected_connection;
  std::string last_remote_command;
  std::string last_written_data_;
  int connect_calls{0};
  int exec_calls{0};
  int interactive_shell_calls{0};
  SftpHandle last_written_handle_{0};

 private:
  zssh::protocol::ExecResponse response_{0};
  std::string stdout_text_;
  std::string stderr_text_;
  std::unordered_map<std::string, zssh::protocol::SftpFileAttributes> sftp_stats_;
  std::unordered_map<std::string, std::vector<std::string>> sftp_lists_;
  std::unordered_map<SftpHandle, std::string> sftp_read_buffers_;
  SftpHandle next_sftp_handle_{1};

  SftpOpenHandler sftp_open_write_handler_;
  SftpWriteHandler sftp_write_handler_;
  SftpOpenHandler sftp_open_read_handler_;
  SftpReadHandler sftp_read_handler_;
};

}  // namespace zssh::tests
