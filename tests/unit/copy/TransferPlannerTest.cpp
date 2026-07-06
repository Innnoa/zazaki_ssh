#include <gtest/gtest.h>
#include "zssh/copy/TransferPlanner.hpp"
#include "zssh/copy/CopyRequest.hpp"

TEST(TransferPlannerTest, PlanSingleFileUpload) {
  zssh::copy::CopyRequest request;
  request.source_path = "/tmp/local.txt";
  request.destination_path = "/remote/path/local.txt";
  request.direction = zssh::copy::CopyDirection::LocalToRemote;

  auto plan = zssh::copy::TransferPlanner::plan(request, [](const std::string& path) {
    return zssh::copy::FileInfo{path, 1024, false};
  });

  ASSERT_EQ(plan.transfers.size(), 1);
  EXPECT_EQ(plan.transfers[0].local_path, "/tmp/local.txt");
  EXPECT_EQ(plan.transfers[0].remote_path, "/remote/path/local.txt");
  EXPECT_EQ(plan.transfers[0].total_bytes, 1024);
}

TEST(TransferPlannerTest, NonRecursiveDirectoryReturnsEmpty) {
  zssh::copy::CopyRequest request;
  request.source_path = "/tmp/dir";
  request.destination_path = "/remote/dir";
  request.direction = zssh::copy::CopyDirection::LocalToRemote;
  request.recursive = false;

  auto plan = zssh::copy::TransferPlanner::plan(request, [](const std::string& path) {
    if (path == "/tmp/dir") return zssh::copy::FileInfo{path, 0, true};
    return zssh::copy::FileInfo{path, 0, false};
  });

  EXPECT_EQ(plan.transfers.size(), 0);
}

TEST(TransferPlannerTest, ResumeCalculatesOffset) {
  zssh::copy::CopyRequest request;
  request.source_path = "/tmp/big.bin";
  request.destination_path = "/remote/big.bin";
  request.resume = true;

  auto plan = zssh::copy::TransferPlanner::plan(request,
    [](const std::string& path) {
      if (path == "/tmp/big.bin") return zssh::copy::FileInfo{path, 4096, false};
      return zssh::copy::FileInfo{path, 0, false};
    },
    [](const std::string& path) {
      return zssh::copy::FileInfo{path, 1024, false};
    }
  );

  ASSERT_EQ(plan.transfers.size(), 1);
  EXPECT_EQ(plan.transfers[0].resume_offset, 1024);
}

TEST(TransferPlannerTest, ResumeWithoutRemoteStatUsesZeroOffset) {
  zssh::copy::CopyRequest request;
  request.source_path = "/tmp/file.txt";
  request.destination_path = "/remote/file.txt";
  request.resume = true;

  auto plan = zssh::copy::TransferPlanner::plan(request,
    [](const std::string&) { return zssh::copy::FileInfo{"/tmp/file.txt", 2048, false}; },
    nullptr
  );

  ASSERT_EQ(plan.transfers.size(), 1);
  EXPECT_EQ(plan.transfers[0].resume_offset, 0);
}
