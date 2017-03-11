/*
 * hls_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */


#include <limits.h>
#include "gtest/gtest.h"
#include "../../src/hls/HLS.h"

namespace hls {
TEST(HlsTest, LoadMasterPlaylist) {
  hls::FileMasterPlaylist mp = hls::FileMasterPlaylist();
  bool ret = mp.open("test/hls/bipbopall.m3u8");
  EXPECT_TRUE(ret);
}

TEST(HlsTest, LoadMasterPlaylistBad) {
  hls::FileMasterPlaylist mp = hls::FileMasterPlaylist();
  bool ret = mp.open("test/hls/hls_test.cpp");
  EXPECT_FALSE(ret);
}

TEST(HlsTest, GetAttributeValue) {
  MasterPlaylist mp = MasterPlaylist();
  std::string attribute_value;
  attribute_value = mp.get_attribute_value("#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=200000", "PROGRAM-ID");
  EXPECT_EQ("1", attribute_value);
  attribute_value = mp.get_attribute_value("#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=200000", "BANDWIDTH");
  EXPECT_EQ("200000", attribute_value);
  attribute_value = mp.get_attribute_value("#EXT-X-STREAM-INF:PROGRAM-ID=1, BANDWIDTH=200000", "AVERAGE_BANDWIDTH");
  EXPECT_EQ("", attribute_value);
}

TEST(HlsTest, GetBaseUrl) {
  hls::FileMasterPlaylist mp = hls::FileMasterPlaylist();
  mp.open("test/hls/bipbopall.m3u8");
  EXPECT_EQ("test/hls/", mp.get_base_url());
}

TEST(HlsTest, StreamUrl) {
  hls::FileMasterPlaylist mp = hls::FileMasterPlaylist();
  mp.open("test/hls/bipbopall.m3u8");
  std::vector<MediaPlaylist*> streams = mp.get_streams();
  ASSERT_EQ(4, streams.size());
  EXPECT_EQ("test/hls/gear1/prog_index.m3u8", streams[0]->get_url());
  EXPECT_EQ("test/hls/gear2/prog_index.m3u8", streams[1]->get_url());
  EXPECT_EQ("test/hls/gear3/prog_index.m3u8", streams[2]->get_url());
  EXPECT_EQ("test/hls/gear4/prog_index.m3u8", streams[3]->get_url());
}

TEST(HlsTest, LoadMediaPlaylist) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  bool ret = mp.open("test/hls/gear1/prog_index.m3u8");
  EXPECT_TRUE(ret);
}

TEST(HlsTest, GetAttributes) {
  MediaPlaylist mp = MediaPlaylist();
  std::vector<std::string> attributes;
  attributes = mp.get_attributes("#EXTINF:10, no desc");
  ASSERT_EQ(2, attributes.size());
  EXPECT_EQ("10", attributes[0]);
  EXPECT_EQ(" no desc", attributes[1]);
  attributes = mp.get_attributes("#EXT-X-TARGETDURATION:10");
  ASSERT_EQ(1, attributes.size());
  EXPECT_EQ("10", attributes[0]);
}

TEST(HlsTest, MediaPlayistUrl) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  mp.open("test/hls/gear1/prog_index.m3u8");
  EXPECT_EQ("test/hls/gear1/", mp.get_base_url());
}

TEST(HlsTest, SegmentUrl) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  mp.open("test/hls/gear1/prog_index.m3u8");
  std::vector<Segment*> segments = mp.get_segments();
  ASSERT_EQ(181, segments.size());
  EXPECT_EQ("test/hls/gear1/fileSequence0.ts", segments[0]->get_url());
}
}
