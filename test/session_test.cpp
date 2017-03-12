/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include "gtest/gtest.h"

#include "../src/hls/session.h"

namespace hls {
TEST(SessionTest, CreateSession) {
  FileMasterPlaylist master_playlist = FileMasterPlaylist();
  master_playlist.open("test/hls/bipbopall.m3u8");
  Session session = Session(master_playlist);
  std::vector<Stream*> streams = session.get_streams();
  EXPECT_EQ(2, streams.size());
}
}
