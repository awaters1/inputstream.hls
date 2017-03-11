/*
 * hls_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */


#include <limits.h>
#include "gtest/gtest.h"
#include "../../src/hls/HLS.h"

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


