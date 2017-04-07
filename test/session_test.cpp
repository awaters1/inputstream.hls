/*
 * session_test.cpp Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#include <limits.h>
#include "gtest/gtest.h"
#include "helpers.h"

#include "../src/hls/session.h"
#include "../src/downloader/file_downloader.h"

namespace hls {

class SessionTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    FileMasterPlaylist master_playlist = FileMasterPlaylist();
    master_playlist.open("test/hls/bipbopall_session.m3u8");
    session = new Session(master_playlist, new FileDownloader());
  }

  virtual void TearDown() {
    delete session;
  }

  Session *session;
};

TEST_F(SessionTest, TotalTime) {
  EXPECT_EQ(20, session->get_total_time());
}

TEST_F(SessionTest, GetStreams) {
  session->read_next_pkt();
  EXPECT_EQ(2, session->get_streams().m_streamCount);
}

class EncryptedSessionTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    FileMasterPlaylist master_playlist = FileMasterPlaylist();
    master_playlist.open("test/hls/encrypted.m3u8");
    session = new Session(master_playlist, new FileDownloader());
  }

  virtual void TearDown() {
    delete session;
  }

  Session *session;
};


}
