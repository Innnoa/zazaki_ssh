#include "zssh/protocol/LibsshAdapter.hpp"

#include <string>

#include <cstdint>
#include <fcntl.h>

#include <poll.h>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

#include <libssh/callbacks.h>
#include <libssh/libssh.h>
#include <libssh/sftp.h>

#include "zssh/protocol/LibsshError.hpp"

namespace zssh::protocol {
namespace {

SshErrorCategory categorize_ssh_error(int rc) {
  switch (rc) {
    case SSH_AUTH_ERROR:
      return SshErrorCategory::auth_failed;
    default:
      return SshErrorCategory::unknown;
  }
}

}  // namespace

LibsshAdapter::LibsshAdapter() : session_(ssh_new()) {
  if (session_ == nullptr) {
    throw LibsshError(SshErrorCategory::unknown, "ssh_new failed");
  }
}

LibsshAdapter::~LibsshAdapter() {
  if (sftp_ != nullptr) {
    sftp_free(static_cast<sftp_session_struct*>(sftp_));
  }
  if (session_ != nullptr) {
    ssh_disconnect(session_);
    ssh_free(session_);
  }
}

void LibsshAdapter::connect(const SshConnectionConfig& connection) {
  if (session_ == nullptr) {
    throw LibsshError(SshErrorCategory::unknown, "session is null");
  }

  ssh_options_set(session_, SSH_OPTIONS_HOST, connection.host.c_str());
  ssh_options_set(session_, SSH_OPTIONS_PORT, &connection.port);
  ssh_options_set(session_, SSH_OPTIONS_USER, connection.auth.username.c_str());

  const int connect_rc = ssh_connect(session_);
  if (connect_rc != SSH_OK) {
    const auto category = categorize_ssh_error(connect_rc);
    std::string message = "ssh_connect to ";
    message += connection.host;
    message += ":";
    message += std::to_string(connection.port);
    message += " failed: ";
    message += ssh_get_error(session_);
    ssh_free(session_);
    session_ = nullptr;
    throw LibsshError(category, message);
  }

  authenticate(connection);
  enable_keepalive();
}

void LibsshAdapter::authenticate(const SshConnectionConfig& connection) {
  if (connection.auth.method == "publickey") {
    if (!connection.auth.key_path.empty()) {
      ssh_key key = nullptr;
      const int import_rc = ssh_pki_import_privkey_file(
          connection.auth.key_path.c_str(), nullptr, nullptr, nullptr, &key);
      if (import_rc == SSH_OK) {
        const int auth_rc = ssh_userauth_publickey(
            session_, nullptr, key);
        ssh_key_free(key);
        if (auth_rc == SSH_AUTH_SUCCESS) {
          return;
        }
      }
    }

    const int auto_rc = ssh_userauth_publickey_auto(
        session_, connection.auth.username.c_str(), nullptr);
    if (auto_rc == SSH_AUTH_SUCCESS) {
      return;
    }

    if (connection.auth.use_agent) {
      const int agent_rc = ssh_userauth_agent(
          session_, connection.auth.username.c_str());
      if (agent_rc == SSH_AUTH_SUCCESS) {
        return;
      }
    }

    throw LibsshError(
        SshErrorCategory::auth_failed,
        "publickey authentication failed for " + connection.host);
  }

  if (connection.auth.method == "password") {
    if (connection.auth.password_command.empty()) {
      throw LibsshError(
          SshErrorCategory::auth_failed,
          "password authentication requires password_command for " + connection.host);
    }

    const int rc = ssh_userauth_password(
        session_, nullptr, connection.auth.password_command.c_str());
    if (rc != SSH_AUTH_SUCCESS) {
      throw LibsshError(
          SshErrorCategory::auth_failed,
          "password authentication failed for " + connection.host + ": " +
              std::string(ssh_get_error(session_)));
    }
    return;
  }

  throw LibsshError(
      SshErrorCategory::auth_failed,
      "unsupported auth method '" + connection.auth.method + "' for " + connection.host);
}

void LibsshAdapter::enable_keepalive() {
}

ExecResponse LibsshAdapter::exec(
    const std::string& remote_command,
    const ExecCallbacks& callbacks) {
  ssh_channel channel = ssh_channel_new(session_);
  if (channel == nullptr) {
    throw LibsshError(
        SshErrorCategory::channel_closed,
        "ssh_channel_new failed: " + std::string(ssh_get_error(session_)));
  }

  const int open_rc = ssh_channel_open_session(channel);
  if (open_rc != SSH_OK) {
    ssh_channel_free(channel);
    throw LibsshError(
        SshErrorCategory::channel_closed,
        "ssh_channel_open_session failed: " + std::string(ssh_get_error(session_)));
  }

  const int exec_rc = ssh_channel_request_exec(channel, remote_command.c_str());
  if (exec_rc != SSH_OK) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    throw LibsshError(
        SshErrorCategory::exec_failed,
        "ssh_channel_request_exec failed: " + std::string(ssh_get_error(session_)));
  }

  char buffer[4096];
  int bytes_read;
  while ((bytes_read = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0) {
    if (callbacks.on_stdout) {
      callbacks.on_stdout(std::string_view(buffer, static_cast<std::size_t>(bytes_read)));
    }
  }

  while ((bytes_read = ssh_channel_read(channel, buffer, sizeof(buffer), 1)) > 0) {
    if (callbacks.on_stderr) {
      callbacks.on_stderr(std::string_view(buffer, static_cast<std::size_t>(bytes_read)));
    }
  }

  ssh_channel_send_eof(channel);
  const int exit_code = ssh_channel_get_exit_status(channel);
  ssh_channel_close(channel);
  ssh_channel_free(channel);

  return ExecResponse{exit_code};
}

void LibsshAdapter::start_interactive_shell() {
  ssh_channel channel = ssh_channel_new(session_);
  if (channel == nullptr) {
    throw LibsshError(
        SshErrorCategory::channel_closed,
        "ssh_channel_new failed: " + std::string(ssh_get_error(session_)));
  }

  const int open_rc = ssh_channel_open_session(channel);
  if (open_rc != SSH_OK) {
    ssh_channel_free(channel);
    throw LibsshError(
        SshErrorCategory::channel_closed,
        "ssh_channel_open_session failed: " + std::string(ssh_get_error(session_)));
  }

  const int pty_rc = ssh_channel_request_pty(channel);
  if (pty_rc != SSH_OK) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    throw LibsshError(
        SshErrorCategory::channel_closed,
        "ssh_channel_request_pty failed: " + std::string(ssh_get_error(session_)));
  }

  const int shell_rc = ssh_channel_request_shell(channel);
  if (shell_rc != SSH_OK) {
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    throw LibsshError(
        SshErrorCategory::channel_closed,
        "ssh_channel_request_shell failed: " + std::string(ssh_get_error(session_)));
  }

  termios original_termios;
  tcgetattr(STDIN_FILENO, &original_termios);
  termios raw = original_termios;
  cfmakeraw(&raw);
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  signal(SIGWINCH, SIG_IGN);

  char buffer[4096];
  bool running = true;

  while (running) {
    pollfd stdin_fd;
    stdin_fd.fd = STDIN_FILENO;
    stdin_fd.events = POLLIN;
    stdin_fd.revents = 0;

    ::poll(&stdin_fd, 1, 5);

    if (stdin_fd.revents & POLLIN) {
      const ssize_t bytes = ::read(STDIN_FILENO, buffer, sizeof(buffer));
      if (bytes > 0) {
        ssh_channel_write(channel, buffer, static_cast<std::uint32_t>(bytes));
      } else {
        running = false;
      }
    }

    while (ssh_channel_poll(channel, 0) > 0) {
      const int bytes_read =
          ssh_channel_read(channel, buffer, sizeof(buffer), 0);
      if (bytes_read > 0) {
        ::write(STDOUT_FILENO, buffer, static_cast<std::size_t>(bytes_read));
      } else {
        running = false;
        break;
      }
    }

    if (ssh_channel_is_eof(channel)) {
      running = false;
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
  ssh_channel_close(channel);
  ssh_channel_free(channel);
}

void* LibsshAdapter::ensure_sftp_session() {
  if (sftp_ != nullptr) {
    return sftp_;
  }

  auto sftp = sftp_new(session_);
  if (sftp == nullptr) {
    throw LibsshError(SshErrorCategory::channel_closed,
        "sftp_new failed: " + std::string(ssh_get_error(session_)));
  }

  const int init_rc = sftp_init(sftp);
  if (init_rc != SSH_OK) {
    sftp_free(sftp);
    throw LibsshError(SshErrorCategory::channel_closed,
        "sftp_init failed: " + std::string(ssh_get_error(session_)));
  }

  sftp_ = sftp;
  return sftp_;
}

SftpFileAttributes LibsshAdapter::sftp_stat(const std::string& remote_path) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  sftp_attributes attrs = ::sftp_stat(sftp, remote_path.c_str());
  if (attrs == nullptr) {
    return {0, false, false};
  }

  SftpFileAttributes result;
  result.size = attrs->size;
  result.is_directory = (attrs->type == SSH_FILEXFER_TYPE_DIRECTORY);
  result.exists = true;
  sftp_attributes_free(attrs);
  return result;
}

std::vector<std::string> LibsshAdapter::sftp_list_directory(const std::string& remote_path) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  sftp_dir dir = sftp_opendir(sftp, remote_path.c_str());
  if (dir == nullptr) {
    throw LibsshError(SshErrorCategory::unknown,
        "sftp_opendir failed for " + remote_path + ": " +
            std::string(ssh_get_error(session_)));
  }

  std::vector<std::string> entries;
  sftp_attributes attrs;
  while ((attrs = sftp_readdir(sftp, dir)) != nullptr) {
    entries.emplace_back(attrs->name);
    sftp_attributes_free(attrs);
  }

  sftp_closedir(dir);
  return entries;
}

void LibsshAdapter::sftp_mkdir(const std::string& remote_path) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  const int rc = ::sftp_mkdir(sftp, remote_path.c_str(), 0755);
  if (rc != SSH_OK) {
    throw LibsshError(SshErrorCategory::unknown,
        "sftp_mkdir failed for " + remote_path + ": " +
            std::string(ssh_get_error(session_)));
  }
}

SftpHandle LibsshAdapter::sftp_open_read(const std::string& remote_path, std::uint64_t offset) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  sftp_file file = sftp_open(sftp, remote_path.c_str(), O_RDONLY, 0);
  if (file == nullptr) {
    throw LibsshError(SshErrorCategory::unknown,
        "sftp_open(read) failed for " + remote_path + ": " +
            std::string(ssh_get_error(session_)));
  }

  if (offset > 0) {
    sftp_seek64(file, offset);
  }

  return reinterpret_cast<SftpHandle>(file);
}

SftpHandle LibsshAdapter::sftp_open_write(const std::string& remote_path, std::uint64_t offset) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  int flags = O_WRONLY | O_CREAT | O_TRUNC;
  if (offset > 0) {
    flags = O_WRONLY | O_CREAT;
  }
  sftp_file file = sftp_open(sftp, remote_path.c_str(), flags, 0644);
  if (file == nullptr) {
    throw LibsshError(SshErrorCategory::unknown,
        "sftp_open(write) failed for " + remote_path + ": " +
            std::string(ssh_get_error(session_)));
  }

  if (offset > 0) {
    sftp_seek64(file, offset);
  }

  return reinterpret_cast<SftpHandle>(file);
}

std::uint64_t LibsshAdapter::sftp_read(SftpHandle handle, void* buffer, std::uint64_t count) {
  auto file = reinterpret_cast<sftp_file>(handle);
  const auto bytes = ::sftp_read(file, buffer, count);
  if (bytes < 0) {
    throw LibsshError(SshErrorCategory::unknown, "sftp_read failed");
  }
  return static_cast<std::uint64_t>(bytes);
}

std::uint64_t LibsshAdapter::sftp_write(SftpHandle handle, const void* buffer, std::uint64_t count) {
  auto file = reinterpret_cast<sftp_file>(handle);
  const auto bytes = ::sftp_write(file, buffer, count);
  if (bytes < 0) {
    throw LibsshError(SshErrorCategory::unknown, "sftp_write failed");
  }
  return static_cast<std::uint64_t>(bytes);
}

void LibsshAdapter::sftp_close(SftpHandle handle) {
  auto file = reinterpret_cast<sftp_file>(handle);
  ::sftp_close(file);
}

void LibsshAdapter::sftp_remove(const std::string& remote_path) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  const int rc = sftp_unlink(sftp, remote_path.c_str());
  if (rc != SSH_OK) {
    throw LibsshError(SshErrorCategory::unknown,
        "sftp_unlink failed for " + remote_path + ": " +
            std::string(ssh_get_error(session_)));
  }
}

void LibsshAdapter::sftp_set_attributes(
    const std::string& remote_path, const SftpFileAttributes& attrs) {
  auto sftp = static_cast<sftp_session_struct*>(ensure_sftp_session());
  sftp_attributes_struct attrs_buf{};
  attrs_buf.size = attrs.size;
  if (attrs.size > 0) {
    attrs_buf.flags |= SSH_FILEXFER_ATTR_SIZE;
  }

  const int rc = sftp_setstat(sftp, remote_path.c_str(), &attrs_buf);
  if (rc != SSH_OK) {
    throw LibsshError(SshErrorCategory::unknown,
        "sftp_setstat failed for " + remote_path + ": " +
            std::string(ssh_get_error(session_)));
  }
}

}  // namespace zssh::protocol
