/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include "gtest/gtest.h"

#include "../src/queue/active_segment_controller.h"
#include "../src/queue/file_downloader.h"

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
  active_segment_controller.get_download_segment(0).wait();
  ASSERT_EQ(1, active_segment_controller.download_segment_index);
  ASSERT_EQ(SegmentState::DOWNLOADED, active_segment_controller.segment_state[segment]);
}
