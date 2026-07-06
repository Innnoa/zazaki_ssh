#include <gtest/gtest.h>

#include "zssh/session/BackpressureController.hpp"
#include "zssh/session/BoundedByteQueue.hpp"

TEST(BackpressureControllerTest, RequestsPauseAtHighWatermarkAndResumeAtLowWatermark) {
  zssh::session::BackpressureController controller(1024, 256);

  EXPECT_FALSE(controller.should_pause(512));
  EXPECT_FALSE(controller.should_resume(128));
  EXPECT_TRUE(controller.should_pause(1024));
  EXPECT_FALSE(controller.should_pause(2048));
  EXPECT_FALSE(controller.should_resume(257));
  EXPECT_TRUE(controller.should_resume(256));
  EXPECT_FALSE(controller.should_resume(128));
}

TEST(BackpressureControllerTest, RequestsPauseBeforeIncomingChunkWouldCrossThreshold) {
  zssh::session::BackpressureController controller(1024, 256);

  EXPECT_TRUE(controller.should_pause(1000, 24));
}

TEST(BackpressureControllerTest, RejectsInvertedThresholds) {
  EXPECT_THROW(
      static_cast<void>(zssh::session::BackpressureController(256, 1024)),
      std::invalid_argument);
  EXPECT_THROW(
      static_cast<void>(zssh::session::BackpressureController(256, 256)),
      std::invalid_argument);
}

TEST(BackpressureControllerTest, RejectsZeroThresholds) {
  EXPECT_THROW(
      static_cast<void>(zssh::session::BackpressureController(0, 0)),
      std::invalid_argument);
  EXPECT_THROW(
      static_cast<void>(zssh::session::BackpressureController(0, 1)),
      std::invalid_argument);
  EXPECT_THROW(
      static_cast<void>(zssh::session::BackpressureController(1, 0)),
      std::invalid_argument);
}

TEST(BackpressureControllerTest, QueueCanDrainBufferedBytes) {
  zssh::session::BoundedByteQueue queue(1024);

  queue.push("abc");
  queue.push("de");
  EXPECT_EQ(queue.size(), 5U);

  EXPECT_EQ(queue.pop(), "abc");
  EXPECT_EQ(queue.size(), 2U);
  EXPECT_EQ(queue.pop(), "de");
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0U);
}

TEST(BackpressureControllerTest, QueueRejectsPushBeyondHighWatermark) {
  zssh::session::BoundedByteQueue queue(4);

  queue.push("ab");
  queue.push("cd");
  EXPECT_EQ(queue.size(), 4U);
  EXPECT_THROW(queue.push("cde"), std::length_error);
  EXPECT_EQ(queue.size(), 4U);
}

TEST(BackpressureControllerTest, QueueRejectsZeroHighWatermark) {
  EXPECT_THROW(
      static_cast<void>(zssh::session::BoundedByteQueue(0)),
      std::invalid_argument);
}

TEST(BackpressureControllerTest, QueueRejectsEmptyChunk) {
  zssh::session::BoundedByteQueue queue(4);

  EXPECT_THROW(queue.push(""), std::invalid_argument);
  EXPECT_TRUE(queue.empty());
  EXPECT_EQ(queue.size(), 0U);
}
