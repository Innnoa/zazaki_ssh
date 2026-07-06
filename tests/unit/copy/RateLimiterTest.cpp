#include <gtest/gtest.h>
#include "zssh/copy/RateLimiter.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

TEST(RateLimiterTest, AllowsBurstUpToBucketSize) {
  zssh::copy::RateLimiter limiter(1024 * 1024);

  EXPECT_TRUE(limiter.try_consume(512 * 1024, 0s));
}

TEST(RateLimiterTest, BlocksWhenBucketExhausted) {
  zssh::copy::RateLimiter limiter(1024 * 1024);

  EXPECT_TRUE(limiter.try_consume(1024 * 1024, 0s));
  EXPECT_FALSE(limiter.try_consume(1, 0s));
}

TEST(RateLimiterTest, ReplenishesOverTime) {
  zssh::copy::RateLimiter limiter(1024 * 1024);

  limiter.try_consume(1024 * 1024, 0s);

  auto wait_time = limiter.wait_for_bytes(512 * 1024);
  EXPECT_GT(wait_time.count(), 0);
}

TEST(RateLimiterTest, RefillsAfterElapsedTime) {
  zssh::copy::RateLimiter limiter(500 * 1024);

  limiter.try_consume(500 * 1024, 0s);
  EXPECT_FALSE(limiter.try_consume(1, 0s));

  EXPECT_TRUE(limiter.try_consume(250 * 1024, 500ms));
}
