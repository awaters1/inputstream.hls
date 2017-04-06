/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../src/ring_buffer.h"

TEST(RingBuffer, PutPartial) {
  RingBuffer buffer(5);
  buffer.put("123");
  uint8_t dest[3];
  EXPECT_EQ(3, buffer.get_size());
  EXPECT_TRUE(buffer.has_data(0, 3));
  buffer.read(0, 3, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '1', '2', '3'}));
}

TEST(RingBuffer, GetPartial) {
  RingBuffer buffer(5);
  buffer.put("123");
  uint8_t dest[2];
  EXPECT_EQ(3, buffer.get_size());
  EXPECT_TRUE(buffer.has_data(1, 2));
  buffer.read(1, 2, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '2', '3'}));
}

TEST(RingBuffer, PutMore) {
  RingBuffer buffer(5);
  buffer.put("123");
  EXPECT_EQ(3, buffer.get_size());
  buffer.put("456");
  EXPECT_EQ(5, buffer.get_size());
  uint8_t dest[4];
  // We don't have the 0th position
  EXPECT_FALSE(buffer.has_data(0, 5));
  // We do have from 1 to 5
  EXPECT_TRUE(buffer.has_data(1, 5));
  size_t amount_read = buffer.read(0, 5, dest);
  // Position 0 isn't in the buffer anymore
  EXPECT_EQ(4, amount_read);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({'2', '3', '4', '5'}));
}

TEST(RingBuffer, PutMoreAgain) {
  RingBuffer buffer(5);
  buffer.put("123");
  EXPECT_EQ(3, buffer.get_size());
  buffer.put("456");
  EXPECT_EQ(5, buffer.get_size());
  buffer.put("789");
  EXPECT_EQ(5, buffer.get_size());
  uint8_t dest[5];
  // We don't have the 0th position
  EXPECT_FALSE(buffer.has_data(0, 5));
  EXPECT_TRUE(buffer.has_data(4, 5));
  size_t amount_read = buffer.read(4, 5, dest);
  EXPECT_EQ(5, amount_read);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({'5', '6', '7', '8', '9'}));
}
