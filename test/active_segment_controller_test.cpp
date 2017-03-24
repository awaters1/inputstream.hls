/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include "gtest/gtest.h"

#include "../src/hls/active_segment_controller.h"
#include "../src/downloader/file_downloader.h"

TEST(ActiveSegmentController, CreateController) {
  ActiveSegmentController active_segment_controller(
      std::unique_ptr<Downloader>(new FileDownloader));
  EXPECT_TRUE(true);
}

TEST(ActiveSegmentController, DownloadSegment) {
  ActiveSegmentController active_segment_controller(
        std::unique_ptr<Downloader>(new FileDownloader));
  hls::Segment segment;
  segment.set_url("test/hls/decrypted_segment.ts");
  active_segment_controller.add_segment(segment);
  hls::Segment segment2;
  segment2.set_url("test/hls/encrypted_segment.ts");
  active_segment_controller.add_segment(segment2);
  std::future<std::unique_ptr<hls::ActiveSegment>> future = active_segment_controller.get_active_segment(segment);
  std::unique_ptr<hls::ActiveSegment> active_segment = future.get();
  ASSERT_EQ(SegmentState::DEMUXED, active_segment_controller.segment_data[segment].state);
  std::cout << "Segment2\n";
  auto future2 = active_segment_controller.get_active_segment(segment2);
  future2.wait();
  ASSERT_EQ(SegmentState::DEMUXED, active_segment_controller.segment_data[segment2].state);
  ASSERT_EQ(2, active_segment_controller.download_segment_index);
}
