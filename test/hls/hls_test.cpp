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
}
