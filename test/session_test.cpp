/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include "gtest/gtest.h"

#include "../src/hls/session.h"

namespace hls {

class SessionTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    FileMasterPlaylist master_playlist = FileMasterPlaylist();
    master_playlist.open("test/hls/bipbopall.m3u8");
    session = new Session(master_playlist);
  }

  virtual void TearDown() {
    delete session;
  }

  Session *session;
};

TEST_F(SessionTest, CreateSession) {
  std::vector<Stream> streams = session->get_streams();
  EXPECT_EQ(2, streams.size());
  EXPECT_EQ("aac", streams[0].codec_name);
  EXPECT_EQ("h264", streams[1].codec_name);
}

TEST_F(SessionTest, TotalTime) {
  EXPECT_EQ(1801, session->get_total_time());
}

TEST_F(SessionTest, StartCurrentTime) {
  EXPECT_EQ(0, session->get_current_time());
}

TEST_F(SessionTest, CurrentTime) {
  session->get_streams(); // fetches active_segment
  for(int i = 0; i < 100; ++i) {
    session->read_next_pkt();
  }
  // TODO: Not sure what the value should be
  // EXPECT_EQ(10032, session->get_current_time());
}


}
