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


class ActiveSegmentController {
public:
  ActiveSegmentController(Downloader *downloader, hls::MediaPlaylist &media_playlist);
  ~ActiveSegmentController();
private:
  bool has_next_demux_segment();
  bool has_demux_buffer_room();
  bool has_next_download_segment();
  void download_next_segment();
  void demux_next_segment();
  void reload_playlist();
  void print_segment_data();
private:
  Downloader *downloader;
  hls::MediaPlaylist &media_playlist;


  std::unordered_map<std::string, std::string> aes_uri_to_key;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  // Where the download is pointing
  int32_t download_segment_index;
  // Segment we started at, may be empty
  hls::Segment start_segment;

  std::vector<SegmentData> last_downloaded_segments;

  // Download thread
  std::condition_variable download_cv;
  std::thread download_thread;

  // Demux thread
  std::condition_variable demux_cv;
  std::thread demux_thread;

  // Reload playlist thread
  std::condition_variable reload_cv;
  std::thread reload_thread;
  std::atomic_bool reload_playlist_flag;

  std::atomic_bool quit_processing;
};
