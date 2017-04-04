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

TEST(HlsTest, GetAttributeValueString) {
  MasterPlaylist mp = MasterPlaylist();
  std::string attribute_value;
  attribute_value = mp.get_string_attribute_value("#EXT-X-KEY:METHOD=AES-128,URI=\"https://foliovision.com/?fv_player_hls_key=20_gothic_avenue_live\",IV=0x9f11a1b6a9fe0d800f5c9688370e694d", "URI");
  EXPECT_EQ("https://foliovision.com/?fv_player_hls_key=20_gothic_avenue_live", attribute_value);
}

TEST(HlsTest, GetBaseUrl) {
  hls::FileMasterPlaylist mp = hls::FileMasterPlaylist();
  mp.open("test/hls/bipbopall.m3u8");
  EXPECT_EQ("test/hls/", mp.get_base_url());
}

TEST(HlsTest, StreamUrl) {
  hls::FileMasterPlaylist mp = hls::FileMasterPlaylist();
  mp.open("test/hls/bipbopall.m3u8");
  std::vector<MediaPlaylist> streams = mp.get_media_playlist();
  ASSERT_EQ(4, streams.size());
  EXPECT_EQ("test/hls/gear1/prog_index.m3u8", streams[0].get_url());
  EXPECT_EQ("test/hls/gear2/prog_index.m3u8", streams[1].get_url());
  EXPECT_EQ("test/hls/gear3/prog_index.m3u8", streams[2].get_url());
  EXPECT_EQ("test/hls/gear4/prog_index.m3u8", streams[3].get_url());
}

TEST(HlsTest, LoadMediaPlaylist) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  bool ret = mp.open("test/hls/gear1/prog_index.m3u8");
  EXPECT_TRUE(ret);
  EXPECT_FALSE(mp.live);
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
  attributes = mp.get_attributes("#EXT-X-MEDIA-SEQUENCE:23");
  ASSERT_EQ(1, attributes.size());
  EXPECT_EQ("23", attributes[0]);
  EXPECT_EQ(23, std::stoul(attributes[0]));
}

TEST(HlsTest, MediaPlayistUrl) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  mp.open("test/hls/gear1/prog_index.m3u8");
  EXPECT_EQ("test/hls/gear1/", mp.get_base_url());
}

TEST(HlsTest, SegmentUrl) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  mp.open("test/hls/gear1/prog_index.m3u8");
  ASSERT_EQ(2, mp.get_number_of_segments());
  EXPECT_EQ("test/hls/gear1/fileSequence0.ts", mp.get_segment(0).get_url());
}

TEST(HlsTest, MasterPlaylistLoadsMediaPlaylist) {
  FileMasterPlaylist master_playlist = FileMasterPlaylist();
  master_playlist.open("test/hls/bipbopall.m3u8");
  ASSERT_EQ(4, master_playlist.get_media_playlist().size());
  EXPECT_EQ(2, master_playlist.get_media_playlist()[0].get_number_of_segments());
  EXPECT_EQ(181, master_playlist.get_media_playlist()[1].get_number_of_segments());
  EXPECT_EQ(181, master_playlist.get_media_playlist()[2].get_number_of_segments());
  EXPECT_EQ(181, master_playlist.get_media_playlist()[3].get_number_of_segments());
}

TEST(HlsTest, EncryptedMediaPlaylist) {
  hls::FileMediaPlaylist mp = hls::FileMediaPlaylist();
  mp.open("test/hls/hls_400_.m3u8");
  EXPECT_TRUE(mp.encrypted);
  EXPECT_EQ("test/hls/aes_key", mp.aes_uri);
  EXPECT_EQ("0x9f11a1b6a9fe0d800f5c9688370e694d", mp.aes_iv);
}

TEST(HlsTest, EncryptedSegments) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/hls/hls_400_.m3u8");
  Segment segment = media_playlist.get_segment(0);
  EXPECT_EQ("test/hls/aes_key", segment.aes_uri);
  EXPECT_EQ("0x9f11a1b6a9fe0d800f5c9688370e694d", segment.aes_iv);
  EXPECT_TRUE(segment.encrypted);
}

TEST(HlsTest, ByteRangeSegments) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/hls/byte_range.m3u8");
  EXPECT_EQ(181, media_playlist.get_number_of_segments());
  EXPECT_EQ(326744, media_playlist.get_segment(0).byte_length);
  EXPECT_EQ("test/hls/main.ts", media_playlist.get_segment(0).get_url());
  EXPECT_EQ(0, media_playlist.get_segment(0).byte_offset);
  EXPECT_EQ(139872, media_playlist.get_segment(180).byte_length);
  EXPECT_EQ(59226768, media_playlist.get_segment(180).byte_offset);
  EXPECT_EQ("test/hls/main.ts", media_playlist.get_segment(180).get_url());
}

TEST(HlsTest, LiveMediaPlaylist) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/live/media.m3u8");
  EXPECT_TRUE(media_playlist.live);
}

TEST(HlsTest, HasNextSegmentFirst) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/live/media.m3u8");
  EXPECT_TRUE(media_playlist.has_segment(0));
}

TEST(HlsTest, HasNextSegment) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/live/media.m3u8");
  EXPECT_TRUE(media_playlist.has_segment(2));
  EXPECT_FALSE(media_playlist.has_segment(7));
}

TEST(HlsTest, GetNextSegment) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/live/media.m3u8");
  Segment segment = media_playlist.get_segment(6);
  EXPECT_EQ(segment.media_sequence, 6);
}

TEST(HlsTest, MergeMediaPlaylist) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/live/media.m3u8");
  EXPECT_EQ(7, media_playlist.get_number_of_segments());
  EXPECT_EQ(6, media_playlist.get_segment(6).media_sequence);
  FileMediaPlaylist updated_media_playlist = FileMediaPlaylist();
  updated_media_playlist.open("test/live/updated_media.m3u8");
  media_playlist.merge(updated_media_playlist);
  EXPECT_EQ(11, media_playlist.get_number_of_segments());
  EXPECT_EQ(10, media_playlist.get_segment(10).media_sequence);
}

TEST(HlsTest, GetSegmentIndex) {
  FileMediaPlaylist media_playlist = FileMediaPlaylist();
  media_playlist.open("test/hls/gear1/prog_index.m3u8");
  hls::Segment segment;
  segment.media_sequence = 1;
  int32_t index = media_playlist.get_segment_index(segment);
  EXPECT_EQ(1, index);
  segment.media_sequence = 1000;
  index = media_playlist.get_segment_index(segment);
  EXPECT_EQ(-1, index);
}
}
