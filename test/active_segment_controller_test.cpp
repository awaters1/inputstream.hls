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
  hls::FileMediaPlaylist mp;
  mp.open("test/hls/gear1/prog_index.m3u8");
  active_segment_controller.set_media_playlist(mp);
  std::future<std::unique_ptr<hls::ActiveSegment>> future = active_segment_controller.get_next_segment();
  std::unique_ptr<hls::ActiveSegment> active_segment = future.get();
  ASSERT_TRUE(active_segment);
  std::cout << "Segment2\n";
  auto future2 = active_segment_controller.get_next_segment();
  future2.wait();
  ASSERT_TRUE(future2.get());
  ASSERT_EQ(2, active_segment_controller.download_segment_index);
}
