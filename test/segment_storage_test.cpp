/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "../src/segment_storage.h"

TEST(SegmentStorage, WriteSegment) {
  hls::Segment segment;
  segment.valid = true;
  SegmentStorage segment_storage;
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment, "12345");
  segment_storage.end_segment(segment);
  uint8_t dest[5];
  EXPECT_TRUE(segment_storage.has_data(0, 5));
  size_t size = 5;
  segment_storage.read(0, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '1', '2', '3', '4', '5'}));
}

TEST(SegmentStorage, WriteMultipleSegments) {
  hls::Segment segment;
  segment.valid = true;
  SegmentStorage segment_storage;
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment, "12345");
  segment_storage.end_segment(segment);
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment, "6789101112131415");
  segment_storage.end_segment(segment);
  uint8_t dest[5];
  EXPECT_TRUE(segment_storage.has_data(0, 21));
  size_t size = 5;
  segment_storage.read(16, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '3', '1', '4', '1', '5'}));
}

TEST(SegmentStorage, ReadAcrossMultipleSegments) {
  hls::Segment segment;
  segment.valid = true;
  SegmentStorage segment_storage;
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment,"12345");
  segment_storage.end_segment(segment);
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment, "6789");
  segment_storage.end_segment(segment);
  uint8_t dest[7];
  EXPECT_TRUE(segment_storage.has_data(0, 7));
  size_t size = 7;
  segment_storage.read(0, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '1', '2', '3', '4', '5', '6', '7'}));
}

TEST(SegmentStorage, ReadPartialAcrossMultipleSegments) {
  hls::Segment segment;
  segment.valid = true;
  SegmentStorage segment_storage;
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment,"12345");
  segment_storage.end_segment(segment);
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment,"6789");
  segment_storage.end_segment(segment);
  uint8_t dest[7];
  EXPECT_TRUE(segment_storage.has_data(0, 7));
  size_t size = 7;
  segment_storage.read(0, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '1', '2', '3', '4', '5', '6', '7'}));
  size = 7;
  segment_storage.read(2, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '3', '4', '5', '6', '7', '8', '9'}));
}

TEST(SegmentStorage, ReadWriteSameSegment) {
  hls::Segment segment;
  segment.valid = true;
  SegmentStorage segment_storage;
  segment_storage.start_segment(segment);
  segment_storage.write_segment(segment, "12345");
  uint8_t dest[5];
  EXPECT_TRUE(segment_storage.has_data(0, 5));
  size_t size = 5;
  segment_storage.read(0, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '1', '2', '3', '4', '5'}));
  segment_storage.write_segment(segment, "67890");
  size = 5;
  segment_storage.read(5, size, dest);
  EXPECT_THAT(dest, ::testing::ElementsAreArray({ '6', '7', '8', '9', '0'}));
}
