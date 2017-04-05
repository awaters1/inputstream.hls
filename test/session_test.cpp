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
  // EXPECT_EQ(20, session->get_total_time());
}

TEST_F(SessionTest, ReadStreamPart) {
  std::string file_contents = load_file_contents("test/hls/gear1/fileSequence0.ts");
  size_t len = 100;
  uint8_t *buf = new uint8_t[len];
  size_t amount_read = 0; //session->read_stream(buf, len);
  EXPECT_TRUE(memcmp(file_contents.c_str(), buf, len) == 0);
  EXPECT_EQ(len, amount_read);
}

TEST_F(SessionTest, ReadStreamSplit) {
  std::string file_contents1 = load_file_contents("test/hls/gear1/fileSequence0.ts");
  std::string file_contents2 = load_file_contents("test/hls/gear1/fileSequence1.ts");
  size_t len = 500000;
  uint8_t *buf = new uint8_t[len];
  size_t amount_read = 0; //session->read_stream(buf, len);
  EXPECT_TRUE(memcmp(file_contents1.c_str(), buf, file_contents1.length()) == 0);
  size_t left_over = amount_read - file_contents1.length();
  EXPECT_TRUE(memcmp(file_contents2.c_str(), buf + file_contents1.length(), left_over) == 0);
  EXPECT_EQ(len, amount_read);
}

TEST_F(SessionTest, ReadStreamEnd) {
  std::string file_contents1 = load_file_contents("test/hls/gear1/fileSequence0.ts");
  std::string file_contents2 = load_file_contents("test/hls/gear1/fileSequence1.ts");
  size_t len = 10000000;
  uint8_t *buf = new uint8_t[len];
  size_t amount_read = 0; //session->read_stream(buf, len);
  EXPECT_TRUE(memcmp(file_contents1.c_str(), buf, file_contents1.length()) == 0);
  size_t left_over = amount_read - file_contents1.length();
  EXPECT_TRUE(memcmp(file_contents2.c_str(), buf + file_contents1.length(), left_over) == 0);
  EXPECT_EQ(1256780, amount_read);
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
