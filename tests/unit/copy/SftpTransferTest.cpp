#include <gtest/gtest.h>
#include "tests/support/FakeSshAdapter.hpp"
#include "zssh/copy/SftpTransfer.hpp"

#include <cstring>

TEST(SftpTransferTest, UploadWritesEntireFile) {
  zssh::tests::FakeSshAdapter ssh;
  std::string written_data;

  ssh.set_sftp_open_write_handler([&](const std::string&, std::uint64_t) {
    return 1;
  });
  ssh.set_sftp_write_handler([&](zssh::protocol::SftpHandle, const void* buf, std::uint64_t count) {
    written_data.append(static_cast<const char*>(buf), count);
    return count;
  });

  zssh::copy::SftpTransfer transfer(ssh);
  std::string content = "hello sftp world";

  bool ok = transfer.upload_file("/tmp/local.txt", "/remote/local.txt", content.data(), content.size());

  EXPECT_TRUE(ok);
  EXPECT_EQ(written_data, "hello sftp world");
}

TEST(SftpTransferTest, DownloadReadsEntireFile) {
  zssh::tests::FakeSshAdapter ssh;
  std::string file_content = "remote file data";

  ssh.set_sftp_open_read_handler([&](const std::string&, std::uint64_t) {
    return 1;
  });
  ssh.set_sftp_read_handler([&](zssh::protocol::SftpHandle, void* buf, std::uint64_t count) -> std::uint64_t {
    static std::uint64_t offset = 0;
    std::uint64_t remaining = file_content.size() - offset;
    std::uint64_t to_copy = std::min(count, remaining);
    std::memcpy(buf, file_content.data() + offset, to_copy);
    offset += to_copy;
    return to_copy;
  });

  zssh::copy::SftpTransfer transfer(ssh);
  std::string buffer(1024, '\0');

  std::uint64_t bytes_read = transfer.download_file("/remote/data.bin", buffer.data(), buffer.size());

  EXPECT_EQ(bytes_read, 16);
  EXPECT_EQ(std::string(buffer.data(), bytes_read), "remote file data");
}

TEST(SftpTransferTest, UploadReturnsFalseOnWriteFailure) {
  zssh::tests::FakeSshAdapter ssh;

  ssh.set_sftp_open_write_handler([](const std::string&, std::uint64_t) { return 1; });
  ssh.set_sftp_write_handler([](zssh::protocol::SftpHandle, const void*, std::uint64_t) -> std::uint64_t { return 0; });

  zssh::copy::SftpTransfer transfer(ssh);
  std::string content = "data";

  bool ok = transfer.upload_file("/tmp/fail.txt", "/remote/fail.txt", content.data(), content.size());

  EXPECT_FALSE(ok);
}
