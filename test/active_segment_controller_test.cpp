/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <fstream>
#include <limits.h>
#include "gtest/gtest.h"

#include "../src/hls/active_segment_controller.h"
#include "../src/downloader/file_downloader.h"

void copy_file(std::string source, std::string destination) {
  std::ifstream src(source, std::ios::binary);
  std::ofstream dst(destination, std::ios::binary);

  dst << src.rdbuf();
  src.close();
  dst.close();
}

TEST(ActiveSegmentController, CreateController) {
  ActiveSegmentController active_segment_controller(new FileDownloader);
  EXPECT_TRUE(true);
}

TEST(ActiveSegmentController, DownloadSegment) {
  ActiveSegmentController active_segment_controller(new FileDownloader);
  hls::FileMediaPlaylist mp;
  mp.open("test/hls/gear1/prog_index.m3u8");
  active_segment_controller.set_media_playlist(mp);
//  auto demux_container = active_segment_controller.get_next_segment();
//  ASSERT_TRUE(active_segment);
//  std::cout << "Segment2\n";
//  auto demux_container2 = active_segment_controller.get_next_segment();
//  ASSERT_TRUE(future2.get());
//  ASSERT_EQ(2, active_segment_controller.download_segment_index);
}

TEST(ActiveSegmentController, ReloadPlaylist) {
  copy_file("test/live/fake_live.m3u8", "test/live/temp_playlist.m3u8");
  hls::FileMediaPlaylist media_playlist = hls::FileMediaPlaylist();
  media_playlist.open("test/live/temp_playlist.m3u8");
  ActiveSegmentController active_segment_controller(new FileDownloader);
  active_segment_controller.set_media_playlist(media_playlist);
  active_segment_controller.get_next_segment();
  std::cout << "Added in new playlist\n";
  copy_file("test/live/fake_live_reload.m3u8", "test/live/temp_playlist.m3u8");
  active_segment_controller.get_next_segment();
  // Gets segment 3 which causes a reload
  active_segment_controller.get_next_segment();
  EXPECT_EQ(4, active_segment_controller.media_playlist.get_number_of_segments());
}
