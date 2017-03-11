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
  EXPECT_EQ(4, streams.size());
  EXPECT_EQ("test/hls/gear1/prog_index.m3u8", streams[0]->get_url());
  EXPECT_EQ("test/hls/gear2/prog_index.m3u8", streams[1]->get_url());
  EXPECT_EQ("test/hls/gear3/prog_index.m3u8", streams[2]->get_url());
  EXPECT_EQ("test/hls/gear4/prog_index.m3u8", streams[3]->get_url());
}

}
