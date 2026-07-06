#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace zssh::protocol {

struct SshAuthConfig {
  std::string method;
  std::string username;
  std::string password_command;
  std::string key_path;
  bool use_agent{true};
};

struct SshConnectionConfig {
  std::string profile_name;
  std::string host;
  int port{22};
  SshAuthConfig auth;
};

struct ExecResponse {
  int exit_code;
};

struct ExecCallbacks {
  std::function<void(std::string_view)> on_stdout;
  std::function<void(std::string_view)> on_stderr;
};

struct SftpFileAttributes {
  std::uint64_t size{0};
  bool is_directory{false};
  bool exists{false};
};

using SftpHandle = std::intptr_t;

class ISshAdapter {
 public:
  virtual ~ISshAdapter() = default;

  virtual void connect(const SshConnectionConfig& connection) = 0;
  virtual ExecResponse exec(const std::string& remote_command, const ExecCallbacks& callbacks) = 0;
  virtual void start_interactive_shell() = 0;

  virtual SftpFileAttributes sftp_stat(const std::string& remote_path) = 0;
  virtual std::vector<std::string> sftp_list_directory(const std::string& remote_path) = 0;
  virtual void sftp_mkdir(const std::string& remote_path) = 0;
  virtual SftpHandle sftp_open_read(const std::string& remote_path, std::uint64_t offset = 0) = 0;
  virtual SftpHandle sftp_open_write(const std::string& remote_path, std::uint64_t offset = 0) = 0;
  virtual std::uint64_t sftp_read(SftpHandle handle, void* buffer, std::uint64_t count) = 0;
  virtual std::uint64_t sftp_write(SftpHandle handle, const void* buffer, std::uint64_t count) = 0;
  virtual void sftp_close(SftpHandle handle) = 0;
  virtual void sftp_remove(const std::string& remote_path) = 0;
  virtual void sftp_set_attributes(const std::string& remote_path, const SftpFileAttributes& attrs) = 0;
};

}  // namespace zssh::protocol
