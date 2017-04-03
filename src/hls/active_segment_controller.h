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
#include "../demuxer/demux.h"
#include "../demux_container.h"
#include "../downloader/downloader.h"

static const int NUM_RELOAD_TRIES = 10;


class ActiveSegmentController {
public:
  ActiveSegmentController(Downloader *downloader);
  ~ActiveSegmentController();
  void set_media_playlist(hls::MediaPlaylist media_playlist, hls::Segment active_segment);
  void set_media_playlist(hls::MediaPlaylist media_playlist);
  DemuxContainer get_next_segment();
  void set_current_segment(hls::Segment segment);
  hls::Segment get_current_segment();
  hls::MediaPlaylist get_current_playlist() { return media_playlist; };
  bool is_live() { return media_playlist.live; };
  double get_average_bandwidth() { return downloader->get_average_bandwidth(); };
  double get_current_bandwidth() { return downloader->get_current_bandwidth(); };
  uint32_t get_bandwidth_of_current_playlist() { return media_playlist.bandwidth; };
  double get_percentage_buffer_full() { return demux->get_percentage_buffer_full(); };
private:
  bool has_next_demux_segment();
  bool has_next_download_segment();
  void download_next_segment();
  void demux_next_segment();
  void reload_playlist();
  void print_segment_data();
private:
  // This pointer is managed by the session
  Downloader *downloader;
  int max_segment_data;
  std::unordered_map<std::string, std::string> aes_uri_to_key;

  std::mutex private_data_mutex;
  FRIEND_TEST(ActiveSegmentController, DownloadSegment);
  // Where the download is pointing
  int32_t download_segment_index;
  // Segment we started at, may be empty
  hls::Segment start_segment;
  FRIEND_TEST(ActiveSegmentController, ReloadPlaylist);
  hls::MediaPlaylist media_playlist;

  std::vector<SegmentData> last_downloaded_segments;

  // Download thread
  std::condition_variable download_cv;
  std::mutex download_mutex;
  std::thread download_thread;

  // Demux thread
  std::condition_variable demux_cv;
  std::mutex demux_mutex;
  std::thread demux_thread;

  // Reload playlist thread
  std::condition_variable reload_cv;
  std::mutex reload_mutex;
  std::thread reload_thread;
  std::atomic_bool reload_playlist_flag;

  std::atomic_bool quit_processing;

  std::unique_ptr<Demux> demux;
};
