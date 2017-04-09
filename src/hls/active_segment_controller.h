/*
 * active_segment_queue.h Copyright (C) 2017 Anthony Waters <awaters1@gmail.com>
 */

#pragma once

#include <unordered_map>
#include <future>
#include <thread>
#include <vector>
#include <atomic>

#include "HLS.h"
#include "../downloader/downloader.h"

static const int NUM_RELOAD_TRIES = 10;

struct DataHelper {
  std::string aes_uri;
  std::string aes_iv;
  bool encrypted;
  hls::Segment segment;
};

class Demux;

class ActiveSegmentController {
public:
  ActiveSegmentController(Demux *demux, Downloader *downloader, hls::MediaPlaylist &media_playlist);
  ~ActiveSegmentController();

  // @return true if we expect more data, false if we don't
  // expect to download more data
  bool trigger_download();
private:
  void download_next_segment();
  void process_data(DataHelper &data_helper, std::string data);
  void reload_playlist();
private:
  Downloader *downloader;
  hls::MediaPlaylist &media_playlist;
  Demux *demux;

  // Maps a position in the byte buffer to a media sequence of a segment
  // the first segment is starts at byte 0
  uint64_t current_pos;
  std::unordered_map<uint64_t, uint64_t> pos_to_media_sequence;
  std::unordered_map<std::string, std::string> aes_uri_to_key;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  // Where the download is pointing
  int32_t download_segment_index;
  // Segment we started at, may be empty
  hls::Segment start_segment;


  // Download thread
  std::condition_variable download_cv;
  std::thread download_thread;

  // Reload playlist thread
  std::condition_variable reload_cv;
  std::thread reload_thread;
  std::atomic_bool reload_playlist_flag;

  std::atomic_bool quit_processing;
  std::atomic_bool download_segment;
};
