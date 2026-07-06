#include <gtest/gtest.h>
#include "tests/support/FakeSshAdapter.hpp"
#include "zssh/copy/CopyService.hpp"

#include <cstring>
#include <string>

class FakeProgressSink : public zssh::copy::IProgressSink {
 public:
  void on_progress(const zssh::copy::TransferProgress&) override { progress_calls_++; }
  void on_file_complete(const std::string&, bool success) override {
    if (success) { files_succeeded_++; } else { files_failed_++; }
  }
  void on_transfer_complete(const zssh::copy::TransferSummary& summary) override {
    summary_ = summary;
  }

  int progress_calls_{0};
  int files_succeeded_{0};
  int files_failed_{0};
  zssh::copy::TransferSummary summary_;
};

TEST(CopyServiceTest, RunSingleUploadCallsProgressAndCompletes) {
  zssh::tests::FakeSshAdapter ssh;
  FakeProgressSink sink;

  ssh.set_sftp_open_write_handler([](const std::string&, std::uint64_t) { return 1; });
  ssh.set_sftp_write_handler([](zssh::protocol::SftpHandle, const void*, std::uint64_t count) { return count; });

  zssh::copy::CopyRequest request;
  request.profile_name = "test";
  request.source_path = "/tmp/test.txt";
  request.destination_path = "/remote/test.txt";
  request.direction = zssh::copy::CopyDirection::LocalToRemote;

  zssh::copy::CopyService service(ssh);
  std::string file_content = "test data for copy service";
  bool ok = service.run(request, sink,
    [&](const std::string&) {
      return zssh::copy::FileInfo{request.source_path, static_cast<std::uint64_t>(file_content.size()), false};
    },
    [&](const std::string&, char* buf, std::uint64_t size) -> std::uint64_t {
      static std::uint64_t pos = 0;
      std::uint64_t remaining = file_content.size() - pos;
      std::uint64_t to_copy = std::min(size, remaining);
      std::memcpy(buf, file_content.data() + pos, to_copy);
      pos += to_copy;
      return to_copy;
    });

  EXPECT_TRUE(ok);
  EXPECT_GT(sink.progress_calls_, 0);
  EXPECT_EQ(sink.files_succeeded_, 1);
  EXPECT_EQ(sink.files_failed_, 0);
  EXPECT_EQ(sink.summary_.succeeded, 1);
  EXPECT_EQ(sink.summary_.total_files, 1);
}

TEST(CopyServiceTest, HandlesEmptyTransferPlan) {
  zssh::tests::FakeSshAdapter ssh;
  FakeProgressSink sink;

  zssh::copy::CopyRequest request;
  request.profile_name = "test";
  request.source_path = "/tmp/emptydir";
  request.destination_path = "/remote/emptydir";
  request.recursive = false;

  zssh::copy::CopyService service(ssh);
  bool ok = service.run(request, sink,
    [](const std::string&) { return zssh::copy::FileInfo{"/tmp/emptydir", 0, true}; },
    nullptr);

  EXPECT_TRUE(ok);
  EXPECT_EQ(sink.files_succeeded_, 0);
  EXPECT_EQ(sink.files_failed_, 0);
  EXPECT_EQ(sink.summary_.total_files, 0);
}

TEST(CopyServiceTest, HandlesDownload) {
  zssh::tests::FakeSshAdapter ssh;
  FakeProgressSink sink;

  zssh::copy::CopyRequest request;
  request.profile_name = "test";
  request.source_path = "/remote/file.bin";
  request.destination_path = "/tmp/file.bin";
  request.direction = zssh::copy::CopyDirection::RemoteToLocal;

  std::string remote_content = "remote content";
  ssh.set_sftp_open_read_handler([&](const std::string&, std::uint64_t) { return 1; });
  ssh.set_sftp_read_handler([&](zssh::protocol::SftpHandle, void* buf, std::uint64_t count) -> std::uint64_t {
    static std::uint64_t pos = 0;
    std::uint64_t remaining = remote_content.size() - pos;
    std::uint64_t to_copy = std::min(count, remaining);
    std::memcpy(buf, remote_content.data() + pos, to_copy);
    pos += to_copy;
    if (to_copy == 0) pos = 0;
    return to_copy;
  });

  std::string written_data;
  zssh::copy::CopyService service(ssh);
  bool ok = service.run(request, sink,
    [](const std::string&) { return zssh::copy::FileInfo{"/remote/file.bin", 14, false}; },
    nullptr,
    [&](const std::string&, const char* data, std::uint64_t size) -> std::uint64_t {
      written_data.append(data, size);
      return size;
    });

  EXPECT_TRUE(ok);
  EXPECT_EQ(written_data, "remote content");
  EXPECT_EQ(sink.files_succeeded_, 1);
}

TEST(CopyServiceTest, HandlesUpload) {
  zssh::tests::FakeSshAdapter ssh;
  FakeProgressSink sink;

  ssh.set_sftp_open_write_handler([](const std::string&, std::uint64_t) { return 1; });
  ssh.set_sftp_write_handler([](zssh::protocol::SftpHandle, const void*, std::uint64_t count) { return count; });

  zssh::copy::CopyRequest request;
  request.profile_name = "test";
  request.source_path = "/tmp/test.txt";
  request.destination_path = "/remote/test.txt";
  request.direction = zssh::copy::CopyDirection::LocalToRemote;

  zssh::copy::CopyService service(ssh);
  std::string file_content = "test data for copy service";
  bool ok = service.run(request, sink,
    [&](const std::string&) {
      return zssh::copy::FileInfo{request.source_path, static_cast<std::uint64_t>(file_content.size()), false};
    },
    [&](const std::string&, char* buf, std::uint64_t size) -> std::uint64_t {
      static std::uint64_t pos = 0;
      std::uint64_t remaining = file_content.size() - pos;
      std::uint64_t to_copy = std::min(size, remaining);
      std::memcpy(buf, file_content.data() + pos, to_copy);
      pos += to_copy;
      return to_copy;
    });

  EXPECT_TRUE(ok);
  EXPECT_EQ(sink.files_succeeded_, 1);
}
