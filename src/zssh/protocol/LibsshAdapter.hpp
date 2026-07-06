#pragma once

#include <libssh/libssh.h>

#include "zssh/protocol/ISshAdapter.hpp"

namespace zssh::protocol {

class LibsshAdapter final : public ISshAdapter {
 public:
  LibsshAdapter();
  ~LibsshAdapter() override;

  void connect(const SshConnectionConfig& connection) override;

  ExecResponse exec(
      const std::string& remote_command,
      const ExecCallbacks& callbacks) override;

  void start_interactive_shell() override;

  SftpFileAttributes sftp_stat(const std::string&) override;
  std::vector<std::string> sftp_list_directory(const std::string&) override;
  void sftp_mkdir(const std::string&) override;
  SftpHandle sftp_open_read(const std::string&, std::uint64_t = 0) override;
  SftpHandle sftp_open_write(const std::string&, std::uint64_t = 0) override;
  std::uint64_t sftp_read(SftpHandle, void*, std::uint64_t) override;
  std::uint64_t sftp_write(SftpHandle, const void*, std::uint64_t) override;
  void sftp_close(SftpHandle) override;
  void sftp_remove(const std::string&) override;
  void sftp_set_attributes(
      const std::string&, const SftpFileAttributes&) override;

 private:
  void authenticate(const SshConnectionConfig& connection);
  void enable_keepalive();
  void* ensure_sftp_session();

  ssh_session session_{nullptr};
  void* sftp_{nullptr};
};

}  // namespace zssh::protocol
